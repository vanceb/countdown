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

static const char *TAG = "scary";


const int strobe = 12;
const int clock = 14;
const int data = 27;

uint8_t display[16]; // The display buffer
uint8_t s1,s2,s3,s4; // Variables to hold the secret code digits
uint8_t c1,c2,c3,c4; // Variables to hold the guess digits

/* 7-segment display lookup */
const int digits[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};

/* Pre-build command buffers */
uint8_t set_all[] = {0xff, 0x01, 0xff, 0x01, 0xff, 0x01, 0xff, 0x01,
                                0xff, 0x01, 0xff, 0x01, 0xff, 0x01, 0xff, 0x01};
uint8_t set_none[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


void bb_send_byte(uint8_t value)
{
    int i;
    /* Loop through the data */
    ESP_LOGD(TAG, "Sending byte: %02x", value);
    for (i=0; i<16; i++) {
        if (i % 2 == 0) {
            /* Set the data bit */
            gpio_set_level(data, (value >> i/2) & 0x01);
            /* Take the clock low */
            gpio_set_level(clock, 0);
        } else {
            /* Take the clock high */
            gpio_set_level(clock, 1);
        }
    }
}

void bb_send_cmd(uint8_t cmd)
{
    /* Set the strobe low */
    gpio_set_level(strobe, 0);
    bb_send_byte(cmd);
    /* Set the strobe high */
    gpio_set_level(strobe, 1);
}

void bb_send(uint8_t *to_display, uint8_t length){
    ESP_LOGD(TAG, "bb_send");
    bb_send_cmd(0x40); // Bulk update
    int i;
    /* Set the strobe low */
    gpio_set_level(strobe, 0);
    bb_send_byte(0xc0); // Start address
    for (i=0; i<length; i++){
        bb_send_byte(to_display[i]);
    }
    /* Set the strobe high */
    gpio_set_level(strobe, 1);
}

void bb_send_address(uint8_t address, uint8_t value)
{
    /* Set the strobe low */
    gpio_set_level(strobe, 0);
    bb_send_byte(0x44);
    bb_send_byte(address);
    bb_send_byte(value);
    /* Set the strobe high */
    gpio_set_level(strobe, 1);
}

uint8_t bb_read_buttons()
{
    int i, j;
    uint8_t value = 0;
    uint8_t bit;
    uint8_t buttons = 0;
    /* Set the strobe low */
    gpio_set_level(strobe, 0);
    bb_send_byte(0x42); // Read data command
    gpio_set_direction(data, GPIO_MODE_INPUT);
    for (i=0; i<4; i++) {
        for (j=0; j<16; j++) {
            if (j % 2 == 0) {
                /* Take the clock low */
                gpio_set_level(clock, 0);
            } else {
                /* Take the clock high */
                gpio_set_level(clock, 1);
                /* Read the data bit */
                bit = gpio_get_level(data);
                value = value << 1;
                value |= (bit & 0x01);
            }
        }
        buttons |= value >> i;
    }
    gpio_set_direction(data, GPIO_MODE_OUTPUT);
    /* Set the strobe high */
    gpio_set_level(strobe, 1);
    if (buttons != 0)
        ESP_LOGI(TAG, "Buttons: 0x%02x", buttons);
    return buttons;
}


void update_display()
{
    ESP_LOGD(TAG, "Update display");
    bb_send(display, 16);
}


void display_blank()
{
    int i;
    /* Blank the display */
    for (i=0; i<16; i++) 
        display[i] = set_none[i];
}

void display_full()
{
    int i;
    for (i=0; i<16; i++) 
        display[i] = set_all[i];
}

void display_setup(uint8_t brightness)
{
    ESP_LOGI(TAG, "Display Init");
    /* Set up the pins */
    gpio_pad_select_gpio(data);
    gpio_pad_select_gpio(clock);
    gpio_pad_select_gpio(strobe);
    gpio_set_direction(data, GPIO_MODE_OUTPUT);
    gpio_set_direction(clock, GPIO_MODE_OUTPUT);
    gpio_set_direction(strobe, GPIO_MODE_OUTPUT);
    /* Enable display and set brightness */
    uint8_t enable_display = 0x88 | (brightness & 0x07);
    ESP_LOGI(TAG, "Enabling display with: 0x%02x", enable_display);
    bb_send_cmd(enable_display);
    vTaskDelay(1);
    display_blank();
    update_display();
}

void display_leds(uint8_t value) {
    int i;
    for (i=0; i<8; i++) {
        ESP_LOGD(TAG, "Setting display[%d] = %d", ((i*2)+1), ((value >> i) & 0x01));
        display[((7-i)*2)+1] = (uint8_t) ((value >> i) & 0x01);
    }
}

void display_timer(int seconds) {
    uint8_t minutes = seconds / 60;
    seconds = seconds - (minutes * 60);
    uint8_t tens = minutes / 10;
    display[8] = digits[tens];
    minutes = minutes % 10;
    display[10] = digits[minutes];
    tens = seconds / 10;
    display[12] = digits[tens];
    seconds = seconds % 10;
    display[14] = digits[seconds];
    /* Flash the decimal point once per second */
    if (seconds % 2 == 0)
        display[10] = display[10] | 0x80;
}

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
        update_display();
        vTaskDelay(100);
    }
    
}