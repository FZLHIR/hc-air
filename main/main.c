#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "oled.h"
#include "rgb.h"
#include "led_strip.h"
void app_main(void)
{
    OLED_Init(OLED_ADD_BUS(i2c_init()));
    OLED_CLS();
    OLED_ShowStr(0, 0, "Hello, world!", 1);

    led_strip_handle_t led_strip = configure_led_strip();

    while (1)
    {
        set_led_colors(led_strip);
        ESP_LOGI("测试", "打印");
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 1000即延时1s
        
    }
}
