/* hsv_color.c
 *
 * Implementation of the HSV -> RGB conversion.
 */

#include "hsv_color.h"

#include <math.h>

rgb_t hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v)
{
    rgb_t rgb;

    if (h >= HUE_MAX)
    {
        h = (uint16_t)(h % HUE_MAX);
    }

    float sf = s / 100.0f;
    float vf = v / 100.0f;

    float c = vf * sf;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = vf - c;

    float r1, g1, b1;

    if (h < 60)
    {
        r1 = c; g1 = x; b1 = 0;
    }
    else if (h < 120)
    {
        r1 = x; g1 = c; b1 = 0;
    }
    else if (h < 180)
    {
        r1 = 0; g1 = c; b1 = x;
    }
    else if (h < 240)
    {
        r1 = 0; g1 = x; b1 = c;
    }
    else if (h < 300)
    {
        r1 = x; g1 = 0; b1 = c;
    }
    else
    {
        r1 = c; g1 = 0; b1 = x;
    }

    rgb.r = (uint8_t)((r1 + m) * 255.0f + 0.5f);
    rgb.g = (uint8_t)((g1 + m) * 255.0f + 0.5f);
    rgb.b = (uint8_t)((b1 + m) * 255.0f + 0.5f);

    return rgb;
}