#include "rgb_led.h"
#include "board_config.h"

#include <stdbool.h>

#include "nrfx_pwm.h"
#include "app_error.h"

#define PWM_TOP_VALUE   255

static nrfx_pwm_t m_pwm0 = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_individual_t m_pwm_seq_values;
static nrf_pwm_sequence_t const    m_pwm_sequence =
{
    .values.p_individual = &m_pwm_seq_values,
    .length              = NRF_PWM_VALUES_LENGTH(m_pwm_seq_values),
    .repeats             = 0,
    .end_delay           = 0
};

void rgb_led_init(void)
{
    nrfx_pwm_config_t const config =
    {
        .output_pins =
        {
            LED2_R | NRFX_PWM_PIN_INVERTED,
            LED2_G | NRFX_PWM_PIN_INVERTED,
            LED2_B | NRFX_PWM_PIN_INVERTED,
            NRFX_PWM_PIN_NOT_USED           
        },
        .irq_priority = APP_IRQ_PRIORITY_LOWEST,
        .base_clock   = NRF_PWM_CLK_1MHz,
        .count_mode   = NRF_PWM_MODE_UP,
        .top_value    = PWM_TOP_VALUE,
        .load_mode    = NRF_PWM_LOAD_INDIVIDUAL,
        .step_mode    = NRF_PWM_STEP_AUTO
    };

    APP_ERROR_CHECK(nrfx_pwm_init(&m_pwm0, &config, NULL));

    m_pwm_seq_values.channel_0 = 0;
    m_pwm_seq_values.channel_1 = 0;
    m_pwm_seq_values.channel_2 = 0;
    m_pwm_seq_values.channel_3 = 0;
}


static void rgb_led_write(rgb_t rgb)
{
    m_pwm_seq_values.channel_0 = rgb.r;
    m_pwm_seq_values.channel_1 = rgb.g;
    m_pwm_seq_values.channel_2 = rgb.b;
    m_pwm_seq_values.channel_3 = 0;

    (void)nrfx_pwm_simple_playback(&m_pwm0, &m_pwm_sequence, 1, NRFX_PWM_FLAG_LOOP);
}

void rgb_led_set_hsv(hsv_t hsv)
{
    static bool  s_have_prev = false;
    static rgb_t s_prev_rgb;

    rgb_t rgb = hsv_to_rgb(hsv.hue, hsv.saturation, hsv.value);

    if (!s_have_prev ||
        rgb.r != s_prev_rgb.r ||
        rgb.g != s_prev_rgb.g ||
        rgb.b != s_prev_rgb.b)
    {
        rgb_led_write(rgb);
        s_prev_rgb = rgb;
        s_have_prev = true;
    }
}