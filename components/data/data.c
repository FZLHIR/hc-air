#include <stdio.h>
#include "data.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "Sensor.h"
#include "dht.h"
#include "fan.h"
#include "oled.h"
#include "rgb.h"
#include "esp_timer.h"

static EnvironmentalData env_data = {0};
static bool auto_select = true;
static bool health = true;
static uint16_t active_states = 0;
void data_comp(void)
{
    env_data.co = CO_get_data();
    env_data.ch2o = CH2O_get_data();
    env_data.pm25 = PM25_get_data();
    dht11_read_data(&env_data.humidity, &env_data.temperature);
    env_data.fan_mode = fan_get_mode();
    ESP_LOGD("数据处理", "PM2.5:%.2f ug/m3\nCO:%.2f ppm\nCH2O:%.2f ppm\nTemperature:%d C\nHumidity:%d %%RH\nFan mode:%d", env_data.pm25, env_data.co, env_data.ch2o, env_data.temperature, env_data.humidity, env_data.fan_mode);
    static bool i = true;
    if (i)
    {
        OLED_refresh_sp();
        i = false;
    }
    OLED_refresh(env_data);
    if (auto_select)
        auto_control();
}

void state_control(SystemStatus state, bool onf)
{
    uint16_t bit = 1 << state;
    if (onf)
        active_states |= bit; // 置位操作
    else
        active_states &= ~bit; // 清零操作
    ESP_LOGD("状态控制", "设置状态:%d", state);
}

void state_update(void *n)
{

    static uint8_t timer = 0;
    static bool MU = true;
    static uint8_t idx = 0;
    static uint8_t last_idx = 0xFF;  // 上次索引
    static uint16_t last_states = 0; // 上次状态码

    uint8_t valid_states[11];
    uint8_t valid_count = 0;

    // 位掩码遍历
    for (int i = 0; i <= 10; i++)
        if (active_states & (1 << i))
            valid_states[valid_count++] = i;

    timer += 1; // 计时递增

    // 状态变化检测
    bool state_changed = (active_states != last_states) || (valid_count > 1 && idx != last_idx);
    last_states = active_states;
    last_idx = idx;

    if (valid_count == 1) // 单状态持续显示
    {
        if (state_changed) // 状态变化
            status_update(valid_states[0]);
        timer = 0;
        return;
    }

    if (MU) // 多状态循环逻辑
    {
        if (timer >= 10) // 结束1s显示
        {
            MU = false;
            timer = 0;
            status_update(11);
        }
        else // 保持显示
            if (state_changed)
                status_update(valid_states[idx]);
    }
    else
    {
        if (timer >= 5) // 结束0.5s间隔
        {
            MU = true;
            timer = 0;
            idx = (idx + 1) % valid_count; // 循环显示
            status_update(valid_states[idx]);
        }
        else // 保持间隔
            if (state_changed)
                status_update(11);
    }
}

void state_set_init(void)
{
    // 初始化状态
    esp_timer_create_args_t state_timer_args = {// 100ms定时器
                                                .callback = &state_update,
                                                .name = "state_timer"};
    esp_timer_handle_t state_timer;
    ESP_ERROR_CHECK(esp_timer_create(&state_timer_args, &state_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(state_timer, 100 * 1000));
}

void rgb_update(void)
{
    if (auto_select)
        set_RGB_colors(1);
    else if (!health)
        set_RGB_colors(2);
    else
        set_RGB_colors(0);
}

void auto_control(void)
{
    state_control(4, true);
    static int count = 3;
    if (env_data.pm25 > 200)
        count++;
    else
        count--;
    if (env_data.ch2o > 1000)
        count++;
    else
        count--;
    if (env_data.co > 1000)
        count++;
    else
        count--;
    switch (count)
    {
    case 0:
        fan_set_mode(0);
        break;
    case 2:
        fan_set_mode(1);
        break;
    case 4:
        fan_set_mode(2);
        break;
    case 6:
        fan_set_mode(2);
        break;
    default:
        fan_set_mode(1);
        break;
    }
    if (count)
        health = false;
    rgb_update();
}

bool fan_control(int fan_mode)
{
    if (fan_mode < 0 || fan_mode > 3)
        return false;
    switch (fan_mode)
    {
    case 0:
        auto_select = true;
        auto_control();
        break;
    case 1:
        fan_set_mode(0); /* fallthrough */
    case 2:
        fan_set_mode(1); /* fallthrough */
    case 3:
        fan_set_mode(2); /* fallthrough */
    default:
        auto_select = false;
        break;
    }
    return true;
}
