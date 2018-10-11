#ifndef SEVEN_SEG_UI_H
#define SEVEN_SEG_UI_H

#include <stdint.h>

#define DISPLAY_BUFFER_LENGTH 16

typedef struct ui {
    uint8_t data_pin;
    uint8_t clock_pin;
    uint8_t strobe_pin;
    uint8_t display_buffer[DISPLAY_BUFFER_LENGTH];
    uint8_t flash;
    uint8_t initialized;
} seven_segment_ui;

/* Public functions */
extern seven_segment_ui* display_setup(uint8_t strobe_pin, 
                            uint8_t clock_pin, 
                            uint8_t data_pin, 
                            uint8_t brightness);
extern void update_display(seven_segment_ui *display);
extern void display_blank(seven_segment_ui *display);
extern void display_all(seven_segment_ui *display);
extern void display_leds(seven_segment_ui *display, uint8_t value);
extern uint8_t display_digit(uint8_t digit);
extern void display_code(seven_segment_ui *display, uint8_t *code);
extern void display_timer(seven_segment_ui *display, int seconds);
extern uint8_t read_buttons(seven_segment_ui *display);

#endif