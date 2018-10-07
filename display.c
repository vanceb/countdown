#include <stdio.h>

static const char *TAG = "7seg";

enum state {
    OFF = 0,
    RUNNING,
    WON,
    LOST
};

uint8_t s1,s2,s3,s4; // Variables to hold the secret code digits
uint8_t c1,c2,c3,c4; // Variables to hold the guess digits

/* 7-segment display lookup */
const int digits[] = {0x3f, 0x06, 0x9b, 0x8f, 0xc6, 0x6d, 0x7d, 0x07, 0x7f, 0xcf};

/* Pre-build command buffers */
uint8_t set_all[] = {0x40, 0xc0, 0xff, 0x01, 0xff, 0x01, 0xff, 0x01, 0xff, 0x01,
                                0xff, 0x01, 0xff, 0x01, 0xff, 0x01, 0xff, 0x01};
uint8_t set_none[] = {0x40, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


void bb_send_byte(uint8_t value)
{
    int i;
    /* Loop through the data */
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


void bb_send(uint8_t *to_display, uint8_t length){
    ESP_LOGD(TAG, "bb_send");
    int i;
    /* Make sure the commands are set at the start of the buffer for the transfer */
    to_display[0] = 0x40;
    to_display[1] = 0xc0;
    ESP_LOGD(TAG, "Sending: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
                to_display[0],
                to_display[1],
                to_display[2],
                to_display[3],
                to_display[4],
                to_display[5],
                to_display[6],
                to_display[7],
                to_display[8],
                to_display[9],
                to_display[10],
                to_display[11],
                to_display[12],
                to_display[13],
                to_display[14],
                to_display[15],
                to_display[16],
                to_display[17]
                );
    /* Set the strobe low */
    gpio_set_level(strobe, 0);
    //vTaskDelay(1);
    for (i=0; i<length; i++){
        ESP_LOGD(TAG, "Sending byte: %02x", to_display[i]);
        bb_send_byte(to_display[i]);
    }
    /* Set the strobe high */
    //vTaskDelay(1);
    gpio_set_level(strobe, 1);
}

void bb_send_single(uint8_t *to_display, uint8_t length)
{
    int i;
    /* Set the strobe low */
    gpio_set_level(strobe, 0);
    for (i=0; i<length; i++){
        bb_send_byte(0x44);
        bb_send_byte(0xc0 + i);
        bb_send_byte(to_display[i]);
        vTaskDelay(1);
    }
    /* Set the strobe high */
    gpio_set_level(strobe, 1);
}

void update_display()
{
    ESP_LOGD(TAG, "Update display");
    bb_send(display, 18);
    //bb_send_single(&display[2], 16);
}


void display_blank()
{
    int i;
    /* Blank the display */
    for (i=0; i<18; i++) 
        display[i] = set_none[i];
}

void display_full()
{
    int i;
    for (i=0; i<18; i++) 
        display[i] = set_all[i];
}

void display_setup(uint8_t brightness)
{
    ESP_LOGD(TAG, "Display Init");
    /* Set up the pins */
    gpio_pad_select_gpio(data);
    gpio_pad_select_gpio(clock);
    gpio_pad_select_gpio(strobe);
    gpio_set_direction(data, GPIO_MODE_OUTPUT);
    gpio_set_direction(clock, GPIO_MODE_OUTPUT);
    gpio_set_direction(strobe, GPIO_MODE_OUTPUT);
    /* Enable display and set brightness */
    uint8_t enable_display[] = {0x88 | (brightness & 0x07)};
    bb_send(enable_display, 1);
    display_blank();
    update_display();
}

void display_leds(uint8_t value) {
    int i;
    for (i=0; i<8; i++) {
        ESP_LOGD(TAG, "Setting display[%d] = %d", ((i*2)+3), ((value >> i) & 0x01));
        display[(i*2)+3] = (uint8_t) ((value >> i) & 0x01);
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
