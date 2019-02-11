/* BSD Socket API Example

    This is program of CH5 Electric&Power System
Function:
// 1. RS422 <---> UDP
// 2. 
*/
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <stdio.h>
#include "driver/uart.h"


#define EXAMPLE_ESP_WIFI_SSID      CONFIG_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_WIFI_PASSWORD
#define EXAMPLE_MAX_STA_CONN       CONFIG_MAX_STA_CONN

#define PORT CONFIG_EXAMPLE_PORT

#define	ECHO_TEST_TXD	(GPIO_NUM_10)
#define ECHO_TEST_RXD	(GPIO_NUM_9)
#define	ECHO_TEST_RTS	(UART_PIN_NO_CHANGE)
#define	ECHO_TEST_CTS	(UART_PIN_NO_CHANGE)
#define	BUF_SIZE (1024)


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;
/* FreeRTOS event group to signal when we are connected*/
//static EventGroupHandle_t s_wifi_event_group;

const int IPV4_GOTIP_BIT = BIT0;
const int IPV6_GOTIP_BIT = BIT1;

struct sockaddr_in sourceAddr;
struct sockaddr_in destAddr;
char rx_buffer[128];
char addr_str[128];
int addr_family;
int ip_protocol;
socklen_t socklen;
int sock ;

void socket_connect(void);

static const char *TAG = "CH5-WIFI-PROGRAM";

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    default:
        break;
    }
    return ESP_OK;
}
void wifi_init_softap()
{
    wifi_event_group = xEventGroupCreate();


    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);


    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}


static void rs422_to_udp_task(void *pvParameters)
{
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);

        while (1) {

    	int rs422_len = uart_read_bytes(UART_NUM_1, data, BUF_SIZE, 20 / portTICK_RATE_MS);
	    if(rs422_len > 0){

                	 int err = sendto(sock, data, rs422_len, 0, (struct sockaddr *)&sourceAddr, sizeof(sourceAddr));
                     if (err < 0) {
                    	 ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
                    	 ESP_LOGE(TAG, "Shutting down socket and restarting...");
                         shutdown(sock, 0);
                         close(sock);
                    	 socket_connect();
 //                  	 break;
                	}
		       }
        vTaskDelay(50/ portTICK_RATE_MS);
        }

    vTaskDelete(NULL);
}

static void udp_to_rs422_task(void *pvParameters)
{

        while (1) {

        ESP_LOGI(TAG, "Waiting for data");

	    int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&sourceAddr, &socklen);

	    // Error occured during receiving
	    //
            if ( len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                ESP_LOGE(TAG, "Shutting down socket and restarting...");
                shutdown(sock, 0);
                close(sock);
                socket_connect();
 //               break;
            }
            // Data received
            else {

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "%s", rx_buffer);

                uart_write_bytes(UART_NUM_1, (const char *) rx_buffer, len);
            }

        }
    vTaskDelete(NULL);
}

void init_uart()		// uart init function
{

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);

}

void socket_connect(void)
{
    sourceAddr.sin_addr.s_addr = inet_addr("192.168.4.2");
    sourceAddr.sin_family = AF_INET;
    sourceAddr.sin_port = htons(4001);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(sourceAddr.sin_addr, addr_str, sizeof(addr_str) - 1);
    socklen = sizeof(sourceAddr);

    destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

    sock = socket(addr_family, SOCK_DGRAM, ip_protocol);	//create socket
    if (sock < 0) {							//error detect

        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
 //       break;

    }
    ESP_LOGI(TAG, "Socket created success!");

    int err = bind(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));	//bind the port
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    }
        ESP_LOGI(TAG, "Socket binded success!");
}

void app_main()
{
    ESP_ERROR_CHECK( nvs_flash_init() );	//Init the device
    init_uart();
    wifi_init_softap();

    socket_connect();

    xTaskCreate(rs422_to_udp_task, "rs422_to_udp", 4096, NULL, 5, NULL);
    xTaskCreate(udp_to_rs422_task, "udp_to_rs422", 4096, NULL, 6, NULL);
}
