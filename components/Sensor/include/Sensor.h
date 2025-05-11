void Sensor_init(void);
float PM25_get_data(void);
float CO_get_data(void);
float CH2O_get_data(void);
void pm25_uart_init(void);
float pm25_get_data_uart(void);
int (*get_original_data(void))[4];