/* hsv_color.h
 *
 * HSV and RGB color types plus the HSV -> RGB conversion used to drive
 * the PWM-controlled RGB LED.
 */

#ifndef HSV_COLOR_H
#define HSV_COLOR_H

#include <stdint.h>

#define HUE_MAX        360   /* valid range: 0..359 */
#define PERCENT_MAX    100   /* valid range for Saturation/Value: 0..100 */

typedef struct
{
    uint16_t hue;         /* 0..359 degrees           */
    uint8_t  saturation;  /* 0..100 percent           */
    uint8_t  value;       /* 0..100 percent (brightness) */
} hsv_t;

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

/* Standard HSV (H: 0-359, S: 0-100, V: 0-100) to RGB (0-255) conversion. */
rgb_t hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v);

#endif /* HSV_COLOR_H */