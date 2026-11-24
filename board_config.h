#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "nrf_gpio.h"

#define LED1_G      NRF_GPIO_PIN_MAP(0, 6)
#define LED2_R      NRF_GPIO_PIN_MAP(0, 8)
#define LED2_G      NRF_GPIO_PIN_MAP(1, 9)
#define LED2_B      NRF_GPIO_PIN_MAP(0, 12)
#define BUTTON_1    NRF_GPIO_PIN_MAP(1, 6)

#define DEVICE_ID           6578

#define DEFAULT_HUE_DEG     ((uint16_t)(360UL * (DEVICE_ID % 100) / 100UL))
#define DEFAULT_SATURATION  100
#define DEFAULT_VALUE       100

#endif