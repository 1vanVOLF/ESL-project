#ifndef INPUT_MODE_H
#define INPUT_MODE_H

#include <stdint.h>

typedef enum
{
    MODE_NONE = 0,
    MODE_HUE,
    MODE_SATURATION,
    MODE_BRIGHTNESS,
    MODE_COUNT
} input_mode_t;

void input_mode_init(void);

void input_mode_advance(void);

input_mode_t input_mode_get(void);

void input_mode_led_update(uint32_t dt_ms);

#endif