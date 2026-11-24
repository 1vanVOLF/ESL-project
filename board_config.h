/* board_config.h
 *
 * Pin assignment for the nRF52840 Dongle (PCA10059) and the default
 * HSV values derived from the device ID, used across the whole project.
 *
 * All LEDs on this board are active-low (common anode): driving a pin low
 * turns the corresponding LED on.
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "nrf_gpio.h"

/* ------------------------------------------------------------------------ */
/* Pin definitions                                                           */
/* ------------------------------------------------------------------------ */

#define LED1_G      NRF_GPIO_PIN_MAP(0, 6)   /* input-mode indicator LED */
#define LED2_R      NRF_GPIO_PIN_MAP(0, 8)   /* RGB LED2, red channel    */
#define LED2_G      NRF_GPIO_PIN_MAP(1, 9)   /* RGB LED2, green channel  */
#define LED2_B      NRF_GPIO_PIN_MAP(0, 12)  /* RGB LED2, blue channel   */
#define BUTTON_1    NRF_GPIO_PIN_MAP(1, 6)   /* SW1, active low, pull-up */

/* ------------------------------------------------------------------------ */
/* Device ID based default Hue/Saturation/Value                             */
/* ------------------------------------------------------------------------ */

#define DEVICE_ID           6578

/* Default Hue = 360 * (last two digits of device ID) / 100, rounded down.
 * For DEVICE_ID = 6578: last two digits = 78 -> 360 * 0.78 = 280.8 -> 280 */
#define DEFAULT_HUE_DEG     ((uint16_t)(360UL * (DEVICE_ID % 100) / 100UL))
#define DEFAULT_SATURATION  100
#define DEFAULT_VALUE       100

#endif /* BOARD_CONFIG_H */