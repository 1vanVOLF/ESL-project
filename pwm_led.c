#include "pwm_led.h"
#include "button.h"
#include "boards.h"
#include "nrf_drv_pwm.h"
#include "nrf_log.h"
#include "app_timer.h"
#include "app_error.h"
#include <string.h>

#define PWM_TOP_VALUE           1000
#define RAMP_STEP_PERCENT       1
#define RAMP_STEP_MS            8
#define NUM_LEDS                4

static uint8_t const m_digit_blinks[NUM_LEDS] = {6, 5, 7, 8};
static uint32_t const m_digit_led_pin[NUM_LEDS] = { LED1_G, LED2_R, LED2_G, LED2_B };

APP_TIMER_DEF(m_ramp_timer);

static nrf_drv_pwm_t const m_pwm0 = NRF_DRV_PWM_INSTANCE(0);
static nrf_pwm_values_individual_t m_pwm_seq_values;
static nrf_pwm_sequence_t const m_pwm_seq = {
    .values.p_individual = &m_pwm_seq_values,
    .length = NRF_PWM_VALUES_LENGTH(m_pwm_seq_values),
    .repeats = 0, .end_delay = 0
};

static uint8_t    m_current_digit_idx    = 0;
static uint8_t    m_blinks_done_on_digit = 0;
static int32_t    m_duty_percent         = 0;
static bool       m_ramp_up              = true;
static bool       m_pwm_pin_connected    = false;

static void pwm_reinit_for_pin(uint32_t led_pin)
{
    nrf_drv_pwm_uninit(&m_pwm0);
    nrf_drv_pwm_config_t const config0 = {
        .output_pins = { led_pin | NRF_DRV_PWM_PIN_INVERTED, NRF_DRV_PWM_PIN_NOT_USED, NRF_DRV_PWM_PIN_NOT_USED, NRF_DRV_PWM_PIN_NOT_USED },
        .irq_priority = APP_IRQ_PRIORITY_LOWEST,
        .base_clock = NRF_PWM_CLK_1MHz,
        .count_mode = NRF_PWM_MODE_UP,
        .top_value = PWM_TOP_VALUE,
        .load_mode = NRF_PWM_LOAD_INDIVIDUAL,
        .step_mode = NRF_PWM_STEP_AUTO
    };
    ret_code_t err_code = nrf_drv_pwm_init(&m_pwm0, &config0, NULL);
    APP_ERROR_CHECK(err_code);
    m_pwm_pin_connected = true;
}

static void pwm_set_duty_percent(int32_t percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    uint16_t compare = (uint16_t)((PWM_TOP_VALUE * percent) / 100);
    m_pwm_seq_values.channel_0 = compare;
    m_pwm_seq_values.channel_1 = compare;
    m_pwm_seq_values.channel_2 = compare;
    m_pwm_seq_values.channel_3 = compare;
    nrf_drv_pwm_simple_playback(&m_pwm0, &m_pwm_seq, 1, NRF_DRV_PWM_FLAG_LOOP);
}

static void ramp_timer_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);

    if (!button_is_sequence_running()) {
        return;
    }

    if (!m_pwm_pin_connected) {
        pwm_reinit_for_pin(m_digit_led_pin[m_current_digit_idx]);
        pwm_set_duty_percent(m_duty_percent);
    }

    if (m_ramp_up) {
        m_duty_percent += RAMP_STEP_PERCENT;
        if (m_duty_percent >= 100) { m_duty_percent = 100; m_ramp_up = false; }
    } else {
        m_duty_percent -= RAMP_STEP_PERCENT;
        if (m_duty_percent <= 0) {
            m_duty_percent = 0;
            m_ramp_up = true;
            m_blinks_done_on_digit++;

            NRF_LOG_INFO("Digit %d: blink %d/%d done",
                         m_current_digit_idx,
                         m_blinks_done_on_digit,
                         m_digit_blinks[m_current_digit_idx]);

            if (m_blinks_done_on_digit >= m_digit_blinks[m_current_digit_idx]) {
                m_blinks_done_on_digit = 0;
                m_current_digit_idx = (m_current_digit_idx + 1) % NUM_LEDS;
                m_duty_percent = 0;
                pwm_reinit_for_pin(m_digit_led_pin[m_current_digit_idx]);
                NRF_LOG_INFO("Digit %d target: %d", m_current_digit_idx, m_digit_blinks[m_current_digit_idx]);
            }
        }
    }
    pwm_set_duty_percent(m_duty_percent);
}

void pwm_led_init(void)
{
    ret_code_t err_code;

    memset(&m_pwm_seq_values, 0, sizeof(m_pwm_seq_values));
    pwm_reinit_for_pin(m_digit_led_pin[m_current_digit_idx]);
    pwm_set_duty_percent(0);

    err_code = app_timer_create(&m_ramp_timer, APP_TIMER_MODE_REPEATED, ramp_timer_handler);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_start(m_ramp_timer, APP_TIMER_TICKS(RAMP_STEP_MS), NULL);
    APP_ERROR_CHECK(err_code);
}