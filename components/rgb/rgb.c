#include <stdio.h>
#include "rgb.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "driver/rmt_tx.h"
#include "led_strip.h"

// 定义 RMT 参数
#define LED_STRIP_RMT_RES_HZ 10*1000*1000 // 10MHz 分辨率
#define LED_GPIO_NUM 21               // 数据引脚

static led_strip_handle_t led_strip;

void RGB_init()
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO_NUM,
        .max_leds = 6,                                               // LED 数量
        .led_model = LED_MODEL_WS2812,                               // LED 型号
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // 颜色格式（根据 LED 型号调整）
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .mem_block_symbols = 64,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
}

void set_RGB_colors(int gear) // 设定led模式（绿/白/红）
{
    switch (gear)
    {
    case 1:
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 0, 2, 0));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 1, 0, 2, 0));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 2, 0, 2, 0));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 3, 0, 2, 0));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 4, 0, 2, 0));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 5, 0, 2, 0));
        break;
    case 2:
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 2, 2, 2));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 1, 2, 2, 2));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 2, 2, 2, 2));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 3, 2, 2, 2));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 4, 2, 2, 2));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 5, 2, 2, 2));
        break;
    case 3:
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 2, 0, 0));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 1, 2, 0, 0));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 2, 2, 0, 0));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 3, 2, 0, 0));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 4, 2, 0, 0));
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 5, 2, 0, 0));
        break;
    default:
        ESP_LOGW("RGB", "无效的模式: %d", gear);
        break;
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}