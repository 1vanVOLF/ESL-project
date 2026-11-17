/**
 * Lab 4 — PWM-based LED sequence with GPIOTE button handling.
 *
 * Dongle ID: 6578  ->  LED blink counts per digit: 6, 5, 7, 8
 * (same per-digit "blink N times" sequence idea as lab 2, now driven by PWM)
 *
 * Behaviour:
 *   - A single logical "sequence" step = one currently-active LED whose
 *     duty cycle ramps smoothly 0% -> 100% -> 0% using the PWM peripheral
 *     at 1 kHz carrier frequency.
 *   - Which LED is active, and how many times it blinks, follows the
 *     digit sequence of the dongle ID 6,5,7,8 (each digit's LED blinks
 *     that many smooth up/down cycles before moving to the next digit).
 *   - Double-click on the button toggles the whole sequence on/off.
 *       * Not blinking + double-click -> start smooth blinking
 *       * Blinking + double-click     -> freeze: LED holds its current
 *                                         duty cycle until toggled again
 *   - GPIOTE is used to detect button edges; a simple software
 *     double-click detector (two presses within a time window) runs via
 *     an app_timer single-shot timer.
 *   - Optional log messages go out over the project's existing USB CDC
 *     ACM log backend (LOG_BACKEND_USB_ENABLED in app_config.h), the
 *     same one used by the dongle_logs sample: NRF_LOG_DEFAULT_BACKENDS_INIT()
 *     wires it up automatically, so plain NRF_LOG_INFO() calls just work
 *     and show up on /dev/ttyACM0.
 *
 * Board: PCA10059 (nRF52840 dongle)
 *   Button: BSP_BUTTON_0 (SW1) is the only user button on the dongle.
 *   LEDs:   the standard PCA10059 board file exposes one RGB LED
 *           (LED_R / LED_G / LED_B). Since lab 4 wants one LED per
 *           digit of a 4-digit ID, the 4th digit reuses LED_R.
 *           If your actual lab 2 setup drives 4 separate LEDs (e.g.
 *           external LEDs on spare GPIOs), change LED_PIN_0..3 below.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nrf.h"
#include "nrf_drv_pwm.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_clock.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "boards.h"
#include "app_error.h"
#include "app_timer.h"
#include "app_usbd.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

/* ------------------------------------------------------------------- */
/*  Configuration                                                       */
/* ------------------------------------------------------------------- */

/* Device id ABCD = 6578 -> one blink-count per digit. */
#define DEVICE_ID_DIGIT_COUNT   4
static uint8_t const m_digit_blinks[DEVICE_ID_DIGIT_COUNT] = {6, 5, 7, 8};

/* Map each digit position to the LED that represents it. See file header
 * for why LED_R is reused for the 4th digit on the stock PCA10059 board. */
#if defined(BOARD_PCA10059)
    #define LED_PIN_0   LED1_G
    #define LED_PIN_1   LED2_R
    #define LED_PIN_2   LED2_G
    #define LED_PIN_3   LED2_B   /* reused: dongle only has one RGB LED */
#else
    #define LED_PIN_0   BSP_LED_0
    #define LED_PIN_1   BSP_LED_1
    #define LED_PIN_2   BSP_LED_2
    #define LED_PIN_3   BSP_LED_3
#endif

static uint32_t const m_digit_led_pin[DEVICE_ID_DIGIT_COUNT] =
{
    LED_PIN_0, LED_PIN_1, LED_PIN_2, LED_PIN_3
};

/* PWM carrier frequency: 1 kHz, as required. Base clock 1 MHz / top 1000. */
#define PWM_TOP_VALUE           1000    /* gives exactly 1 kHz with 1 MHz base clock */

/* Smooth ramp timing: how long one 0%->100%->0% cycle takes. Tune freely. */
#define RAMP_STEP_MS            15      /* time between duty-cycle steps   */
#define RAMP_STEP_PERCENT       1       /* duty cycle change per step (%)  */

/* Double-click detection window (ms). Two presses closer together than
 * this count as a double-click; otherwise the first press is discarded
 * as a single click (ignored, since lab 4 only defines double-click). */
#define DOUBLE_CLICK_WINDOW_MS  400

APP_TIMER_DEF(m_ramp_timer);
APP_TIMER_DEF(m_click_window_timer);

/* ------------------------------------------------------------------- */
/*  State                                                                */
/* ------------------------------------------------------------------- */

static nrf_drv_pwm_t const m_pwm0 = NRF_DRV_PWM_INSTANCE(0);

/* Single-channel individual duty cycle value played back on loop.
 * NRF_DRV_PWM_PIN_INVERTED on the output pin gives correct polarity for
 * an active-low LED, so 0 = 0% duty and PWM_TOP_VALUE = 100% duty here. */
static nrf_pwm_values_individual_t m_pwm_seq_values;
static nrf_pwm_sequence_t const m_pwm_seq =
{
    .values.p_individual = &m_pwm_seq_values,
    .length               = NRF_PWM_VALUES_LENGTH(m_pwm_seq_values),
    .repeats              = 0,
    .end_delay            = 0
};

typedef enum
{
    RAMP_UP,
    RAMP_DOWN
} ramp_dir_t;

static volatile bool m_blinking_active   = false; /* toggled by double-click */
static volatile bool m_pwm_pin_connected = false; /* is a pin currently driven */

static uint8_t    m_current_digit_idx    = 0;  /* which digit of 6,5,7,8 we're on */
static uint8_t    m_blinks_done_on_digit = 0;  /* how many full ramps done on it  */
static ramp_dir_t m_ramp_dir             = RAMP_UP;
static int32_t    m_duty_percent         = 0;  /* 0..100, current duty cycle      */

/* Double-click bookkeeping */
static volatile uint8_t m_click_count       = 0;
static volatile bool    m_click_window_open = false;

/* ------------------------------------------------------------------- */
/*  PWM helpers                                                          */
/* ------------------------------------------------------------------- */

/* Re-init PWM so only the LED pin for the current digit is driven.
 * Only channel 0 is used; the other three stay NOT_USED. */
static void pwm_reinit_for_pin(uint32_t led_pin)
{
    nrf_drv_pwm_uninit(&m_pwm0);

    nrf_drv_pwm_config_t const config0 =
    {
        .output_pins =
        {
            led_pin | NRF_DRV_PWM_PIN_INVERTED,  /* channel 0: active-low LED */
            NRF_DRV_PWM_PIN_NOT_USED,            /* channel 1 */
            NRF_DRV_PWM_PIN_NOT_USED,            /* channel 2 */
            NRF_DRV_PWM_PIN_NOT_USED,            /* channel 3 */
        },
        .irq_priority = APP_IRQ_PRIORITY_LOWEST,
        .base_clock   = NRF_PWM_CLK_1MHz,
        .count_mode   = NRF_PWM_MODE_UP,
        .top_value    = PWM_TOP_VALUE,
        .load_mode    = NRF_PWM_LOAD_INDIVIDUAL,
        .step_mode    = NRF_PWM_STEP_AUTO
    };

    ret_code_t err_code = nrf_drv_pwm_init(&m_pwm0, &config0, NULL);
    APP_ERROR_CHECK(err_code);

    m_pwm_pin_connected = true;
}

/* Convert 0..100 percent duty cycle into the 0..PWM_TOP_VALUE compare
 * value used by the individual channel. */
static void pwm_set_duty_percent(int32_t percent)
{
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;

    uint16_t compare = (uint16_t)((PWM_TOP_VALUE * percent) / 100);

    m_pwm_seq_values.channel_0 = compare;
    m_pwm_seq_values.channel_1 = compare;
    m_pwm_seq_values.channel_2 = compare;
    m_pwm_seq_values.channel_3 = compare;

    /* NRF_DRV_PWM_FLAG_LOOP keeps re-playing the single updated value so
     * the peripheral itself maintains the output between our ramp steps. */
    nrf_drv_pwm_simple_playback(&m_pwm0, &m_pwm_seq, 1, NRF_DRV_PWM_FLAG_LOOP);
}

/* ------------------------------------------------------------------- */
/*  Sequence state machine (called from ramp timer)                     */
/* ------------------------------------------------------------------- */

static void advance_to_next_digit(void)
{
    m_blinks_done_on_digit = 0;
    m_current_digit_idx = (uint8_t)((m_current_digit_idx + 1) % DEVICE_ID_DIGIT_COUNT);
    m_ramp_dir     = RAMP_UP;
    m_duty_percent = 0;

    pwm_reinit_for_pin(m_digit_led_pin[m_current_digit_idx]);
    pwm_set_duty_percent(m_duty_percent);

    NRF_LOG_INFO("Digit %d (LED pin %d): blink target = %d",
                 m_current_digit_idx,
                 m_digit_led_pin[m_current_digit_idx],
                 m_digit_blinks[m_current_digit_idx]);
}

static void ramp_timer_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    if (!m_blinking_active)
    {
        /* Frozen: keep current duty cycle, do nothing. */
        return;
    }

    if (!m_pwm_pin_connected)
    {
        pwm_reinit_for_pin(m_digit_led_pin[m_current_digit_idx]);
        pwm_set_duty_percent(m_duty_percent);
    }

    if (m_ramp_dir == RAMP_UP)
    {
        m_duty_percent += RAMP_STEP_PERCENT;
        if (m_duty_percent >= 100)
        {
            m_duty_percent = 100;
            m_ramp_dir = RAMP_DOWN;
        }
    }
    else /* RAMP_DOWN */
    {
        m_duty_percent -= RAMP_STEP_PERCENT;
        if (m_duty_percent <= 0)
        {
            m_duty_percent = 0;
            m_ramp_dir = RAMP_UP;

            /* One full 0->100->0 cycle finished. */
            m_blinks_done_on_digit++;
            NRF_LOG_INFO("Digit %d: blink %d/%d done",
                         m_current_digit_idx,
                         m_blinks_done_on_digit,
                         m_digit_blinks[m_current_digit_idx]);

            if (m_blinks_done_on_digit >= m_digit_blinks[m_current_digit_idx])
            {
                advance_to_next_digit();
                return;
            }
        }
    }

    pwm_set_duty_percent(m_duty_percent);
}

/* ------------------------------------------------------------------- */
/*  Double-click detection                                              */
/* ------------------------------------------------------------------- */

static void toggle_blinking(void)
{
    m_blinking_active = !m_blinking_active;

    if (m_blinking_active)
    {
        NRF_LOG_INFO("Double-click: START blinking");
        if (!m_pwm_pin_connected)
        {
            pwm_reinit_for_pin(m_digit_led_pin[m_current_digit_idx]);
            pwm_set_duty_percent(m_duty_percent);
        }
    }
    else
    {
        NRF_LOG_INFO("Double-click: FREEZE blinking (duty = %d%%)", m_duty_percent);
        /* Nothing else to do: ramp_timer_handler() checks m_blinking_active
         * and simply stops updating the duty cycle, which stays as-is
         * since the PWM peripheral keeps replaying the last value. */
    }
}

static void click_window_timer_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    m_click_window_open = false;

    if (m_click_count >= 2)
    {
        toggle_blinking();
    }
    /* A single click within the window is intentionally ignored:
     * this lab only defines double-click behaviour. */

    m_click_count = 0;
}

/* Called from GPIOTE interrupt context on every button press (falling
 * edge, since nRF52840 dongle buttons are active-low with pull-up). */
static void button_event_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    UNUSED_PARAMETER(pin);
    UNUSED_PARAMETER(action);

    m_click_count++;

    if (!m_click_window_open)
    {
        m_click_window_open = true;
        ret_code_t err_code = app_timer_start(
            m_click_window_timer,
            APP_TIMER_TICKS(DOUBLE_CLICK_WINDOW_MS),
            NULL);
        APP_ERROR_CHECK(err_code);
    }
}

/* ------------------------------------------------------------------- */
/*  Init helpers                                                        */
/* ------------------------------------------------------------------- */

static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    /* Reads LOG_BACKEND_USB_ENABLED etc. from app_config.h and wires up
     * the project's USB CDC ACM log backend automatically, including
     * starting the USB stack (LOG_BACKEND_USB_INIT_STACK). */
    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static void clock_init(void)
{
    ret_code_t err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);

    /* Low-frequency clock is required for app_timer. */
    nrf_drv_clock_lfclk_request(NULL);

    while (!nrf_drv_clock_lfclk_is_running())
    {
        /* wait */
    }
}

static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_ramp_timer, APP_TIMER_MODE_REPEATED, ramp_timer_handler);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_click_window_timer, APP_TIMER_MODE_SINGLE_SHOT, click_window_timer_handler);
    APP_ERROR_CHECK(err_code);
}

static void gpiote_init(void)
{
    ret_code_t err_code;

    if (!nrf_drv_gpiote_is_init())
    {
        err_code = nrf_drv_gpiote_init();
        APP_ERROR_CHECK(err_code);
    }

    /* High-accuracy sense on falling edge: button pressed pulls pin low. */
    nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    config.pull = NRF_GPIO_PIN_PULLUP;

    err_code = nrf_drv_gpiote_in_init(BSP_BUTTON_0, &config, button_event_handler);
    APP_ERROR_CHECK(err_code);

    nrf_drv_gpiote_in_event_enable(BSP_BUTTON_0, true);
}

static void pwm_init(void)
{
    memset(&m_pwm_seq_values, 0, sizeof(m_pwm_seq_values));
    pwm_reinit_for_pin(m_digit_led_pin[m_current_digit_idx]);
    pwm_set_duty_percent(0);
}

/* ------------------------------------------------------------------- */
/*  Main                                                                 */
/* ------------------------------------------------------------------- */

int main(void)
{
    clock_init();
    log_init();
    timers_init();
    gpiote_init();
    pwm_init();

    NRF_LOG_INFO("Lab4 started. Device id digits: 6,5,7,8");
    NRF_LOG_INFO("Double-click the button to start/stop smooth PWM blinking.");
    NRF_LOG_FLUSH();

    ret_code_t err_code = app_timer_start(m_ramp_timer, APP_TIMER_TICKS(RAMP_STEP_MS), NULL);
    APP_ERROR_CHECK(err_code);

    while (true)
    {
        /* Drives both the USB CDC ACM log backend's event queue and, via
         * LOG_BACKEND_USB_UTILIZE_POWER_EVENTS, USB connect/disconnect
         * handling; see this repo's own dongle_logs sample for the same
         * pattern. */
        while (app_usbd_event_queue_process())
        {
        }

        NRF_LOG_FLUSH();
        __WFE();
    }
}