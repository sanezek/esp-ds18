/* Very basic example that just demonstrates we can run at all!
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include <string.h>
#include "task.h"
#include "queue.h"
#include <ssid_config.h>
#include <httpd/httpd.h>

#include "ds18b20/ds18b20.h"


#define SENSOR_GPIO 13
#define MAX_SENSORS 8
#define RESCAN_INTERVAL 8
#define LOOP_DELAY_MS 250

QueueHandle_t tempqueue = NULL;

typedef struct {
  ds18b20_addr_t addrs[MAX_SENSORS];
  float temps[MAX_SENSORS];
  int sensor_count;
} tempStat ;

void temperature_task(void *pvParameters) {
    ds18b20_addr_t addrs[MAX_SENSORS];
    float temps[MAX_SENSORS];
    int sensor_count;
    char response[256];
    // There is no special initialization required before using the ds18b20
    // routines.  However, we make sure that the internal pull-up resistor is
    // enabled on the GPIO pin so that one can connect up a sensor without
    // needing an external pull-up (Note: The internal (~47k) pull-ups of the
    // ESP8266 do appear to work, at least for simple setups (one or two sensors
    // connected with short leads), but do not technically meet the pull-up
    // requirements from the DS18B20 datasheet and may not always be reliable.
    // For a real application, a proper 4.7k external pull-up resistor is
    // recommended instead!)

    gpio_set_pullup(SENSOR_GPIO, true, true);

    while(1) {
        // Every RESCAN_INTERVAL samples, check to see if the sensors connected
        // to our bus have changed.
        sensor_count = ds18b20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS);
        sensor_count = sensor_count;
        if (sensor_count < 1) {
            strcpy(response,"{\"sensor_count\":0}");
        } else {
            // printf("\n%d sensors detected:\n", sensor_count);
            // If there were more sensors found than we have space to handle,
            // just report the first MAX_SENSORS..
            if (sensor_count > MAX_SENSORS) sensor_count = MAX_SENSORS;

            // Do a number of temperature samples, and print the results.
            for (int i = 0; i < RESCAN_INTERVAL; i++) {
                ds18b20_measure_and_read_multi(SENSOR_GPIO, addrs, sensor_count, temps);
                char sensors[200];
                char past_sensors[200];
                int i,sensors_len;
                int32_t addr0;
                uint32_t addr1;
                float temp_c;
                sensors_len = 0;
                for (i = 0; i < sensor_count-1; i++){
                    strcpy(past_sensors,sensors);
                    addr0 = addrs[i] >> 32;
                    addr1 = addrs[i];
                    temp_c = temps[i];
                    // printf("%i\n",i);
                    sensors_len = snprintf(sensors,sizeof(sensors),
                    "%.*s\"%08x%08x\":%.2f,", sensors_len ,past_sensors ,addr0, addr1, temp_c
                  );
                  // printf("%i\n%s",sensors_len,sensors);
                }
                addr0 = addrs[i] >> 32;
                addr1 = addrs[i];
                temp_c = temps[i];
                strcpy(past_sensors,sensors);
                // printf(past_sensors);
                sensors_len = snprintf(sensors,sizeof(sensors),
                "%.*s\"%08x%08x\":%.2f", sensors_len ,past_sensors ,addr0, addr1, temp_c);
                snprintf(response,sizeof(response),
                "{\"sensor_count\": %i,\"sensors\":{%.*s}}",sensor_count, sensors_len, sensors
                );
                }
                // printf("\n");

                // Wait for a little bit between each sample (note that the
                // ds18b20_measure_and_read_multi operation already takes at
                // least 750ms to run, so this is on top of that delay).
                xQueueReset(tempqueue);
                xQueueSend(tempqueue, &response,100);
                vTaskDelay(LOOP_DELAY_MS / portTICK_PERIOD_MS);
            }
        }

}



void print_temperature_task(void *pvParameters)
{
    printf("Hello from task 2!\r\n");
    while(1) {
        char response[256];
        if(xQueuePeek(tempqueue, &response, 1000)) {
            printf(response);
        }
        printf("\n");
        vTaskDelay(1000);
    }
}

void websocket_cb(struct tcp_pcb *pcb, uint8_t *data, u16_t data_len, uint8_t mode)
{
    printf("[websocket_callback]:\n%.*s\n", (int) data_len, (char*) data);
    printf("0\n");
    char response[256];
    uint16_t val;
    int resp_len;

    switch (data[0]) {
        case 'A': // ADC
            xQueuePeek(tempqueue, &response, 1000);
            break;
        default:
            printf("Unknown command\n");
            val = 0;
            break;
    }
    printf("4\n");
    websocket_write(pcb, (unsigned char *) response, strlen(response), WS_TEXT_MODE);
}

/**
 * This function is called when new websocket is open and
 * creates a new websocket_task if requested URI equals '/stream'.
 */
void websocket_open_cb(struct tcp_pcb *pcb, const char *uri) {}

void httpd_task(void *pvParameters)
{

    websocket_register_callbacks((tWsOpenHandler) websocket_open_cb,
            (tWsHandler) websocket_cb);
    httpd_init();

    for (;;);
}


void user_init(void)
{
    uart_set_baud(0, 115200);

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);
    sdk_wifi_station_connect();

    printf("SDK version:%s\n", sdk_system_get_sdk_version());
    tempqueue = xQueueCreate(1, sizeof(char[256]));
    xTaskCreate(temperature_task, "tsk1", 512, NULL, 2, NULL);
    xTaskCreate(print_temperature_task, "tsk2", 256, NULL, 2, NULL);
    xTaskCreate(&httpd_task, "HTTP Daemon", 1024, NULL, 2, NULL);
}
