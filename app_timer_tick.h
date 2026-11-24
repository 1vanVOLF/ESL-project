/* app_timer_tick.h
 *
 * Starts the low-frequency clock and a repeating application timer that
 * drives the whole application logic (button state machine, LED1
 * indication, RGB LED update) at a fixed period.
 */

#ifndef APP_TIMER_TICK_H
#define APP_TIMER_TICK_H

#include <stdint.h>

#define APP_TIMER_TICK_MS   10   /* periodic application tick period, in ms */

/* Signature of the function called on every application tick. */
typedef void (* app_tick_callback_t)(uint32_t dt_ms);

/* Starts the LFCLK and the repeating application timer, which will call
 * the given callback every APP_TIMER_TICK_MS milliseconds. */
void app_timer_tick_init(app_tick_callback_t callback);

#endif /* APP_TIMER_TICK_H */