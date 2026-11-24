/* rgb_led.h
 *
 * Hardware PWM driver for the RGB LED (LED2), driven via nrfx_pwm.
 */

#ifndef RGB_LED_H
#define RGB_LED_H

#include "hsv_color.h"

/* Initializes the PWM peripheral and the three RGB output pins.
 * Must be called once before rgb_led_set_hsv(). */
void rgb_led_init(void);

/* Updates LED2 to display the given HSV color. Internally converts HSV to
 * RGB and only re-arms the PWM sequence when the resulting RGB values
 * actually changed, to avoid needless PWM restarts (and visible flicker). */
void rgb_led_set_hsv(hsv_t hsv);

#endif /* RGB_LED_H */