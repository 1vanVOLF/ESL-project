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

#define DEBOUNCE_MS              25
#define DOUBLE_CLICK_WINDOW_MS   400
#define LONG_PRESS_THRESHOLD_MS  500
#define VALUE_CHANGE_PERIOD_MS   20
#define VALUE_LOG_PERIOD_MS      200

#define HUE_SWEEP_TIME_MS        1800UL
#define PERCENT_SWEEP_TIME_MS    1800UL

APP_TIMER_DEF(m_debounce_timer);
APP_TIMER_DEF(m_long_press_timer);
APP_TIMER_DEF(m_double_click_timer);
APP_TIMER_DEF(m_value_change_timer);

typedef enum
{
    BTN_IDLE = 0,
    BTN_DEBOUNCE_PRESS,
    BTN_PRESSED,
    BTN_WAIT_SECOND_CLICK,
    BTN_DOUBLE_CLICK_HOLD,
    BTN_LONG_PRESS
} button_state_t;

static button_state_t m_state = BTN_IDLE;
static hsv_t *m_p_hsv;
static uint32_t m_value_log_acc_ms = 0;

static const char *mode_name(input_mode_t mode)
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

static void advance_active_value(uint32_t dt_ms, hsv_t *p_hsv)
{
    static uint32_t hue_acc = 0;
    static uint32_t sat_acc = 0;
    static uint32_t val_acc = 0;

    switch (input_mode_get())
    {
        case MODE_HUE:
        {
            hue_acc += (uint32_t)HUE_MAX * 1000UL * dt_ms / HUE_SWEEP_TIME_MS;
            while (hue_acc >= 1000UL)
            {
                hue_acc -= 1000UL;
                p_hsv->hue = (uint16_t)((p_hsv->hue + 1) % HUE_MAX);
            }
            break;
        }
        case MODE_SATURATION:
        {
            sat_acc += (uint32_t)PERCENT_MAX * 1000UL * dt_ms / PERCENT_SWEEP_TIME_MS;
            while (sat_acc >= 1000UL)
            {
                sat_acc -= 1000UL;
                p_hsv->saturation = (uint8_t)((p_hsv->saturation + 1) % (PERCENT_MAX + 1));
            }
            break;
        }
        case MODE_BRIGHTNESS:
        {
            static bool rising = true;

            val_acc += (uint32_t)PERCENT_MAX * 1000UL * dt_ms / PERCENT_SWEEP_TIME_MS;
            while (val_acc >= 1000UL)
            {
                val_acc -= 1000UL;
                if (rising)
                {
                    if (p_hsv->value < PERCENT_MAX) p_hsv->value++;
                    else { rising = false; p_hsv->value--; }
                }
                else
                {
                    if (p_hsv->value > 0) p_hsv->value--;
                    else { rising = true; p_hsv->value++; }
                }
            }
            break;
        }
        default:
            break;
    }
}

static void debounce_handler(void *p_context)
{
    (void)p_context;
    if (nrf_gpio_pin_read(BUTTON_1) == 0)
    {
        m_state = BTN_PRESSED;
        APP_ERROR_CHECK(app_timer_start(m_long_press_timer, APP_TIMER_TICKS(LONG_PRESS_THRESHOLD_MS), NULL));
    }
    else
    {
        m_state = BTN_IDLE;
    }
}

static void long_press_handler(void *p_context)
{
    (void)p_context;
    if (m_state == BTN_PRESSED)
    {
        m_state = BTN_LONG_PRESS;
        if (input_mode_get() != MODE_NONE)
        {
            m_value_log_acc_ms = 0;
            APP_ERROR_CHECK(app_timer_start(m_value_change_timer, APP_TIMER_TICKS(VALUE_CHANGE_PERIOD_MS), NULL));
        }
    }
}

static void double_click_handler(void *p_context)
{
    (void)p_context;
    if (m_state == BTN_WAIT_SECOND_CLICK)
    {
        m_state = BTN_IDLE;
    }
}

static void value_change_handler(void *p_context)
{
    (void)p_context;
    if (m_state == BTN_LONG_PRESS)
    {
        advance_active_value(VALUE_CHANGE_PERIOD_MS, m_p_hsv);
        m_value_log_acc_ms += VALUE_CHANGE_PERIOD_MS;
        if (m_value_log_acc_ms >= VALUE_LOG_PERIOD_MS)
        {
            m_value_log_acc_ms = 0;
            NRF_LOG_INFO("mode %s: H=%d S=%d V=%d", (uint32_t)mode_name(input_mode_get()), m_p_hsv->hue, m_p_hsv->saturation, m_p_hsv->value);
        }
    }
    else
    {
        m_value_log_acc_ms = 0;
        (void)app_timer_stop(m_value_change_timer);
    }
}

static void button_gpiote_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    (void)action;
    bool pressed = (nrf_gpio_pin_read(pin) == 0);

    if (pressed)
    {
        switch (m_state)
        {
            case BTN_IDLE:
                m_state = BTN_DEBOUNCE_PRESS;
                APP_ERROR_CHECK(app_timer_start(m_debounce_timer, APP_TIMER_TICKS(DEBOUNCE_MS), NULL));
                break;
            case BTN_WAIT_SECOND_CLICK:
                (void)app_timer_stop(m_double_click_timer);
                input_mode_advance();
                NRF_LOG_INFO("Double click -> mode switched to: %s", (uint32_t)mode_name(input_mode_get()));
                m_state = BTN_DOUBLE_CLICK_HOLD;
                break;
            default:
                break;
        }
    }
    else
    {
        switch (m_state)
        {
            case BTN_DEBOUNCE_PRESS:
                (void)app_timer_stop(m_debounce_timer);
                m_state = BTN_IDLE;
                break;
            case BTN_PRESSED:
                (void)app_timer_stop(m_long_press_timer);
                m_state = BTN_WAIT_SECOND_CLICK;
                APP_ERROR_CHECK(app_timer_start(m_double_click_timer, APP_TIMER_TICKS(DOUBLE_CLICK_WINDOW_MS), NULL));
                break;
            case BTN_LONG_PRESS:
                (void)app_timer_stop(m_value_change_timer);
                NRF_LOG_INFO("Long press released in mode %s -> H=%d S=%d V=%d", (uint32_t)mode_name(input_mode_get()), m_p_hsv->hue, m_p_hsv->saturation, m_p_hsv->value);
                m_state = BTN_IDLE;
                break;
            case BTN_DOUBLE_CLICK_HOLD:
                m_state = BTN_IDLE;
                break;
            default:
                break;
        }
    }
}

void button_handler_init(hsv_t *p_hsv)
{
    m_p_hsv = p_hsv;

    APP_ERROR_CHECK(app_timer_create(&m_debounce_timer, APP_TIMER_MODE_SINGLE_SHOT, debounce_handler));
    APP_ERROR_CHECK(app_timer_create(&m_long_press_timer, APP_TIMER_MODE_SINGLE_SHOT, long_press_handler));
    APP_ERROR_CHECK(app_timer_create(&m_double_click_timer, APP_TIMER_MODE_SINGLE_SHOT, double_click_handler));
    APP_ERROR_CHECK(app_timer_create(&m_value_change_timer, APP_TIMER_MODE_REPEATED, value_change_handler));

    if (!nrf_drv_gpiote_is_init())
    {
        ret_code_t err = nrf_drv_gpiote_init();
        APP_ERROR_CHECK(err);
    }

    nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
    config.pull = NRF_GPIO_PIN_PULLUP;

    ret_code_t err = nrf_drv_gpiote_in_init(BUTTON_1, &config, button_gpiote_handler);
    APP_ERROR_CHECK(err);
    nrf_drv_gpiote_in_event_enable(BUTTON_1, true);
    NRF_LOG_INFO("button_handler_init done, BUTTON_1 = %d", nrf_gpio_pin_read(BUTTON_1));
    NRF_LOG_FLUSH();
}