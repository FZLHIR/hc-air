#include "driver/i2c_master.h"

i2c_master_bus_handle_t i2c_init(void);
i2c_master_dev_handle_t OLED_ADD_BUS(i2c_master_bus_handle_t i2c_bus);
void OLED_Init(i2c_master_dev_handle_t oled_handle);
void OLED_CLS(void);
void OLED_ShowStr(uint8_t x, uint8_t y, char ch[], uint8_t TextSize);