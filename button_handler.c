/* button_handler.c
 *
 * Hardware-driven button handling: GPIOTE interrupt on both edges plus
 * app_timer instances for debounce, long-press detection, double-click
 * window and the repeated value change while the button is held.
 */

#include "button_handler.h"
#include "board_config.h"
#include "input_mode.h"

#include <stdbool.h>

#include "nrf_drv_gpiote.h"
#include "app_timer.h"
#include "app_error.h"
#include "nrf_gpio.h"

#define NRF_LOG_MODULE_NAME button
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
NRF_LOG_MODULE_REGISTER();

/* Timing constants (milliseconds). */
#define DEBOUNCE_MS              25
#define DOUBLE_CLICK_WINDOW_MS   400
#define LONG_PRESS_THRESHOLD_MS  500
#define VALUE_CHANGE_PERIOD_MS   20

/* Full sweep (0 -> max -> 0 wrap) takes about 1.8s, which fits the
 * "slowly" requirement (~1.5-2s per full range sweep). */
#define HUE_SWEEP_TIME_MS        1800UL
#define PERCENT_SWEEP_TIME_MS    1800UL

APP_TIMER_DEF(m_debounce_timer);
APP_TIMER_DEF(m_long_press_timer);
APP_TIMER_DEF(m_double_click_timer);
APP_TIMER_DEF(m_value_change_timer);

typedef enum
{
    BTN_IDLE = 0,             /* waiting for a press */
    BTN_DEBOUNCE_PRESS,       /* edge seen, waiting for debounce to settle */
    BTN_PRESSED,              /* press confirmed, waiting for long-press or release */
    BTN_WAIT_SECOND_CLICK,    /* released once, waiting to see if a 2nd click follows */
    BTN_DOUBLE_CLICK_HOLD,    /* double click just switched the mode; wait for release */
    BTN_LONG_PRESS            /* long press active, value_change_timer is running */
} button_state_t;

static button_state_t m_state = BTN_IDLE;
static hsv_t *        m_p_hsv;

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

/* Advances the active HSV parameter (chosen by the current input mode) by
 * an amount proportional to dt_ms, using fixed-point accumulation so that
 * fractional progress is not lost between ticks. Wraps around to 0 after
 * the maximum. Does nothing while in "No input" mode. */
static void advance_active_value(uint32_t dt_ms, hsv_t * p_hsv)
{
    static uint32_t hue_acc_mdeg = 0;
    static uint32_t sat_acc_mpct = 0;
    static uint32_t val_acc_mpct = 0;

    switch (input_mode_get())
    {
        case MODE_HUE:
        {
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
            break;
    }
}

/* --------------------------------------------------------------------- */
/* app_timer handlers                                                    */
/* --------------------------------------------------------------------- */

static void debounce_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    /* Debounce elapsed: accept the press if the pin is still low. */
    if (nrf_gpio_pin_read(BUTTON_1) == 0)
    {
        m_state = BTN_PRESSED;
        APP_ERROR_CHECK(app_timer_start(m_long_press_timer,
                                        APP_TIMER_TICKS(LONG_PRESS_THRESHOLD_MS),
                                        NULL));
    }
    else
    {
        /* It was a glitch, go back to idle. */
        m_state = BTN_IDLE;
    }
}

static void long_press_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    if (m_state == BTN_PRESSED)
    {
        m_state = BTN_LONG_PRESS;
        APP_ERROR_CHECK(app_timer_start(m_value_change_timer,
                                        APP_TIMER_TICKS(VALUE_CHANGE_PERIOD_MS),
                                        NULL));
    }
}

static void double_click_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    /* Window expired: it was a single click, which is ignored. */
    if (m_state == BTN_WAIT_SECOND_CLICK)
    {
        m_state = BTN_IDLE;
    }
}

static void value_change_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    if (m_state == BTN_LONG_PRESS)
    {
        advance_active_value(VALUE_CHANGE_PERIOD_MS, m_p_hsv);
    }
    else
    {
        APP_ERROR_CHECK(app_timer_stop(m_value_change_timer));
    }
}

/* --------------------------------------------------------------------- */
/* GPIOTE interrupt                                                      */
/* --------------------------------------------------------------------- */

static void button_gpiote_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    UNUSED_PARAMETER(action);

    bool pressed = (nrf_gpio_pin_read(pin) == 0);

    if (pressed)
    {
        /* Press (active low). */
        switch (m_state)
        {
            case BTN_IDLE:
                m_state = BTN_DEBOUNCE_PRESS;
                APP_ERROR_CHECK(app_timer_start(m_debounce_timer,
                                                APP_TIMER_TICKS(DEBOUNCE_MS),
                                                NULL));
                break;

            case BTN_WAIT_SECOND_CLICK:
                /* Second click within the window -> double click. */
                APP_ERROR_CHECK(app_timer_stop(m_double_click_timer));
                input_mode_advance();
                NRF_LOG_INFO("Double click -> mode switched to: %s",
                             (uint32_t)mode_name(input_mode_get()));
                //NRF_LOG_FLUSH();
                m_state = BTN_DOUBLE_CLICK_HOLD;
                break;

            default:
                break;
        }
    }
    else /* NRF_GPIOTE_POLARITY_LOTOHI — release */
    {
        switch (m_state)
        {
            case BTN_DEBOUNCE_PRESS:
                /* Released during debounce: glitch. */
                APP_ERROR_CHECK(app_timer_stop(m_debounce_timer));
                m_state = BTN_IDLE;
                break;

            case BTN_PRESSED:
                /* Short click: wait for a possible second one. */
                APP_ERROR_CHECK(app_timer_stop(m_long_press_timer));
                m_state = BTN_WAIT_SECOND_CLICK;
                APP_ERROR_CHECK(app_timer_start(m_double_click_timer,
                                                APP_TIMER_TICKS(DOUBLE_CLICK_WINDOW_MS),
                                                NULL));
                break;

            case BTN_LONG_PRESS:
                APP_ERROR_CHECK(app_timer_stop(m_value_change_timer));
                NRF_LOG_INFO("Long press released in mode %s -> H=%d S=%d V=%d",
                             (uint32_t)mode_name(input_mode_get()),
                             m_p_hsv->hue, m_p_hsv->saturation, m_p_hsv->value);
                //NRF_LOG_FLUSH();
                m_state = BTN_IDLE;
                break;

            case BTN_DOUBLE_CLICK_HOLD:
                /* Release after the double click's second press: back to idle. */
                m_state = BTN_IDLE;
                break;

            default:
                break;
        }
    }
}

/* --------------------------------------------------------------------- */
/* Public API                                                            */
/* --------------------------------------------------------------------- */

void button_handler_init(hsv_t * p_hsv)
{
    ret_code_t err_code;

    m_p_hsv = p_hsv;

    APP_ERROR_CHECK(app_timer_create(&m_debounce_timer,
                                     APP_TIMER_MODE_SINGLE_SHOT,
                                     debounce_handler));
    APP_ERROR_CHECK(app_timer_create(&m_long_press_timer,
                                     APP_TIMER_MODE_SINGLE_SHOT,
                                     long_press_handler));
    APP_ERROR_CHECK(app_timer_create(&m_double_click_timer,
                                     APP_TIMER_MODE_SINGLE_SHOT,
                                     double_click_handler));
    APP_ERROR_CHECK(app_timer_create(&m_value_change_timer,
                                     APP_TIMER_MODE_REPEATED,
                                     value_change_handler));

    if (!nrf_drv_gpiote_is_init())
    {
        err_code = nrf_drv_gpiote_init();
        APP_ERROR_CHECK(err_code);
    }

    nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
    config.pull = NRF_GPIO_PIN_PULLUP;

    err_code = nrf_drv_gpiote_in_init(BUTTON_1, &config, button_gpiote_handler);
    APP_ERROR_CHECK(err_code);
    nrf_drv_gpiote_in_event_enable(BUTTON_1, true);
    NRF_LOG_INFO("button_handler_init done, BUTTON_1 = %d", nrf_gpio_pin_read(BUTTON_1));
    NRF_LOG_FLUSH();
}