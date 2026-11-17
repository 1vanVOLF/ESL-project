#include "button.h"
#include "boards.h"
#include "app_error.h"
#include "app_timer.h"
#include "nrf_drv_gpiote.h"
#include "nrf_log.h"

#define DOUBLE_CLICK_WINDOW_MS  400

APP_TIMER_DEF(m_click_window_timer);

static volatile uint8_t m_click_count       = 0;
static volatile bool    m_click_window_open = false;
static volatile bool    m_blinking_active   = false;

static void click_window_timer_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    m_click_window_open = false;

    if (m_click_count >= 2) {
        m_blinking_active = !m_blinking_active;
        if (m_blinking_active) {
            NRF_LOG_INFO("Double-click: START blinking");
        } else {
            NRF_LOG_INFO("Double-click: FREEZE blinking");
        }
    }
    m_click_count = 0;
}

static void button_event_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    UNUSED_PARAMETER(pin);
    UNUSED_PARAMETER(action);

    m_click_count++;
    if (!m_click_window_open) {
        m_click_window_open = true;
        app_timer_start(m_click_window_timer, APP_TIMER_TICKS(DOUBLE_CLICK_WINDOW_MS), NULL);
    }
}

void button_init(void)
{
    ret_code_t err_code;

    err_code = app_timer_create(&m_click_window_timer, APP_TIMER_MODE_SINGLE_SHOT, click_window_timer_handler);
    APP_ERROR_CHECK(err_code);

    if (!nrf_drv_gpiote_is_init()) {
        err_code = nrf_drv_gpiote_init();
        APP_ERROR_CHECK(err_code);
    }

    nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    config.pull = NRF_GPIO_PIN_PULLUP;

    err_code = nrf_drv_gpiote_in_init(BSP_BUTTON_0, &config, button_event_handler);
    APP_ERROR_CHECK(err_code);
    nrf_drv_gpiote_in_event_enable(BSP_BUTTON_0, true);
}

bool button_is_sequence_running(void)
{
    return m_blinking_active;
}