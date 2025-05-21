#include <stdio.h>
#include "fan.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "driver/i2c_master.h"
#include "driver/ledc.h"

#define I2C_CONT 0
#define FAN_PWM_GPIO 2

static i2c_master_dev_handle_t fan_handle = NULL;
static int fan_mode = 0;

void fan_i2c_init(void)
{
    ESP_LOGI("fan", "初始化 I2C 总线"); // 打印日志 在串口
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_7,
        .scl_io_num = GPIO_NUM_6,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    i2c_device_config_t fan_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x20,
        .scl_speed_hz = 100 * 1000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &fan_cfg, &fan_handle));

    uint8_t cmd_buf[2] = {0x13, 0x3E};
    ESP_ERROR_CHECK(i2c_master_transmit(fan_handle, cmd_buf, 2, -1));
    cmd_buf[0] = 0x02;
    cmd_buf[1] = 0x28;
    ESP_ERROR_CHECK(i2c_master_transmit(fan_handle, cmd_buf, 2, -1));
}

void fan_direct_control(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_4_BIT, //! 0~15
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 25000,        // IDF自动计算div_num
        .clk_cfg = LEDC_AUTO_CLK // 自动选择时钟源
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));
    uint32_t duty_max = ledc_find_suitable_duty_resolution(80 * 1000000, 25000);
    ESP_LOGW("fan", "duty_max:%lu", duty_max);

    // 2. 配置通道
    ledc_channel_config_t ch_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = FAN_PWM_GPIO,
        .duty = 7, // 初始占空比50%
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
}

void fan_init(void)
{
    if (I2C_CONT)
        fan_i2c_init();
    else
        fan_direct_control();
}

void fan_set_mode(int mode)
{
    if (I2C_CONT)
    {
        uint8_t cmd_buf[3];
        cmd_buf[0] = 0x40;
        cmd_buf[2] = 0x00;
        switch (mode)
        {
        case 0:
            cmd_buf[1] = 0x00;
            break;
        case 1:
            cmd_buf[1] = 0x7f;
            break;
        case 2:
            cmd_buf[1] = 0xFF;
            break;
        default:
            break;
        }
        ESP_ERROR_CHECK(i2c_master_transmit(fan_handle, cmd_buf, 3, -1));
    }
    else
    {
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, mode * 7));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    }
    ESP_LOGI("fan", "设置风扇模式 %d", mode);
    fan_mode = mode;
}

int fan_get_mode(void)
{
    return fan_mode;
}
