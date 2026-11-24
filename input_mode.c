/* input_mode.c
 *
 * Implementation of input mode cycling and LED1 indication.
 */

#include "input_mode.h"
#include "board_config.h"

#include <stdbool.h>

#include "nrf_gpio.h"

/* LED1 blink periods (half-period, i.e. toggle interval) */
#define LED1_BLINK_SLOW_MS   500   /* Hue mode: slow blink */
#define LED1_BLINK_FAST_MS   120   /* Saturation mode: fast blink */

static input_mode_t g_mode = MODE_NONE;

static bool     g_led1_on = false;
static uint32_t g_led1_timer_ms = 0;

static void led1_set(bool on)
{
    /* active low */
    if (on)
    {
        nrf_gpio_pin_clear(LED1_G);
    }
    else
    {
        nrf_gpio_pin_set(LED1_G);
    }
}

void input_mode_init(void)
{
    nrf_gpio_cfg_output(LED1_G);
    led1_set(false);
}

void input_mode_advance(void)
{
    g_mode = (input_mode_t)((g_mode + 1) % MODE_COUNT);
}

input_mode_t input_mode_get(void)
{
    return g_mode;
}

void input_mode_led_update(uint32_t dt_ms)
{
    switch (g_mode)
    {
        case MODE_NONE:
            led1_set(false);
            g_led1_on = false;
            g_led1_timer_ms = 0;
            break;

        case MODE_BRIGHTNESS:
            led1_set(true);
            g_led1_on = true;
            g_led1_timer_ms = 0;
            break;

        case MODE_HUE:
        case MODE_SATURATION:
        {
            uint32_t half_period = (g_mode == MODE_HUE) ? LED1_BLINK_SLOW_MS
                                                          : LED1_BLINK_FAST_MS;
            g_led1_timer_ms += dt_ms;
            if (g_led1_timer_ms >= half_period)
            {
                g_led1_timer_ms -= half_period;
                g_led1_on = !g_led1_on;
                led1_set(g_led1_on);
            }
            break;
        }

        default:
            break;
    }
}