/* input_mode.h
 *
 * Input mode state (No input / Hue / Saturation / Brightness) and the
 * LED1 indication that reflects the current mode.
 */

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

/* Initializes LED1 as an output and turns it off. */
void input_mode_init(void);

/* Advances to the next mode in the cycle:
 * No input -> Hue -> Saturation -> Brightness -> No input -> ... */
void input_mode_advance(void);

/* Returns the currently selected input mode. */
input_mode_t input_mode_get(void);

/* Updates LED1 according to the current mode:
 *   No input    -> always off
 *   Hue         -> blinks slowly
 *   Saturation  -> blinks fast
 *   Brightness  -> always on
 * Must be called periodically (every dt_ms milliseconds) from the
 * application tick. */
void input_mode_led_update(uint32_t dt_ms);

#endif /* INPUT_MODE_H */