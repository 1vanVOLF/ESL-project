#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <stdint.h>

#include "hsv_color.h"

void button_handler_init(void);

void button_handler_tick(uint32_t dt_ms, hsv_t * p_hsv);

#endif