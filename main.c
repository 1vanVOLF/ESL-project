#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"

#define LED1_G    NRF_GPIO_PIN_MAP(0, 6)
#define LED2_R    NRF_GPIO_PIN_MAP(0, 8)
#define LED3_G    NRF_GPIO_PIN_MAP(1, 9)
#define LED4_B    NRF_GPIO_PIN_MAP(0, 12)
#define BUTTON_1  NRF_GPIO_PIN_MAP(1, 6)

#define BLINK_TIME_MS 300
#define BLINK_OFF_MS  300
#define PAUSE_TIME_MS 500
#define TICK_MS       10

#define LEDS_NUMBER 4
const uint32_t leds[LEDS_NUMBER]   = { LED1_G, LED2_R, LED3_G, LED4_B };
const uint8_t  counts[LEDS_NUMBER] = { 6, 5, 7, 8 };

int current_led_id = 0;
int blinks_done = 0;
uint32_t timer_ms = 0;
bool led_is_on = false;
bool is_paused = false;


void leds_off_all(void)
{
    nrf_gpio_pin_set(LED1_G);
    nrf_gpio_pin_set(LED2_R);
    nrf_gpio_pin_set(LED3_G);
    nrf_gpio_pin_set(LED4_B);
}

void hardware_init(void)
{
    nrf_gpio_cfg_output(LED1_G);
    nrf_gpio_cfg_output(LED2_R);
    nrf_gpio_cfg_output(LED3_G);
    nrf_gpio_cfg_output(LED4_B);
    leds_off_all();
    
    nrf_gpio_cfg_input(BUTTON_1, NRF_GPIO_PIN_PULLUP);
}

void led_blink(void)
{
    // пауза между сменой цветов
    if (is_paused) 
    {
        timer_ms += TICK_MS;
        if (timer_ms >= PAUSE_TIME_MS) 
        {
            is_paused = false;
            timer_ms = 0;
            nrf_gpio_pin_clear(leds[current_led_id]);
            led_is_on = true;
        }
        return;
    }

    // в процесее мигания
    if (led_is_on) 
    {
        timer_ms += TICK_MS;
        
        if (timer_ms >= BLINK_TIME_MS) 
        {
            nrf_gpio_pin_set(leds[current_led_id]);
            led_is_on = false;
            timer_ms = 0;
            blinks_done++;

            if (blinks_done == counts[current_led_id]) 
            {
                blinks_done = 0;
                current_led_id = (current_led_id + 1) % LEDS_NUMBER;
                is_paused = true;
            }
        }
    } 
    else // между 2 вспышками
    {
        timer_ms += TICK_MS;
        
        if (timer_ms >= BLINK_OFF_MS) 
        {
            nrf_gpio_pin_clear(leds[current_led_id]);
            led_is_on = true;
            timer_ms = 0;
        }
    }
}

int main(void)
{
    hardware_init();

    while (true)
    {
        if (nrf_gpio_pin_read(BUTTON_1) == 0)
        {
            led_blink();
        }
        nrf_delay_ms(TICK_MS);
    }
}