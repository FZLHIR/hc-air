#include <stdio.h>
#include "CH2O.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/task.h"

#define UART_PORT_NUM UART_NUM_0
#define UART_BAUD_RATE (9600)
#define UART_TX_GPIO_NUM (GPIO_NUM_16)
#define UART_RX_GPIO_NUM (GPIO_NUM_17)
#define UART_BUF_SIZE (9)
#define UART_TASK_STACK_SIZE 128
#define UART_TASK_PRIORITY 10

static const char *TAG = "CH2O";

static int8_t data[UART_BUF_SIZE + 1] = {0};
// 串口读取任务
static void CH2O_read(void *arg)
{
    while (1)
    {
        // 读取接收到的数据
        int len = uart_read_bytes(UART_PORT_NUM, &data, UART_BUF_SIZE, 1100 / portTICK_PERIOD_MS);
        if (len > 0)
        {
            data[len] = '\0';
            ESP_LOGI(TAG, "接收到数据: %s", data);
        }
        else
            ESP_LOGW(TAG, "接收超时");
    }
}

int8_t CH2O_get_data(void)
{
    return data[2];
}

void CH2O_init(void)
{
    // UART配置结构体
    uart_config_t uart_config = {
        .source_clk = UART_SCLK_APB,
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
    };

    // 安装UART驱动并配置参数
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));

    // 设置UART的GPIO引脚
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_GPIO_NUM, UART_RX_GPIO_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // 创建串口读取任务
    xTaskCreate(CH2O_read, "CH2O_read_task", UART_TASK_STACK_SIZE, NULL, UART_TASK_PRIORITY, NULL);
}
