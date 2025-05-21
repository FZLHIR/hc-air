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
static char data_upload[64];

void auto_control(void); // 定义在调用后方的函数要预声明
void rgb_update(void);
void state_control(SystemStatus state, bool onf);
void error_check(void);

void data_comp(void)
{
    env_data.co = CO_get_data();
    env_data.ch2o = CH2O_get_data();
    if (0) // 手动切换
        env_data.pm25 = PM25_get_data();
    else
        env_data.pm25 = pm25_get_data_uart();

    dht11_read_data(&env_data.humidity, &env_data.temperature);
    env_data.fan_mode = fan_get_mode();
    ESP_LOGD("数据处理", "PM2.5:%.2f ug/m3\nCO:%.2f ppm\nCH2O:%.2f ppm\nTemperature:%d C\nHumidity:%d %%RH\nFan mode:%d", env_data.pm25, env_data.co, env_data.ch2o, env_data.temperature, env_data.humidity, env_data.fan_mode);
    static bool i = true;
    if (i)
    {
        rgb_update();
        OLED_refresh_sp();
        i = false;
    }
    OLED_refresh(env_data);
    error_check();
    if (auto_select)
        auto_control();
    static bool last_auto_select = true;
    static bool last_health = true;
    ESP_LOGI("自动控制", "自动模式:%d 健康状态:%d", auto_select, health);
    if (auto_select != last_auto_select || health != last_health)
    {
        last_auto_select = auto_select;
        last_health = health;
        rgb_update();
    }
}

void state_control(SystemStatus state, bool onf)//枚举量写1 0的状态到十六位bit
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
    // 状态初始化
    esp_timer_create_args_t state_timer_args = {// 100ms定时
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
    state_control(AUTO_MODE, true);
    int count = 3;
    if (env_data.pm25 > 200) // 修改阈值
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
    else
        health = true;
}

bool fan_control(int fan_mode)
{
    if (fan_mode < 0 || fan_mode > 4)
        return false;
    if (fan_mode == 4)
    {
        state_control(MANUAL_MODE, false);
        state_control(REMOTE_CONTROL_MODE, true);
        return true;
    }

    switch (fan_mode)
    {
    case 3:
        auto_select = true;
        state_control(MANUAL_MODE, false);
        auto_control();
        return true;
    case 0:
        fan_set_mode(0);
        break;
    case 1:
        fan_set_mode(1);
        break;
    case 2:
        fan_set_mode(2);
        break;
    default:
        break;
    }
    auto_select = false;
    state_control(AUTO_MODE, false);
    state_control(REMOTE_CONTROL_MODE, false);
    state_control(MANUAL_MODE, true);
    return true;
}

void error_check(void)
{
    if (env_data.temperature < 5 || env_data.humidity < 2)
        state_control(SENSOR_DHT_FAULT, true);
    else
        state_control(SENSOR_DHT_FAULT, false);
    if (env_data.pm25 == 0 || env_data.pm25 == 0.1)
        state_control(SENSOR_PM25_FAULT, true);
    else
        state_control(SENSOR_PM25_FAULT, false);
    if (env_data.co < 15.6 || env_data.co > 15.3 || env_data.co == 0.1)
        state_control(SENSOR_CO_FAULT, true);
    else
        state_control(SENSOR_CO_FAULT, false);
    if (env_data.ch2o < 4.6 || env_data.ch2o > 3.3 || env_data.ch2o == 0.1)
        state_control(SENSOR_CH2O_FAULT, true);
    else
        state_control(SENSOR_CH2O_FAULT, false);
}

char *get_data_upload(void)
{
    char active_states_binary[5];

    for (int i = 10; i > 6; i--)
    {
        active_states_binary[10 - i] = (active_states & (1 << i)) ? '1' : '0';
    }
    active_states_binary[4] = '\0'; // 添加字符串结束符

    snprintf(data_upload, sizeof(data_upload), "%d#%.2f#%.2f#%.2f#%d#%d#%s",
             auto_select ? 4 : fan_get_mode() + 1, env_data.pm25 / 2, env_data.co / 10, env_data.ch2o / 20, env_data.temperature / 10, env_data.humidity / 10, active_states_binary);
    return data_upload;
}