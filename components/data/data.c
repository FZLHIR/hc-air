#include <stdio.h>
#include "data.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "Sensor.h"
#include "dht.h"
#include "fan.h"
#include "oled.h"

static EnvironmentalData env_data = {0};

void data_comp(void)
{
    env_data.co = CO_get_data();
    env_data.ch2o = CH2O_get_data();
    env_data.pm25 = PM25_get_data();
    dht11_read_data(&env_data.humidity, &env_data.temperature);
    env_data.fan_mode = fan_get_mode();
    ESP_LOGW("数据处理", "PM2.5:%.2f ug/m3\nCO:%.2f ppm\nCH2O:%.2f ppm\nTemperature:%d C\nHumidity:%d %%RH\nFan mode:%d", env_data.pm25, env_data.co, env_data.ch2o, env_data.temperature, env_data.humidity, env_data.fan_mode);
    OLED_refresh_sp();
    OLED_refresh(env_data);
}

void auto_control(void)
{
    // todo判断
    if (0)
    {
        fan_set_mode(2);
    }
}