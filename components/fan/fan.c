#include <stdio.h>
#include "fan.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "driver/i2c_master.h"

static i2c_master_dev_handle_t fan_handle = NULL;
static int fan_mode = 0;

void fan_init(void)
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

void fan_set_mode(int mode)
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
    ESP_LOGI("fan", "设置风扇模式 %d", mode);
    fan_mode = mode;
}
int fan_get_mode(void)
{
    return fan_mode;
}
