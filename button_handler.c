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

#define DOUBLE_CLICK_WINDOW_MS      400
#define LONG_PRESS_THRESHOLD_MS     500


#define HUE_SWEEP_TIME_MS           1800UL
#define PERCENT_SWEEP_TIME_MS       1800UL

typedef enum
{
    BTN_IDLE = 0,             
    BTN_PRESSED,              
    BTN_WAIT_SECOND_CLICK,    
    BTN_DOUBLE_CLICK_HOLD,    
    BTN_LONG_PRESS            
} button_state_t;

static button_state_t g_button_state = BTN_IDLE;
static uint32_t       g_state_timer_ms = 0;   
static bool           g_button_prev_pressed = false;

void button_handler_init(void)
{
    nrf_gpio_cfg_input(BUTTON_1, NRF_GPIO_PIN_PULLUP);
}


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

void button_handler_tick(uint32_t dt_ms, hsv_t * p_hsv)
{
    bool pressed = (nrf_gpio_pin_read(BUTTON_1) == 0);
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
                g_button_state = BTN_LONG_PRESS;
                g_state_timer_ms = 0;
            }
            else if (just_released)
            {

                g_button_state = BTN_WAIT_SECOND_CLICK;
                g_state_timer_ms = 0;
            }
            break;

        case BTN_WAIT_SECOND_CLICK:
            if (just_pressed)
            {
                input_mode_advance();
                NRF_LOG_INFO("Double click -> mode switched to: %s",
                             (uint32_t)mode_name(input_mode_get()));
                NRF_LOG_FLUSH();
                g_button_state = BTN_DOUBLE_CLICK_HOLD;
                g_state_timer_ms = 0;
            }
            else if (g_state_timer_ms >= DOUBLE_CLICK_WINDOW_MS)
            {
                g_button_state = BTN_IDLE;
                g_state_timer_ms = 0;
            }
            break;

        case BTN_DOUBLE_CLICK_HOLD:
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