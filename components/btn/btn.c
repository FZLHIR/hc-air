#include <stdio.h>
#include "btn.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "button_gpio.h"
#include "iot_button.h"
#include "data.h"

#define BUTTON1_GPIO_NUM GPIO_NUM_9
#define BUTTON2_GPIO_NUM GPIO_NUM_10
#define BUTTON3_GPIO_NUM GPIO_NUM_11
#define BUTTON4_GPIO_NUM GPIO_NUM_12

static const char *TAG = "按键";

static void btn1_cb(void *button_handle, void *usr_data)
{
    static uint8_t cycle_param = 0;
    bool ret = fan_control(cycle_param);
    if (!ret)
        ESP_LOGE(TAG, "切换失败，切换值不允许");
    ESP_LOGD(TAG, "%d被点击", (int)button_handle);
    cycle_param = (cycle_param + 1) % 4;
}

void btn_init(void)
{
    // 创建按钮句柄
    button_handle_t btn1_handle = NULL;
    button_handle_t btn2_handle = NULL;
    button_handle_t btn3_handle = NULL;
    button_handle_t btn4_handle = NULL;

    // 按钮配置
    button_config_t btn_cfg = {
        .long_press_time = 3000, // 长按时间阈值，单位为毫秒
        .short_press_time = 200, // 短按时间阈值，单位为毫秒
    };

    // 按钮GPIO配置
    button_gpio_config_t gpio_cfg = {
        .gpio_num = BUTTON1_GPIO_NUM,
        .active_level = 0,
    };

    ESP_ERROR_CHECK(iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn1_handle));
    gpio_cfg.gpio_num = BUTTON2_GPIO_NUM;
    ESP_ERROR_CHECK(iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn2_handle));
    gpio_cfg.gpio_num = BUTTON3_GPIO_NUM;
    ESP_ERROR_CHECK(iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn3_handle));
    gpio_cfg.gpio_num = BUTTON4_GPIO_NUM;
    ESP_ERROR_CHECK(iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn4_handle));

    iot_button_register_cb(btn1_handle, BUTTON_SINGLE_CLICK, NULL, btn1_cb, NULL);
    iot_button_register_cb(btn2_handle, BUTTON_SINGLE_CLICK, NULL, btn1_cb, NULL);
    iot_button_register_cb(btn3_handle, BUTTON_SINGLE_CLICK, NULL, btn1_cb, NULL);
    iot_button_register_cb(btn4_handle, BUTTON_SINGLE_CLICK, NULL, btn1_cb, NULL);
    ESP_LOGI(TAG, "Button initialized.");
}
