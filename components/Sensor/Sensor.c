#include <stdio.h>
#include "Sensor.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <math.h>
#include "driver/uart.h"

#define PM25_ADC ADC_CHANNEL_3
#define PM25_PIN GPIO_NUM_1
#define CO_ADC ADC_CHANNEL_2 // adc2
#define CO_PIN GPIO_NUM_14
#define CH2O_ADC ADC_CHANNEL_6 // adc2
#define CH2O_PIN GPIO_NUM_16

static const char *TAG = "传感器";
static int original_data[2][4] = {{0}, {0}};
static int adc_raw[4] = {0};
static int voltage[4] = {0};

static adc_oneshot_unit_handle_t adc1_handle;
static adc_oneshot_unit_handle_t adc2_handle;
static adc_cali_handle_t adc1_cali_PM25_handle = NULL;
static adc_cali_handle_t adc2_cali_CO_handle = NULL;
static adc_cali_handle_t adc2_cali_CH2O_handle = NULL;

void Sensor_init(void)
{
    // ADC1初始化
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, PM25_ADC, &config));

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = PM25_ADC,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_PM25_handle));

    // ADC2初始化
    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

    cali_config.unit_id = ADC_UNIT_2;
    cali_config.chan = CO_ADC;
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc2_cali_CO_handle));
    cali_config.chan = CH2O_ADC;
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc2_cali_CH2O_handle));

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, CO_ADC, &config));   // 示例后置
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, CH2O_ADC, &config)); // 示例后置
}

float ppm(float V)
{
    float Vol = (V * 5 / 4096) * 1.0;
    float RS = (5 - Vol) / (Vol * 0.5);
    float R0 = 6.64;
    float ppm = powf(11.5428 * R0 / RS, 0.6549f);
    return ppm;
}

float PM25_get_data(void)
{
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, PM25_ADC, &adc_raw[0]));
    ESP_LOGD(TAG, "PM25 ADC 原始值:  %d", adc_raw[0]);
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_PM25_handle, adc_raw[0], &voltage[0]));
    ESP_LOGD(TAG, "PM25 电压值: %d mV", voltage[0]);
    float pm25 = (float)voltage[0] * (float).001 * (float).17 - (float).1;
    vTaskDelay(pdMS_TO_TICKS(10));
    return pm25;
}

float CO_get_data(void)
{
    ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, CO_ADC, &adc_raw[1]));
    ESP_LOGD(TAG, "CO ADC 原始值:  %d", adc_raw[1]);
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_CO_handle, adc_raw[1], &voltage[1]));
    ESP_LOGD(TAG, "CO 电压值: %d mV", voltage[1]);

    float co = ppm(voltage[1]);
    vTaskDelay(pdMS_TO_TICKS(10));
    if (co > 999)
        co = 0.1;
    return co;
}

float CH2O_get_data(void)
{
    ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, CH2O_ADC, &adc_raw[2]));
    ESP_LOGD(TAG, "CH2O ADC 原始值:  %d", adc_raw[2]);
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_CH2O_handle, adc_raw[2], &voltage[2]));
    ESP_LOGD(TAG, "CH2O 电压值: %d mV", voltage[2]);
    float ch2o = ppm(voltage[2]);
    vTaskDelay(pdMS_TO_TICKS(10));
    if (ch2o > 999)
        ch2o = 0.1;
    return ch2o;
}

void pm25_uart_init(void)
{
    adc_oneshot_del_unit(adc1_handle);                           // 删除ADC1的PM2.5通道
    adc_cali_delete_scheme_curve_fitting(adc1_cali_PM25_handle); // 删除ADC1的PM2.5校准方案
    // 配置UART0参数
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, UART_PIN_NO_CHANGE, GPIO_NUM_1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 256, 0, 0, NULL, 0));
}
static float last_pm;
float pm25_get_data_uart(void)
{
    uint8_t data[40] = {0};
    uart_read_bytes(UART_NUM_2, data, 32, 0);
    uint16_t pm25_raw = (data[12] << 8) | data[13];
    // ESP_LOGW(TAG, "数据: %d", data);
    ESP_LOGW(TAG, "数据: %d", pm25_raw);
    if (pm25_raw < 500 || data[0] == 0x42)
    {
        
        last_pm = pm25_raw;
    }
    return (float)last_pm;
}

int (*get_original_data(void))[4]
{
    original_data[0][0] = adc_raw[0];
    original_data[0][1] = adc_raw[1];
    original_data[0][2] = adc_raw[2];
    original_data[0][3] = adc_raw[3];
    original_data[1][0] = voltage[0];
    original_data[1][1] = voltage[1];
    original_data[1][2] = voltage[2];
    original_data[1][3] = voltage[3];
    return original_data;
}