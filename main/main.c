/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
//#include "driver/gpio.h"
#include "esp_log.h"

#include "esp_useful.h"
#include "7_seg_ui.h"
#include "sound.h"

/* Control how the program operates */
#define DEBUG 1
#define ALARM 1
#define TICK 1
#define TILT 1
#define TILT_ARM_DELAY 30000

/* On average it will miss a tick every... */
#define MISS_TICK 6000

static const char *TAG = "scary";

seven_segment_ui *display;

enum gamestate {
    STARTED,
    GUESSING,
    CORRECT,
    TIMEUP,
    RESETTING,
    GAMEOVER
} state;

const int strobe_pin = 12;
const int clock_pin = 14;
const int data_pin = 27;
const int beep_pin = 19;
const int beep_gnd = 22;
const int tilt_pin = 18;
const int tilt_gnd = 23;

uint8_t secret[4] = {0,0,0,0}; // Array to hold the secret code digits
uint8_t code[4] = {0,0,0,0}; // Array to hold the guess digits

void gpio_setup() {
    gpio_pad_select_gpio(beep_pin);
    gpio_pad_select_gpio(beep_gnd);
    gpio_pad_select_gpio(tilt_pin);
    gpio_pad_select_gpio(tilt_gnd);
    gpio_set_direction(beep_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(beep_gnd, GPIO_MODE_OUTPUT);
    gpio_set_direction(tilt_pin, GPIO_MODE_INPUT);
    gpio_set_direction(tilt_gnd, GPIO_MODE_OUTPUT);
    gpio_set_level(beep_pin, 1);
    gpio_set_level(beep_gnd, 1);
    gpio_set_level(tilt_gnd, 0);
    //gpio_pullup_enable(tilt_pin);
    gpio_set_pull_mode(tilt_pin, GPIO_PULLUP_ONLY);
}
void beep(int ticks) {
    gpio_set_level(beep_gnd, 0);
    vTaskDelay(ticks);
    gpio_set_level(beep_gnd, 1);
}

void tick() {
    #if TICK
    if (esp_random() % MISS_TICK)
        beep(1);
    #endif
}

void alarm(int ticks) {
    #if ALARM
    beep(ticks);
    #endif
}

void endgame(seven_segment_ui *display) {
    int i;
    uint8_t leds = 0xff;
    gpio_set_level(beep_gnd, 0);
    for (i=0; i<8; i++) {
        display_leds(display, leds);
        update_display(display);
        leds >>= 1;
        vTaskDelay(10*(8-i));
    }
    gpio_set_level(beep_gnd, 1);
}

uint8_t manage_buttons(seven_segment_ui *display)
{
    /* Keep state a cross calls */
    static uint8_t last_buttons;

    /* Read the current state of the buttons */
    ESP_LOGV(TAG, "Reading buttons");
    uint8_t buttons = read_buttons(display);
    /* Work out which buttons have changed */
    uint8_t changes = buttons ^ last_buttons;
    /* Work out which buttons have been released */
    uint8_t released = ~buttons & changes;

    /* Have the button states changed? */
    if (changes) {
        /* They have so update state and notify */
        last_buttons = buttons;
        ESP_LOGD(TAG, "Buttons: 0x%02X   Released: 0x%02X", buttons, released);
    }
    /* Return the released buttons */
    return released;
}

uint8_t check_code() {
    int i;
    /* Assume all bits unmatched - 1=NOMATCH */
    uint8_t flash=0xf0;
    for (i=0; i<4; i++) {
        /* Check each digit of the code */
        if (code[i] == secret[i]) 
            /* Clear the bit if we match */
            flash &= ~(0x80 >> i);
    }
    ESP_LOGD(TAG, "Check Code: %02x", flash);
    return flash;
}

void game1(unsigned int count_from)
{
    int i=0;
    uint8_t buttons_released = 0;

    int countdown = -1;
    clock_t second_timer = clock();
    clock_t refresh = second_timer;
    unsigned long reset_now=0;
    state = RESETTING;
    while (state != GAMEOVER) {
        /* Manage states */
        switch (state) {
            case STARTED:
                display_code(display, NULL);
                if (countdown == 0) {
                    state = TIMEUP;
                    ESP_LOGD(TAG, "State: TIMEUP");
                } else if (buttons_released) {
                    state = GUESSING;
                    ESP_LOGD(TAG, "State: GUESSING");
                }
                break;
            case GUESSING:
                if (countdown == 0) {
                    state = TIMEUP;
                    ESP_LOGD(TAG, "State: TIMEUP");
                } else {
                    /* Update the guessed code */
                    if (buttons_released & 0x80) {
                        code[0] = (code[0] +1) % 10;
                    }
                    if (buttons_released & 0x40) {
                        code[1] = (code[1] +1) % 10;
                    }
                    if (buttons_released & 0x20) {
                        code[2] = (code[2] +1) % 10;
                    }
                    if (buttons_released & 0x10) {
                        code[3] = (code[3] +1) % 10;
                    }
                    #if DEBUG
                    /* Check code button changed */
                    if (buttons_released & 0x08) {
                        /* End the countdown
                         * This button will not be pushable when built... */
                        ESP_LOGE(TAG, "Artificially ended game!");
                        ESP_LOGD(TAG, "State: GAMEOVER");
                        state = GAMEOVER;
                    }  
                    #endif
                    if (buttons_released & 0x01) {
                        display->flash = check_code();
                        ESP_LOGI(TAG, "Guess %02x", display->flash);
                        if ((display->flash & 0xf0) == 0x00) {
                            state = CORRECT;
                            ESP_LOGD(TAG, "State: CORRECT");
                            reset_now = clock() + 9000;
                        }
                    }
                }
                display_code(display, code);
                break;
            case CORRECT:
                display->flash = 0x0f;
                display_code(display, code);
                if (clock() > reset_now) {
                    state = GAMEOVER;
                    ESP_LOGD(TAG, "State: RESETTING");
                }
                break;
            case TIMEUP:
                state = GAMEOVER;
                ESP_LOGD(TAG, "State: GAMEOVER");
                endgame(display);
                break;
            case RESETTING:
                state = STARTED;
                ESP_LOGD(TAG, "State: STARTED");
                display->flash = 0xf0;
                countdown = count_from;
                for (i=0; i<4; i++) {
                    secret[i] = esp_random() % 10;
                    code[i] = 0;
                }
                display_blank(display);
                break;
            case GAMEOVER:
                break;
            default:
                ESP_LOGE(TAG, "Unhandled state in Game 1");
        }

        /* We have dealt with any released buttons now so reset */
        buttons_released = 0;

        /* Manage timed events */
        if (clock() > second_timer) {
            second_timer += 1000;
            if (state == STARTED || state == GUESSING) {
                countdown--;
                ESP_LOGD(TAG, "%d", countdown);
                tick();

            }
        }

        if (clock() > refresh) {
            refresh += 50;
            buttons_released = manage_buttons(display);
            display_timer(display, countdown);
            //display_leds(display, display->flash);
            update_display(display);
        }
        vTaskDelay(1);
    }
    display_blank(display);
    update_display(display);
    ESP_LOGI(TAG, "Game 1 ended!");
}
    
void binary_task(void *pvParameters)
{
    const int led_pin = 22;
    uint8_t led_value = 0x00;

    gpio_pad_select_gpio(led_pin);
    gpio_set_direction(led_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(led_pin, 0);

    unsigned long counter = 0;
    clock_t refresh = clock() + 1000;

    ESP_LOGD(TAG, "Starting...");
    seven_segment_ui *display = display_setup(strobe_pin, clock_pin, data_pin, 0x01);
    for (;;) {
        if(clock() > refresh) {
            refresh = clock() + 1000;
            display_leds(display, (uint8_t)(++counter & 0xff));
            update_display(display);
            ESP_LOGI(TAG, "%lu", counter);
        }
        led_value ^= 0x01;
        gpio_set_level(led_pin, led_value);
        vTaskDelay(50);
    }
    ESP_LOGE(TAG, "Binary task ended!");
    vTaskDelete(NULL);
}


void app_main()
{
    /* Change log level for all components */
    //esp_log_level_set("*", ESP_LOG_ERROR); 

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

    /* initialise the display */
    display = display_setup(strobe_pin, clock_pin, data_pin, 0x01);
    /* Initialise the sound and tilt sensor */
    gpio_setup();

    clock_t check_buttons = 0;
    int button_ticks = 10;
    clock_t flash_led = 0;
    int led_ticks = 15000;
    uint8_t released_buttons;
    #if TILT
    clock_t tilt_arm = clock() + TILT_ARM_DELAY;
    uint8_t tilt_armed = 0;
    uint8_t tilt = gpio_get_level(tilt_pin);
    uint8_t tilt_prev = tilt;
    #endif

    /* Main loop */
    for (;;) {
        /* Check if a button has been pushed */
        if (clock() > check_buttons || (check_buttons - clock()) > button_ticks) {
            check_buttons = clock() + button_ticks;
            released_buttons = manage_buttons(display);
            if (released_buttons) {
                if (released_buttons & 0x02) {
                    game1(10);
                } else if (released_buttons & 0x01) {
                    game1(60); 
                } else {
                    game1(esp_random() % (2 * released_buttons));
                }
                #if TILT
                tilt_armed = 0;
                tilt_arm = clock() + TILT_ARM_DELAY;
                #endif
            }
            #if TILT
            tilt = gpio_get_level(tilt_pin);
            if (tilt_armed) {
                /* Check the tilt switch */
                if (tilt != tilt_prev) {
                    /* Somebody moved us... */
                    ESP_LOGI(TAG, "Tilted...  Starting countdown!");
                    game1(60 + esp_random() % 60);
                    tilt_armed = 0;
                    tilt_arm = clock() + TILT_ARM_DELAY;
                }
            } else {
                if (tilt != tilt_prev) {
                    /* We are not yet armed and are being moved */
                    /* Reset the arming delay */
                    ESP_LOGI(TAG, "Delaying tilt arming due to movement");
                    tilt_prev = tilt;
                    tilt_arm = clock() + TILT_ARM_DELAY;
                }
                if (clock() > tilt_arm) {
                    ESP_LOGI(TAG, "Tilt mechanism armed...");
                    /* We should arm the tilt mechanism */
                    tilt_prev = gpio_get_level(tilt_pin);
                    tilt_armed = 1;
                } 
            }
            #endif
        }
        /* Flash the LED */
        #if TILT
        if (tilt_armed) {
        #endif

        if (clock() > flash_led || (flash_led - clock()) > led_ticks) {
            flash_led = clock() + led_ticks;
            display_leds(display, 0x01);
            update_display(display);
            vTaskDelay(5);
            display_leds(display, 0x00);
            update_display(display);
        }

        #if TILT
        }
        #endif
        /* Yield */
        vTaskDelay(5);
    }
}