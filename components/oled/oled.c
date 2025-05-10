#include <stdio.h>
#include "oled.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "driver/i2c_master.h"
#include "codetab.h"
#include "data.h"

#define I2C_HOST I2C_NUM_0
#define SDA_pin GPIO_NUM_42
#define SCL_pin GPIO_NUM_41

static i2c_master_dev_handle_t oled_handle;

//*初始化
i2c_master_bus_handle_t
i2c_init(void) // I2C总线初始化
{
    ESP_LOGI("oled", "初始化 I2C 总线"); // 打印日志 在串口
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_HOST,
        .sda_io_num = SDA_pin,
        .scl_io_num = SCL_pin,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));
    return i2c_bus;
}

void OLED_ADD_BUS(i2c_master_bus_handle_t i2c_bus) // 添加OLED设备到I2C总线
{
    i2c_device_config_t oled_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x3C,
        .scl_speed_hz = 400 * 1000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &oled_cfg, &oled_handle));
}

void OLED_Init()
{
    OLED_ADD_BUS(i2c_init());
    vTaskDelay(100 / portTICK_PERIOD_MS);
    uint8_t oled_buffer[28] = {
        0xAE, 0x20, 0x10, 0xb0, 0xc8, 0x00, 0x10,
        0x40, 0x81, 0xff, 0xa1, 0xa6, 0xa8, 0x3F,
        0xa4, 0xd3, 0x00, 0xd5, 0xf0, 0xd9, 0x22,
        0xda, 0x12, 0xdb, 0x20, 0x8d, 0x14, 0xaf};
    i2c_master_transmit_multi_buffer_info_t oled_transmit = {.write_buffer = oled_buffer, .buffer_size = 28};
    i2c_master_multi_buffer_transmit(oled_handle, &oled_transmit, 1, -1);
}

//*执行
void WriteCmd(uint8_t cmd) // 写命令
{
    uint8_t cmd_buf[2] = {0x00, cmd};
    i2c_master_transmit(oled_handle, cmd_buf, 2, -1);
}

void WriteDat(uint8_t dat) // 写数据
{
    uint8_t dat_buf[2] = {0x40, dat};
    i2c_master_transmit(oled_handle, dat_buf, 2, -1);
}

//*功能
void OLED_SetPos(uint8_t x, uint8_t y) // 设置起始点坐标
{
    WriteCmd(0xb0 + y);
    WriteCmd(((x & 0xf0) >> 4) | 0x10);
    WriteCmd((x & 0x0f) | 0x01);
}

void OLED_Fill(uint8_t fill_Data) // 全屏填充
{
    uint8_t m, n;
    for (m = 0; m < 8; m++)
    {
        WriteCmd(0xb0 + m); // page0-page1
        WriteCmd(0x00);     // low column start address
        WriteCmd(0x10);     // high column start address
        for (n = 0; n < 128; n++)
        {
            WriteDat(fill_Data);
        }
    }
}

//*接口
void OLED_CLS(void) // 清屏
{
    OLED_Fill(0x00);
}
//? Y 0~7行，X 0~127列（左右空1px共21列） 偏移
void OLED_ShowStr(uint8_t x, uint8_t y, char ch[], uint8_t TextSize)
{
    uint8_t c = 0, i = 0, j = 0;
    switch (TextSize)
    {
    case 1:
    {
        while (ch[j] != '\0')
        {
            c = ch[j] - 32;
            if (x > 126)
            {
                x = 0;
                y++;
            }
            OLED_SetPos(x, y);
            for (i = 0; i < 6; i++)
                WriteDat(F6x8[c][i]);
            x += 6;
            j++;
        }
    }
    break;
    case 2:
    {
        while (ch[j] != '\0')
        {
            c = ch[j] - 32;
            if (x > 120)
            {
                x = 0;
                y++;
            }
            OLED_SetPos(x, y);
            for (i = 0; i < 8; i++)
                WriteDat(F8X16[c * 16 + i]);
            OLED_SetPos(x, y + 1);
            for (i = 0; i < 8; i++)
                WriteDat(F8X16[c * 16 + i + 8]);
            x += 8;
            j++;
        }
    }
    break;
    }
}

//*外部调用
void OLED_UI(void)
{
    OLED_CLS();
    // 静态显示
    OLED_ShowStr(1, 0, "CO:", 1);
    OLED_ShowStr(1, 1, "CH2O:", 1);
    OLED_ShowStr(1, 2, "PM2.5:", 1);
    OLED_ShowStr(1, 3, "temperature:", 1);
    OLED_ShowStr(1, 4, "humidity:", 1);
    OLED_ShowStr(1, 6, "FAN:", 1);
    // 动态显示
    OLED_ShowStr(91, 0, "___._", 1);
    OLED_ShowStr(91, 1, "___._", 1);
    OLED_ShowStr(91, 2, "___._", 1);
    OLED_ShowStr(91, 3, "--", 1);
    OLED_ShowStr(91, 4, "--", 1);
    OLED_ShowStr(91, 6, "N/A", 1);
    // 状态栏
    OLED_ShowStr(37, 7, "AUTO MODE", 1);
}

void OLED_refresh(EnvironmentalData Sensor_data)
{
    // 转换结构体变量为字符串
    char buf_co[6], buf_ch2o[6], buf_pm25[6];
    char buf_temp[3], buf_humi[3], buf_fan[5];

    // 浮点数保留1位小数
    snprintf(buf_co, sizeof(buf_co), "%.1f", Sensor_data.co);
    snprintf(buf_ch2o, sizeof(buf_ch2o), "%.1f", Sensor_data.ch2o);
    snprintf(buf_pm25, sizeof(buf_pm25), "%.1f", Sensor_data.pm25);

    // 整数转换
    snprintf(buf_temp, sizeof(buf_temp), "%d", Sensor_data.temperature);
    snprintf(buf_humi, sizeof(buf_humi), "%d", Sensor_data.humidity);

    // 风扇模式转换
    const char *fan_modes[] = {"Idle", "Slow", "Fast"};
    snprintf(buf_fan, sizeof(buf_fan), "%s", fan_modes[Sensor_data.fan_mode]);

    // 动态显示
    OLED_ShowStr(91, 0, buf_co, 1);
    OLED_ShowStr(91, 1, buf_ch2o, 1);
    OLED_ShowStr(91, 2, buf_pm25, 1);
    OLED_ShowStr(91, 3, buf_temp, 1);
    OLED_ShowStr(91, 4, buf_humi, 1);
    OLED_ShowStr(91, 6, buf_fan, 1);
}

void OLED_refresh_sp(void)
{
    OLED_ShowStr(91, 0, "      ", 1);
    OLED_ShowStr(91, 1, "      ", 1);
    OLED_ShowStr(91, 2, "      ", 1);
    OLED_ShowStr(91, 3, "  ", 1);
    OLED_ShowStr(91, 4, "  ", 1);
}

void status_update(SystemStatus status)
{
    switch (status)
    {
    case 0:
        OLED_ShowStr(40, 7, "SYS_init", 1);
        break;
    case 1:
        OLED_ShowStr(26, 7, "WLAN_connect", 1);
        break;
    case 2:
        OLED_ShowStr(19, 7, "connect_success", 1);
        break;
    case 3:
        OLED_ShowStr(34, 7, "WLAN_break", 1);
        break;
    case 4:
        OLED_ShowStr(37, 7, "AUTO_MODE", 1);
        break;
    case 5:
        OLED_ShowStr(31, 7, "MANUAL_MODE", 1);
        break;
    case 6:
        OLED_ShowStr(7, 7, "REMOTE_CONTROL_MODE", 1);
        break;
    case 7:
        OLED_ShowStr(19, 7, "SENSOR_CO_FAULT", 1);
        break;
    case 8:
        OLED_ShowStr(13, 7, "SENSOR_CH2O_FAULT", 1);
        break;
    case 9:
        OLED_ShowStr(10, 7, "SENSOR_PM2_5_FAULT", 1);
        break;
    case 10:
        OLED_ShowStr(16, 7, "SENSOR_DHT_FAULT", 1);
        break;
    case 11:
        OLED_ShowStr(0, 7, "                     ", 1);
        break;
    default:
        break;
    };
}
