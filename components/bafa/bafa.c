#include <stdio.h>
#include "bafa.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "esp_wifi.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "data.h"

#pragma region
#define STA_START BIT0                         // sta模式启动完成标志位
#define SCAN_DONE BIT1                         // 扫描完成标志位
#define STA_CONNECTED BIT2                     // sta连接成功标志位;
#define Retry_BIT BIT3                         // 重试标志位
#define CONNECTED BIT4                         // 连接成功标志位
#define WIFI_SSID "HUAWEI-fengtaiyu"           // wifi名称
#define WIFI_PASS "134679852"                  // wifi密码
#define WIFI_MAX_RETRY 5                       // wifi最大重试次数
#define HOST_IP_ADDR "119.91.109.180"          // 主机地址
#define PORT 8344                              // 端口号
#define uid "49140038db22471d9754978f76968eb2" // 设备唯一标识符
#define topic "hcair005"                       // 主题名称
#pragma endregion
//----------------------------------------------------------------------------------------------函数声明
#pragma region
QueueHandle_t Time;                         // 心跳倒计时队列
static EventGroupHandle_t Wlan_event_group; // Wlan事件组

static const char *Subscribe = "cmd=3&uid=49140038db22471d9754978f76968eb2&topic=HCair\r\n"; // 订阅主题
int SW = 0;

//*定时发送
void TimerCallback(void *socket) // 定时器回调函数
{
    int sock = *(int *)socket;
    char *data = get_data_upload();
    char msg[256] = {0};
    snprintf(msg, sizeof(msg), "cmd=2&uid=49140038db22471d9754978f76968eb2&topic=HCair&msg=%s\r\n", data);
    int err = send(sock, msg, strlen(msg), 0); // 发送检查命令/心跳
    ESP_LOGI("服务器", "发送数据");
    if (err < 0)
        ESP_LOGE("服务器", "发送失败:错误号 %d", errno);
}

void data_transmit(void *socket) // 创建一个1秒的定时器
{
    esp_timer_create_args_t time_transmit_args = {
        .callback = &TimerCallback,
        .arg = socket,
        .name = "data_timer"};
    esp_timer_handle_t data_timer;
    ESP_ERROR_CHECK(esp_timer_create(&time_transmit_args, &data_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(data_timer, 1000 * 1000));
}

//*WLAN任务
void Wlan_task(void *arg) // Wifi连接任务
{
    ESP_LOGI("任务", "Wlan Task 创建完成");
    xEventGroupWaitBits(Wlan_event_group,
                        STA_START,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);
    ESP_LOGE("任务", "接收到了STA START事件, 可以运行AP Scan了");
    wifi_country_t wifi_country_config = {
        .cc = "CN",
        .schan = 1,
        .nchan = 13,
    };
    ESP_ERROR_CHECK(esp_wifi_set_country(&wifi_country_config));
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, false));

    xEventGroupWaitBits(Wlan_event_group,
                        SCAN_DONE,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);
    ESP_LOGE("任务", "接收到了SCAN DONE事件, 可以打印出AP扫描结果了");
    uint16_t ap_num = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));
    ESP_LOGI("WIFI", "扫描到AP数量 : %d", ap_num);

    uint16_t max_aps = 10;
    wifi_ap_record_t ap_records[max_aps];
    memset(ap_records, 0, sizeof(ap_records));

    uint16_t aps_count = max_aps;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&aps_count, ap_records));

    ESP_LOGI("WIFI", "记录了%d个AP信息如下:", aps_count);

    printf("%30s %s %s %s\n", "SSID", "频道", "强度", "MAC地址");

    for (int i = 0; i < aps_count; i++)
    {
        printf("%30s  %3d  %3d  %02X-%02X-%02X-%02X-%02X-%02X\n", ap_records[i].ssid, ap_records[i].primary, ap_records[i].rssi, ap_records[i].bssid[0], ap_records[i].bssid[1], ap_records[i].bssid[2], ap_records[i].bssid[3], ap_records[i].bssid[4], ap_records[i].bssid[5]);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI("WIFI", "开始连接");
    esp_wifi_connect();
    xEventGroupWaitBits(Wlan_event_group,
                        STA_CONNECTED,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);
    ESP_LOGI("WIFI", "STA连接成功");

    xEventGroupWaitBits(Wlan_event_group,
                        CONNECTED,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);
    ESP_LOGI("WIFI", "成功连接到%s", WIFI_SSID);

    vTaskDelete(NULL);
}

void Wlan_event(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data) // Wlan事件处理
{
    ESP_LOGE("事件", "BASE:%s, ID:%ld", base, (long)id); // 打印事件信息
    static int8_t Try = 0;
    switch (id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGE("事件", "WIFI_EVENT_STA_START\nSTA模式启动"); // 启动STA模式完成
        xEventGroupSetBits(Wlan_event_group, STA_START);
        break;
    case WIFI_EVENT_SCAN_DONE:
        ESP_LOGE("事件", "WIFI_EVENT_SCAN_DONE\n扫描完成"); // 扫描完成
        xEventGroupSetBits(Wlan_event_group, SCAN_DONE);
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGE("事件", "WIFI_EVENT_STA_CONNECTED\nSTA已连接"); // 连接成功
        xEventGroupSetBits(Wlan_event_group, STA_CONNECTED);
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGE("事件", "WIFI_EVENT_STA_DISCONNECTED\nSTA断开连接"); // 断开连接
        if (Try < WIFI_MAX_RETRY)
        {
            esp_wifi_connect();
            Try++;
            ESP_LOGI("重连", "正在重试，第%d次连接", Try);
        }
        else
        {
            xEventGroupSetBits(Wlan_event_group, Retry_BIT);
            ESP_LOGI("重连", "尝试连接失败！");
        }
        break;

    case IP_EVENT_STA_GOT_IP:
        ESP_LOGE("事件", "IP_EVENT_STA_GOT_IP\nSTA获取IP"); // 获取IP
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI("IP", "获取ip:" IPSTR, IP2STR(&event->ip_info.ip));
        Try = 0;
        xEventGroupSetBits(Wlan_event_group, CONNECTED);
        break;
    default:
        break;
    }
}

void Wlan_Init(void) // WLAN初始化
{
    ESP_LOGI("WIFI", "0. 初始化NVS存储");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI("WIFI", "1.1 Wi-Fi 初始化阶段");
    ESP_ERROR_CHECK(esp_netif_init()); // 初始化网络接口
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t Wlan_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&Wlan_config));

    ESP_LOGI("WIFI", "1.2创建App Task 和 FreeRTOS Event Group");
    Wlan_event_group = xEventGroupCreate();
    xTaskCreate(Wlan_task, "Wlan Task", 1024 * 4, NULL, 1, NULL);
    esp_event_handler_instance_t any_id;
    esp_event_handler_instance_t got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &Wlan_event,
                                                        NULL,
                                                        &any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &Wlan_event,
                                                        NULL,
                                                        &got_ip));

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI("WIFI", "2. Wi-Fi 初始化阶段");
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI("WIFI", "3. Wi-Fi 启动阶段");
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI", "WIFI初始化完成");
    vTaskDelay(20000 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, any_id));
    vEventGroupDelete(Wlan_event_group);

    ESP_LOGE("WIFI", "WLAN任务结束,已注销事件处理");
}

//*TCP客户端任务
void tcp_client_task(void *pvParameters)
{

    char rx_buffer[256];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;
    u32_t num = 0;
    while (1)
    {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(host_ip);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        //*初始化Socket
        int retry_count = 0;
        int sock = -1;
        while (retry_count < 5)
        {
            sock = socket(addr_family, SOCK_STREAM, ip_protocol);
            if (sock < 0)
            {
                ESP_LOGE("socket", "未能创建Socket:错误号 %d, 重试次数 %d", errno, retry_count);
                retry_count++;
                if (retry_count >= 5)
                    ESP_LOGE("socket", "达到最大重试次数，退出循环");
            }
            else
            {
                ESP_LOGI("socket", "Socket成功创建,正在连接到%s:%d", host_ip, PORT);
                break;
            }
        }
        if (retry_count >= 5)
            break;

        //*连接到服务器
        retry_count = 0;
        while (retry_count < 5)
        {
            int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in));
            if (err != 0)
            {
                ESP_LOGE("socket", "Socket 连接失败: 错误号 %d", errno);
                retry_count++;
                if (retry_count >= 5)
                    ESP_LOGE("socket", "达到最大重试次数，退出循环");
            }
            else
            {
                ESP_LOGI("socket", "Socket 连接成功");
                break;
            }
        }
        if (retry_count >= 5)
            break;

        //*发送信息到服务器
        ESP_LOGI("socket", "发送订阅并写入缺省值");
        retry_count = 0;
        while (retry_count < 5)
        {
            int err = send(sock, Subscribe, strlen(Subscribe), 0); // 发送订阅命令
            recv(sock, rx_buffer, sizeof(rx_buffer), 0);           // 清除订阅响应
            if (err < 0)
            {
                ESP_LOGE("socket", "发送失败:错误号 %d", errno);
                retry_count++;
                if (retry_count >= 5)
                    ESP_LOGE("socket", "达到最大重试次数，退出循环");
            }
            else
            {
                ESP_LOGI("socket", "订阅信息发送成功");
                break;
            }
        }
        if (retry_count >= 5)
            break;

        //*定时发送信息
        ESP_LOGI("服务器", "开始通信");
        data_transmit(&sock);
        ESP_LOGI("服务器", "启动定时数据传送");
        //*接收信息(阻塞式)
        while (1)
        {
            ESP_LOGE("socket", "\n\n\n");
            static int retry = 0;
            ESP_LOGW("socket", "接收第 %ld 次数据", num++);
            int len = recv(sock, rx_buffer, sizeof(rx_buffer), 0); // 接收数据
            if (len < 0)
            {
                ESP_LOGE("socket", "接收失败:错误号 %d", errno);
                retry++;
                if (retry >= 5)
                {
                    ESP_LOGE("socket", "5次接收失败！！");
                    break;
                }
            }
            else if (len >= 62)
            {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI("socket", "收到数据长度 %d, 来自 %s", len, HOST_IP_ADDR);
                ESP_LOGD("socket", "%s", rx_buffer);
                char *data = rx_buffer + 59;
                ESP_LOGW("服务器", "%s", data);
                ESP_LOGW("服务器", "%c", data[0]);
                if (data[0] >= '0' && data[0] <= '9')
                {
                    int value = data[0] - '0'; // 将字符转换为整数
                    ESP_LOGI("解算", "转换后的整数值: %d", value);
                    if (value >= 1 && value <= 3)
                    {
                        fan_control(value - 1);
                        fan_control(4);
                    }
                    else if (value == 4)
                        fan_control(3); // 自动模式
                }
                else
                {
                    ESP_LOGE("解算", "无效的字符: %c", data[0]);
                }
            }
            else
            {
                ESP_LOGW("socket", "意料外的数据长度 %d", len);
                ESP_LOGW("socket", "%s", rx_buffer);
            }
        }

        if (sock != -1)
        {
            ESP_LOGE("socket", "接收问题，正在关闭socket...");
            shutdown(sock, 0);
            close(sock);
            ESP_LOGE("socket", "10s后重启");
            vTaskDelay(10000 / portTICK_PERIOD_MS);
        }
    }
}
