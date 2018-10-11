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
#include "sound.h"

static const char *TAG = "scary";

enum gamestate {
    WAITING,
    STARTED,
    GUESSING,
    CORRECT,
    TIMEUP,
    RESETTING
} state;

const int strobe_pin = 12;
const int clock_pin = 14;
const int data_pin = 27;

uint8_t display[16]; // The display buffer
uint8_t secret[4] = {0,0,0,0}; // Array to hold the secret code digits
uint8_t code[4] = {0,0,0,0}; // Array to hold the guess digits
uint8_t buttons;
uint8_t guess;
unsigned long reset_now;


void manage_buttons(seven_segment_ui *display)
{
    static uint8_t last_buttons;
    uint8_t changes;
    buttons = read_buttons(display);
    if ((changes = (buttons ^ last_buttons))) {
        last_buttons = buttons;
        /* button state has changed */
        if (changes & 0x80) {
            if (buttons & 0x80) {
                //ESP_LOGI(TAG, "Sound!");
                //sound(22, 19, 1000, 500);
                /* Button Pressed */   
            } else {
                /* Button released */
                if (!(display->flash & 0x80))
                    code[0] = (code[0] +1) % 10;
            }
        }
        if (changes & 0x40) {
            if (!(buttons & 0x40)) {
                /* Button Pressed */   
            } else {
                /* Button released */
                if (!(display->flash & 0x40))
                    code[1] = (code[1] +1) % 10;
            }
        }
        if (changes & 0x20) {
            if (!(buttons & 0x20)) {
                /* Button Pressed */   
            } else {
                /* Button released */
                if (!(display->flash & 0x20))
                    code[2] = (code[2] +1) % 10;
            }
        }
        if (changes & 0x10) {
            if (!(buttons & 0x10)) {
                /* Button Pressed */   
            } else {
                /* Button released */
                if (!(display->flash & 0x10))
                    code[3] = (code[3] +1) % 10;
            }
        }
        /* Check code button changed */
        if (changes & 0x01) {
            if (!(buttons & 0x01)) {
                /* Button Pressed */   
            } else {
                /* Button released */
                guess = 1;
            }
        }
    }
}

void alarm(uint8_t seconds) {
    unsigned long stop = clock_ms() + 1000 * seconds;
    unsigned long toggle = clock_ms() + 1;
    while (clock_ms() < stop) {
        if (clock_ms() > toggle) {
        }
    }
}

uint8_t check_code() {
    int i;
    uint8_t flash=0;
    for (i=0; i<4; i++) {
        if (code[i] == secret[i]) 
            flash |= (0x80 >> i);
    }
    return flash;
}

void app_main()
{
    int i=0;
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



    seven_segment_ui *display = display_setup(strobe_pin, clock_pin, data_pin, 0x01);

    int loop = 0;
    int countdown = -1;
    unsigned long second_timer = clock_ms();
    unsigned long refresh = second_timer;
    state = RESETTING;
    for (;;) {
        /* Manage states */
        switch (state) {
            case WAITING:
                if (buttons) {
                    countdown = 180;
                    state = STARTED;
                    ESP_LOGI(TAG, "State: STARTED");
                }
                break;
            case STARTED:
                display_code(display, NULL);
                if (countdown == 0) {
                    state = TIMEUP;
                    ESP_LOGI(TAG, "State: TIMEUP");
                } else if (buttons) {
                    state = GUESSING;
                    ESP_LOGI(TAG, "State: GUESSING");
                    for (i=0; i<4; i++)
                        code[i] = 0; 
                }
                break;
            case GUESSING:
                if (countdown == 0) {
                    state = TIMEUP;
                    ESP_LOGI(TAG, "State: TIMEUP");
                } else {
                    display_code(display, code);
                    if (guess) {
                        guess = 0;
                        display->flash |= check_code();
                        ESP_LOGI(TAG, "Guess %02x", display->flash);
                        if ((display->flash & 0xf0) == 0xf0) {
                            state = CORRECT;
                            ESP_LOGI(TAG, "State: CORRECT");
                            reset_now = clock_ms() + 9000;
                        }
                    }
                }
                break;
            case CORRECT:
                display->flash |= 0x0f;
                display_code(display, code);
                if (clock_ms() > reset_now) {
                    state = RESETTING;
                    ESP_LOGI(TAG, "State: RESETTING");
                }
                break;
            case TIMEUP:
                state = RESETTING;
                ESP_LOGI(TAG, "State: RESETTING");
                //alarm(5);
                break;
            case RESETTING:
                state = WAITING;
                ESP_LOGI(TAG, "State: WAITING");
                display->flash = 0;
                countdown = -1;
                for (i=0; i<4; i++) {
                    secret[i] = esp_random() % 10;
                }
                display_blank(display);
                break;
        }

        if (clock_ms() > second_timer) {
            second_timer += 1000;
            if (state == STARTED || state == GUESSING) {
                countdown--;
                ESP_LOGI(TAG, "%d", countdown);
            }
        }

        if (clock_ms() > refresh) {
            refresh += 50;
            manage_buttons(display);
            display_timer(display, countdown);
            display_leds(display, display->flash);
            update_display(display);
        }
        display_timer(display, i);
        vTaskDelay(1);
    }
    
}