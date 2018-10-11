#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include "7_seg_ui.h"

#include "esp_system.h"
#include "esp_log.h"

#include "esp_useful.h"

static const char *TAG = "7-seg";

/*
 * Set the 7-segment displays using an array of values
 * Each 8-bit value controls a single 7-segment display, 
 * plus its decimal point.
 * 
 * The eight bits are as follows:  Msb   PGFEDCBA   Lsb
 * P - Decimal point
 * G - Middle Horizontal
 * F - Top Left
 * E - Bottom Left
 * D - Bottom Horizontal
 * C - Bottom Right
 * B - Top Right
 * A - Top Horizontal
 */
/* 7-segment display lookup */
const int led_digits[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71};

/* Pre-build command buffers */
uint8_t set_all[] = {0xff, 0x01, 0xff, 0x01, 0xff, 0x01, 0xff, 0x01,
                                0xff, 0x01, 0xff, 0x01, 0xff, 0x01, 0xff, 0x01};
uint8_t set_none[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* 
 * Bit-banging to talk to the display
 * This function clocks a single byte out to the display
 */
void bb_send_byte(seven_segment_ui *display, uint8_t value)
{
    int i;
    /* Loop through the data */
    ESP_LOGD(TAG, "Sending byte: %02x", value);
    for (i=0; i<16; i++) {
        if (i % 2 == 0) {
            /* Set the data bit */
            gpio_set_level(display->data_pin, (value >> i/2) & 0x01);
            /* Take the clock low */
            gpio_set_level(display->clock_pin, 0);
        } else {
            /* Take the clock high */
            gpio_set_level(display->clock_pin, 1);
        }
    }
}


/*
 * The display expects the initial command to be sent on its own
 * surrounded by chip select (strobe)
 * Commands:
 *   0x8? - Enable display and set brightness
 *   0x44 - Write to a single address
 *   0x40 - Write to multiple address using auto increment address
 *   0x42 - Read  buttons
 */
void bb_send_cmd(seven_segment_ui *display, uint8_t cmd)
{
    /* Set the strobe low */
    gpio_set_level(display->strobe_pin, 0);
    bb_send_byte(display, cmd);
    /* Set the strobe high */
    gpio_set_level(display->strobe_pin, 1);
}


/*
 * A wrapper command to send data using auto-increment addressing
 */
void bb_send(seven_segment_ui *display){
    ESP_LOGD(TAG, "bb_send");
    bb_send_cmd(display, 0x40); // Bulk update
    int i;
    /* Set the strobe low */
    gpio_set_level(display->strobe_pin, 0);
    bb_send_byte(display, 0xc0); // Start address
    for (i=0; i<DISPLAY_BUFFER_LENGTH; i++){
        bb_send_byte(display, display->display_buffer[i]);
    }
    /* Set the strobe high */
    gpio_set_level(display->strobe_pin, 1);
}


/*
 * Write a value to a single address
 */
void bb_send_address(seven_segment_ui *display, uint8_t address, uint8_t value)
{
    /* Set the strobe low */
    gpio_set_level(display->strobe_pin, 0);
    bb_send_byte(display, 0x44);
    bb_send_byte(display, address);
    bb_send_byte(display, value);
    /* Set the strobe high */
    gpio_set_level(display->strobe_pin, 1);
}

/*
 * Read the buttons
 * This is polling only, you can't interrupt on press
 */
uint8_t bb_read_buttons(seven_segment_ui *display)
{
    int i, j;
    uint8_t value = 0;
    uint8_t bit;
    uint8_t buttons = 0;
    /* Set the strobe low */
    gpio_set_level(display->strobe_pin, 0);
    bb_send_byte(display, 0x42); // Read data command
    gpio_set_direction(display->data_pin, GPIO_MODE_INPUT);
    for (i=0; i<4; i++) {
        for (j=0; j<16; j++) {
            if (j % 2 == 0) {
                /* Take the clock low */
                gpio_set_level(display->clock_pin, 0);
            } else {
                /* Take the clock high */
                gpio_set_level(display->clock_pin, 1);
                /* Read the data bit */
                bit = gpio_get_level(display->data_pin);
                value = value << 1;
                value |= (bit & 0x01);
            }
        }
        buttons |= value >> i;
    }
    gpio_set_direction(display->data_pin, GPIO_MODE_OUTPUT);
    /* Set the strobe high */
    gpio_set_level(display->strobe_pin, 1);
    if (buttons != 0)
        ESP_LOGI(TAG, "Buttons: 0x%02x", buttons);
    return buttons;
}


/*
 * Update the display with the values in the display buffer
 */
void update_display(seven_segment_ui *display)
{
    int i;
    ESP_LOGD(TAG, "Update display");
    /* Deal with flashing digits */
    if (((clock_ms() / 500) % 2) == 0) {
        for (i=0; i<8; i++){
            if (display->flash & (0x01 << i)) {
                display->display_buffer[2*(7-i)] = 0x00;
            }
        }
    }
    bb_send(display);
}


/*
 * Blank the display buffer
 * NOTE:  You need to call update_display() to pass data to display
 */
void display_blank(seven_segment_ui *display)
{
    int i;
    /* Blank the display */
    for (i=0; i<DISPLAY_BUFFER_LENGTH; i++) 
        display->display_buffer[i] = set_none[i];
}


/*
 * Fill the display buffer - All leds on
 * NOTE:  You need to call update_display() to pass data to display
 */
void display_all(seven_segment_ui *display)
{
    int i;
    for (i=0; i<DISPLAY_BUFFER_LENGTH; i++) 
        display->display_buffer[i] = set_all[i];
}


/*
 * Initialise the display
 * You get back a display handle that you can use in subsequent calls
 */
seven_segment_ui* display_setup(uint8_t strobe_pin, 
                            uint8_t clock_pin, 
                            uint8_t data_pin, 
                            uint8_t brightness)
{
    ESP_LOGD(TAG, "Display Init: str: %d, clk: %d, dat: %d", strobe_pin, clock_pin, data_pin);
    seven_segment_ui *display = malloc(sizeof(seven_segment_ui));
    if (display == NULL) {
        ESP_LOGE(TAG, "Unable to allocate memory for display");
    } else {
        display->strobe_pin = strobe_pin;
        display->clock_pin = clock_pin;
        display->data_pin = data_pin;
        display->flash = 0;

        /* Set up the pins */
        gpio_pad_select_gpio(display->data_pin);
        gpio_pad_select_gpio(display->clock_pin);
        gpio_pad_select_gpio(display->strobe_pin);
        gpio_set_direction(display->data_pin, GPIO_MODE_OUTPUT);
        gpio_set_direction(display->clock_pin, GPIO_MODE_OUTPUT);
        gpio_set_direction(display->strobe_pin, GPIO_MODE_OUTPUT);
        /* Enable display and set brightness */
        uint8_t enable_display = 0x88 | (brightness & 0x07);
        ESP_LOGI(TAG, "Enabling display with: 0x%02x", enable_display);
        bb_send_cmd(display, enable_display);
        vTaskDelay(1);
        display_blank(display);
        update_display(display);
        display->initialized = 1;
    }
    return display;
}


/*
 * The 7-segment display and single LEDS are interleaved in address
 * So not easy just to set one or the other
 * The follong functions ease setting just the LEDs or the segments
 */

/*
 * Set the row of 8 LEDs above the segments using a single 8-bit number
 */
void display_leds(seven_segment_ui *display, uint8_t value) 
{
    int i;
    for (i=0; i<8; i++) {
        ESP_LOGD(TAG, "Setting display[%d] = %d", ((i*2)+1), ((value >> i) & 0x01));
        display->display_buffer[((7-i)*2)+1] = (uint8_t) ((value >> i) & 0x01);
    }
}


/*
 * Convert number into a display digit
 * It will take a digit and does the following:
 * * If the digit is negative the digit will be blanked
 * * check if it greater than 15 - if so set the decimal point
 * * Get a value < 16 using modulus and convert it into a display digit
 */
uint8_t display_digit(uint8_t digit)
{
    int i;
    uint8_t seg;
    uint8_t d;
    if (digit < 0) {
        /* Blank the display */
        seg = 0x00;
    } else {
        /* Show hex digit */
        d = digit % 16;
        seg = led_digits[d];
        /* Set the decimal point */
        seg |= (digit > 15) ? 0x80 : 0x00;
    }
    return seg;
}

void display_timer(seven_segment_ui *display, int seconds) 
{
    if (seconds < 0) {
        display->display_buffer[8] = 0x00;
        display->display_buffer[10] = 0x00;
        display->display_buffer[12] = 0x00;
        display->display_buffer[14] = 0x00;
    } else {
        uint8_t minutes = seconds / 60;
        seconds = seconds - (minutes * 60);
        uint8_t tens = minutes / 10;
        display->display_buffer[8] = display_digit(tens);
        minutes = minutes % 10;
        display->display_buffer[10] = display_digit(minutes);
        tens = seconds / 10;
        display->display_buffer[12] = display_digit(tens);
        seconds = seconds % 10;
        display->display_buffer[14] = display_digit(seconds);
        /* Flash the decimal point once per second */
        if (seconds % 2 == 0)
            display->display_buffer[10] |= 0x80;
    }
}


void display_code(seven_segment_ui *display, uint8_t *code)
{
    /* Display the word c0de of no code supplied */
    if (code == NULL) {
        uint8_t codeword[] = {12, 0, 13, 14};
        display_code(display, codeword);
    } else {
        /* Display the code numbers */
        int i;
        for (i=0; i<4; i++) {
            display->display_buffer[2*i] = display_digit(code[i]);
        }
    }
}

uint8_t read_buttons(seven_segment_ui *display)
{
    return bb_read_buttons(display);
}