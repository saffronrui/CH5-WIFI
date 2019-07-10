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

#define GPIO_OUTPUT_IO_0     32                         //LED_Indicator
#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_OUTPUT_IO_0)
int led_state   = 0;

//#define RS422_COM   UART_NUM_2        // UART2 CAN WORK Normally By Remap Pin to GPIO_NUM_12 and GPIO_NUM_13
#define RS422_COM   UART_NUM_1

//#define	ECHO_TEST_TXD	(GPIO_NUM_10)
//#define ECHO_TEST_RXD	(GPIO_NUM_9)
//#define	ECHO_TEST_TXD	(GPIO_NUM_2)
//#define ECHO_TEST_RXD	(GPIO_NUM_15)
#define	ECHO_TEST_TXD	(GPIO_NUM_12)
#define ECHO_TEST_RXD	(GPIO_NUM_13)
#define	ECHO_TEST_RTS	(UART_PIN_NO_CHANGE)
#define	ECHO_TEST_CTS	(UART_PIN_NO_CHANGE)
#define	BUF_SIZE (1024)


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;
/* FreeRTOS event group /to signal when we are connected*/
//static EventGroupHandle_t s_wifi_event_group;

const int IPV4_GOTIP_BIT = BIT0;
const int IPV6_GOTIP_BIT = BIT1;

struct sockaddr_in sourceAddr;
socklen_t socklen;
int sock ;
int ip_protocol;
int addr_family;

struct      sockaddr_in Dev1Addr;       //Address for Transprant COM
char        Dev1Addr_str[128];
socklen_t   dev1_socklen;

struct      sockaddr_in Dev2Addr;       //Address for FUEL system
char        Dev2Addr_str[128];
socklen_t   dev2_socklen;

struct      sockaddr_in Dev3Addr;       //Address for GEAR system
char        Dev3Addr_str[128];
socklen_t   dev3_socklen;

struct      sockaddr_in destAddr;       //Address for UDP server
char        rx_buffer[128];
char        addr_str[128];
char        sourceaddr_str[128];

void socket_connect(void);

static const char *TAG = "CH5-WIFI-PROGRAM";

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_AP_STACONNECTED:

        led_state = 1;              //set LED _STATE

        ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:

        led_state = 0;             //set LED_STATE

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

        int rs422_len = uart_read_bytes(RS422_COM, data, BUF_SIZE, 20 / portTICK_RATE_MS);
	    if(rs422_len > 0){

                     int err = sendto(sock, data, rs422_len, 0, (struct sockaddr *)&Dev1Addr, sizeof(Dev1Addr));   // Transprant COM
                     
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

        char        DevAddr_str[128];
        struct      sockaddr_in DevAddr;
        socklen_t   socklen = sizeof(DevAddr);

        while (1) {

        ESP_LOGI(TAG, "Waiting for data");

	    int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&DevAddr, &socklen);

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
                inet_ntoa_r(((struct sockaddr_in *)&DevAddr)->sin_addr.s_addr, DevAddr_str, sizeof(DevAddr_str) - 1);
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, DevAddr_str);
                ESP_LOGI(TAG, "%s", rx_buffer);

                uart_write_bytes(RS422_COM, (const char *) rx_buffer, len);
            }

        vTaskDelay(50/ portTICK_RATE_MS);
       
        }
    vTaskDelete(NULL);
}


static void gpio_task(void* arg)
{
    static int cnt = 0;
    int time_pra = 1;
    
    for(;;) {
            switch(led_state)
            {
                case 0: 
                        time_pra = 1;   break;
                case 1:
                        time_pra = 5;   break;
            }
            printf("led_blink!\n");
            cnt++;
            gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);

            vTaskDelay(200 * time_pra / portTICK_RATE_MS);
    }
}


void init_uart()		// uart init function
{

    uart_config_t uart_config = {
//        .baud_rate = 115200,
        .baud_rate = 19200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_param_config(RS422_COM, &uart_config);
    uart_set_pin(RS422_COM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);
    uart_driver_install(RS422_COM, BUF_SIZE * 2, BUF_SIZE, 0, NULL, 0);
}

void socket_connect(void)
{
    ///*************** Transparent Transform Channel********///
    ///***  For FCC or ElE program ******************///
    Dev1Addr.sin_addr.s_addr = inet_addr("192.168.4.2");
    Dev1Addr.sin_family = AF_INET;
    Dev1Addr.sin_port = htons(4001);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(Dev1Addr.sin_addr, Dev1Addr_str, sizeof(Dev1Addr_str) - 1);
    dev1_socklen = sizeof(Dev1Addr);
    
    ///************* Config IP and port for GEAR SYSTEM **********///
    ///***  Choose data for GEAR software needed //////
    Dev2Addr.sin_addr.s_addr = inet_addr("192.168.4.3");
    Dev2Addr.sin_family = AF_INET;
    Dev2Addr.sin_port = htons(4001);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(Dev2Addr.sin_addr, Dev2Addr_str, sizeof(Dev2Addr_str) - 1);
    dev2_socklen = sizeof(Dev2Addr);

    ///************* Config IP and port for FUEL SYSTEM **********///
    ///***  Choose data for FULE software needed //////
    Dev3Addr.sin_addr.s_addr = inet_addr("192.168.4.4");
    Dev3Addr.sin_family = AF_INET;
    Dev3Addr.sin_port = htons(4001);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(Dev3Addr.sin_addr, Dev3Addr_str, sizeof(Dev3Addr_str) - 1);
    dev3_socklen = sizeof(Dev3Addr);

    ///************* Config Address and port for server **********///
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

void init_gpio(void)        //LED_INIT_FUNCTION
{
     gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

}

void app_main()
{
    ESP_ERROR_CHECK( nvs_flash_init() );	//Init the device
    init_gpio();
    init_uart();
    wifi_init_softap();

    socket_connect();

    xTaskCreate(rs422_to_udp_task, "rs422_to_udp", 4096, NULL, 5, NULL);
    xTaskCreate(udp_to_rs422_task, "udp_to_rs422", 4096, NULL, 6, NULL);
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 4, NULL);               //LED Task
}
