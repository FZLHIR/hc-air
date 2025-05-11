#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "oled.h"
#include "rgb.h"
#include "Sensor.h"
#include "data.h"
#include "fan.h"
#include "btn.h"
void app_main(void)
{
    OLED_Init();
    OLED_UI();
    state_set_init();
    state_control(SYS_INIT, true);
    Sensor_init();
    fan_init();
    if (1) // 手动切换
        pm25_uart_init();
    RGB_init();
    btn_init();
    while (1)
    {
        data_comp(); // 处理进程
        ESP_LOGD("测试", "打印");
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 1000即延时1s
        static bool i = true;
        if (i)
        {
            state_control(SYS_INIT, false);
            i = false;
        }
    }
}
