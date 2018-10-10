/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
//#include "driver/gpio.h"
#include "esp_log.h"

#include "esp_useful.h"
#include "7_seg_ui.h"

static const char *TAG = "scary";


const int strobe = 12;
const int clock = 14;
const int data = 27;

uint8_t display[16]; // The display buffer
uint8_t s1,s2,s3,s4; // Variables to hold the secret code digits
uint8_t c1,c2,c3,c4; // Variables to hold the guess digits

void app_main()
{
    int i;
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    display_setup(0x01);

    int loop = 0;
    uint8_t leds = 0;
    for(i=180;i>=0;i--) {
        ESP_LOGI(TAG, "%d", i);
        display_timer(i);
        display_leds(bb_read_buttons());
        display_code(NULL);
        update_display();
        vTaskDelay(100);
    }
    
}