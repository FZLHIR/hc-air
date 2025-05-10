#ifndef DATA_H
#define DATA_H
#include "stdbool.h"
typedef struct
{
    float co;        // 一氧化碳浓度(ppm)
    float ch2o;      // 甲醛浓度(ppm)
    float pm25;      // PM2.5浓度(ppm)
    int temperature; // 温度(℃)
    int humidity;    // 湿度(%RH)
    int fan_mode;    // 风扇模式挡位
} EnvironmentalData;

typedef enum
{
    SYS_INIT,            // 系统初始化中
    WIFI_CONNECTING,     // WLAN连接中
    WIFI_CONNECTED,      // WLAN已连接
    WIFI_DISCONNECTED,   // WLAN断开
    AUTO_MODE,           // 自动模式
    MANUAL_MODE,         // 手动模式
    REMOTE_CONTROL_MODE, // 远程控制
    SENSOR_CO_FAULT,     // CO传感器异常
    SENSOR_CH2O_FAULT,   // 甲醛传感器异常
    SENSOR_PM25_FAULT,   // PM2.5传感器异常
    SENSOR_DHT_FAULT,    // 温湿度传感器异常
    NONE,                // 无状态
} SystemStatus;

void data_comp(void);
void state_control(SystemStatus state, bool onf);
void state_set_init(void);
void auto_control(void);
bool fan_control(int fan_mode);
#endif /* DATA_H */