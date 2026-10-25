#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "boards.h"

/* Device ID: 6578 -> LED1 x6, red x5, green x7, blue x8 */
#define DEVICE_ID_DIGITS   4
#define BLINK_ON_MS        300
#define BLINK_OFF_MS       300
#define PAUSE_BETWEEN_MS   1000

static const uint8_t blink_count[DEVICE_ID_DIGITS] = {6, 5, 7, 8};

static void blink_led(uint32_t led, uint8_t times)
{
    for (uint8_t i = 0; i < times; i++)
    {
        bsp_board_led_on(led);
        nrf_delay_ms(BLINK_ON_MS);
        bsp_board_led_off(led);
        nrf_delay_ms(BLINK_OFF_MS);
    }
}

int main(void)
{
    bsp_board_init(BSP_INIT_LEDS);

    while (true)
    {
        for (uint32_t led = 0; led < DEVICE_ID_DIGITS; led++)
        {
            blink_led(led, blink_count[led]);
            nrf_delay_ms(PAUSE_BETWEEN_MS);
        }
    }
}