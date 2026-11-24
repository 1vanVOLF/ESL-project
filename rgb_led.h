#ifndef RGB_LED_H
#define RGB_LED_H

#include "hsv_color.h"


void rgb_led_init(void);

void rgb_led_set_hsv(hsv_t hsv);

#endif