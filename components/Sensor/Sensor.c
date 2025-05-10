#include <stdio.h>
#include "Sensor.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <math.h>

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
    ESP_LOGI(TAG, "PM25 ADC 原始值:  %d", adc_raw[0]);
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_PM25_handle, adc_raw[0], &voltage[0]));
    ESP_LOGI(TAG, "PM25 电压值: %d mV", voltage[0]);
    float pm25 = (float)voltage[0] * (float).001 * (float).17 - (float).1;
    vTaskDelay(pdMS_TO_TICKS(10));
    return pm25;
}

float CO_get_data(void)
{
    ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, CO_ADC, &adc_raw[1]));
    ESP_LOGI(TAG, "CO ADC 原始值:  %d", adc_raw[1]);
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_CO_handle, adc_raw[1], &voltage[1]));
    ESP_LOGI(TAG, "CO 电压值: %d mV", voltage[1]);

    float co = ppm(voltage[1]);
    vTaskDelay(pdMS_TO_TICKS(10));
    return co;
}

float CH2O_get_data(void)
{
    ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, CH2O_ADC, &adc_raw[2]));
    ESP_LOGI(TAG, "CH2O ADC 原始值:  %d", adc_raw[2]);
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_CH2O_handle, adc_raw[2], &voltage[2]));
    ESP_LOGI(TAG, "CH2O 电压值: %d mV", voltage[2]);
    float ch2o = ppm(voltage[2]);
    vTaskDelay(pdMS_TO_TICKS(10));
    return ch2o;
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