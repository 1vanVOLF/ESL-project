/* button_handler.c
 *
 * Implementation of button click/double-click/long-press detection and
 * of the slow HSV value change while the button is held.
 */

#include "button_handler.h"
#include "board_config.h"
#include "input_mode.h"

#include <stdbool.h>

#include "nrf_gpio.h"

#define NRF_LOG_MODULE_NAME button
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
NRF_LOG_MODULE_REGISTER();

static const char * mode_name(input_mode_t mode)
{
    switch (mode)
    {
        case MODE_NONE:       return "No input";
        case MODE_HUE:        return "Hue";
        case MODE_SATURATION: return "Saturation";
        case MODE_BRIGHTNESS: return "Brightness";
        default:              return "Unknown";
    }
}

#define DOUBLE_CLICK_WINDOW_MS      400  /* max gap between two releases to count as double click */
#define LONG_PRESS_THRESHOLD_MS     500  /* holding longer than this starts value change */

/* Full sweep (0 -> max -> 0 wrap) takes about 1.8s, which fits the
 * "slowly" requirement (~1.5-2s per full range sweep). */
#define HUE_SWEEP_TIME_MS           1800UL   /* time to go 0..360 */
#define PERCENT_SWEEP_TIME_MS       1800UL   /* time to go 0..100 */

typedef enum
{
    BTN_IDLE = 0,             /* waiting for a press */
    BTN_PRESSED,              /* button currently down, not yet classified */
    BTN_WAIT_SECOND_CLICK,    /* released once, waiting to see if a 2nd click follows */
    BTN_DOUBLE_CLICK_HOLD,    /* double click just switched the mode; waiting
                                 for the button to be released before a new
                                 long press can start */
    BTN_LONG_PRESS            /* classified as long press, actively changing value */
} button_state_t;

static button_state_t g_button_state = BTN_IDLE;
static uint32_t       g_state_timer_ms = 0;   /* time spent in current button_state */
static bool           g_button_prev_pressed = false;

void button_handler_init(void)
{
    nrf_gpio_cfg_input(BUTTON_1, NRF_GPIO_PIN_PULLUP);
}

/* Advances the active HSV parameter (chosen by the current input mode) by
 * an amount proportional to dt_ms, using fixed-point accumulation so that
 * fractional progress is not lost between ticks. Wraps around to 0 after
 * the maximum. Does nothing while in "No input" mode. */
static void advance_active_value(uint32_t dt_ms, hsv_t * p_hsv)
{
    /* Accumulators keep fractional progress (in milli-units) between calls. */
    static uint32_t hue_acc_mdeg = 0;    /* accumulated milli-degrees */
    static uint32_t sat_acc_mpct = 0;    /* accumulated milli-percent */
    static uint32_t val_acc_mpct = 0;

    switch (input_mode_get())
    {
        case MODE_HUE:
        {
            /* degrees per ms * 1000, to keep integer math precise */
            uint32_t delta_mdeg = (uint32_t)HUE_MAX * 1000UL * dt_ms / HUE_SWEEP_TIME_MS;
            hue_acc_mdeg += delta_mdeg;
            while (hue_acc_mdeg >= 1000UL)
            {
                hue_acc_mdeg -= 1000UL;
                p_hsv->hue = (uint16_t)((p_hsv->hue + 1) % HUE_MAX);
            }
            break;
        }

        case MODE_SATURATION:
        {
            uint32_t delta_mpct = (uint32_t)PERCENT_MAX * 1000UL * dt_ms / PERCENT_SWEEP_TIME_MS;
            sat_acc_mpct += delta_mpct;
            while (sat_acc_mpct >= 1000UL)
            {
                sat_acc_mpct -= 1000UL;
                p_hsv->saturation = (uint8_t)((p_hsv->saturation + 1) % (PERCENT_MAX + 1));
            }
            break;
        }

        case MODE_BRIGHTNESS:
        {
            /* Ping-pongs between 0 and PERCENT_MAX instead of wrapping
             * abruptly back to 0, as a sudden jump in brightness is far
             * more jarring to the eye than a sudden jump in hue or
             * saturation. */
            static bool s_val_rising = true;

            uint32_t delta_mpct = (uint32_t)PERCENT_MAX * 1000UL * dt_ms / PERCENT_SWEEP_TIME_MS;
            val_acc_mpct += delta_mpct;
            while (val_acc_mpct >= 1000UL)
            {
                val_acc_mpct -= 1000UL;

                if (s_val_rising)
                {
                    if (p_hsv->value < PERCENT_MAX)
                    {
                        p_hsv->value++;
                    }
                    else
                    {
                        s_val_rising = false;
                        p_hsv->value--;
                    }
                }
                else
                {
                    if (p_hsv->value > 0)
                    {
                        p_hsv->value--;
                    }
                    else
                    {
                        s_val_rising = true;
                        p_hsv->value++;
                    }
                }
            }
            break;
        }

        case MODE_NONE:
        default:
            /* nothing to change in "no input" mode */
            break;
    }
}

void button_handler_tick(uint32_t dt_ms, hsv_t * p_hsv)
{
    bool pressed = (nrf_gpio_pin_read(BUTTON_1) == 0); /* active low */
    bool just_pressed  = pressed && !g_button_prev_pressed;
    bool just_released = !pressed && g_button_prev_pressed;
    g_button_prev_pressed = pressed;

    g_state_timer_ms += dt_ms;

    switch (g_button_state)
    {
        case BTN_IDLE:
            if (just_pressed)
            {
                g_button_state = BTN_PRESSED;
                g_state_timer_ms = 0;
            }
            break;

        case BTN_PRESSED:
            if (pressed && g_state_timer_ms >= LONG_PRESS_THRESHOLD_MS)
            {
                /* Held long enough: this is a long press, start changing value. */
                g_button_state = BTN_LONG_PRESS;
                g_state_timer_ms = 0;
            }
            else if (just_released)
            {
                /* Released before the long-press threshold: this is a
                 * short click. Wait to see whether a second click follows
                 * (double click) within the allowed window. */
                g_button_state = BTN_WAIT_SECOND_CLICK;
                g_state_timer_ms = 0;
            }
            break;

        case BTN_WAIT_SECOND_CLICK:
            if (just_pressed)
            {
                /* Second click arrived in time -> double click. Switch mode,
                 * then wait for this press to be released before allowing a
                 * new long press to start. This prevents the tail end of
                 * the double click (if the button is held a bit too long)
                 * from being misread as a long press in the mode that was
                 * active a moment ago, which would silently nudge that
                 * parameter before the new mode is even visible. */
                input_mode_advance();
                NRF_LOG_INFO("Double click -> mode switched to: %s",
                             (uint32_t)mode_name(input_mode_get()));
                NRF_LOG_FLUSH();
                g_button_state = BTN_DOUBLE_CLICK_HOLD;
                g_state_timer_ms = 0;
            }
            else if (g_state_timer_ms >= DOUBLE_CLICK_WINDOW_MS)
            {
                /* Window expired: it was a single click, which is ignored. */
                g_button_state = BTN_IDLE;
                g_state_timer_ms = 0;
            }
            break;

        case BTN_DOUBLE_CLICK_HOLD:
            /* Mode has just been switched by a double click. Ignore the
             * button until it is released, so the second click's own
             * press cannot be mistaken for the start of a long press. */
            if (just_released)
            {
                g_button_state = BTN_IDLE;
                g_state_timer_ms = 0;
            }
            break;

        case BTN_LONG_PRESS:
            if (pressed)
            {
                advance_active_value(dt_ms, p_hsv);
            }
            else
            {
                NRF_LOG_INFO("Long press released in mode %s -> H=%d S=%d V=%d",
                             (uint32_t)mode_name(input_mode_get()),
                             p_hsv->hue, p_hsv->saturation, p_hsv->value);
                NRF_LOG_FLUSH();
                g_button_state = BTN_IDLE;
                g_state_timer_ms = 0;
            }
            break;

        default:
            g_button_state = BTN_IDLE;
            break;
    }
}