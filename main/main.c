#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "oled.h"
#include "rgb.h"
#include "Sensor.h"
#include "data.h"
#include "fan.h"
void app_main(void)
{
    OLED_Init();
    OLED_UI();
    Sensor_init();
    fan_init();
    while (1)
    {
        data_comp();
        ESP_LOGI("测试", "打印");
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 1000即延时1s
        }
}
