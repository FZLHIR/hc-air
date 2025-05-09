#include "driver/i2c_master.h"
#include "data.h"
void OLED_Init(void);
void OLED_CLS(void);
void OLED_ShowStr(uint8_t x, uint8_t y, char ch[], uint8_t TextSize);

void OLED_UI(void);
void OLED_refresh(EnvironmentalData Sensor_data);
void OLED_refresh_sp(void);
void status_update(SystemStatus status);