#ifndef APP_TIMER_TICK_H
#define APP_TIMER_TICK_H

#include <stdint.h>

#define APP_TIMER_TICK_MS   20

typedef void (* app_tick_callback_t)(uint32_t dt_ms);


void app_timer_tick_init(app_tick_callback_t callback);

#endif