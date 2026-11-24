/* button_handler.h
 *
 * Button click / double-click / long-press detection, and the slow
 * value change applied to the active HSV parameter while the button
 * is held down (long press).
 *
 * Behaviour:
 *   - A single short click is ignored.
 *   - A double click (second press within DOUBLE_CLICK_WINDOW_MS of the
 *     first release) advances the input mode (see input_mode.h).
 *   - Holding the button longer than LONG_PRESS_THRESHOLD_MS is a long
 *     press: it slowly increases the value of the currently selected
 *     HSV parameter, wrapping around to 0 after the maximum.
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <stdint.h>

#include "hsv_color.h"

/* Initializes the button pin as an input with an internal pull-up. */
void button_handler_init(void);

/* Runs one step of the button state machine and, while in a long press,
 * advances the given HSV value in place according to the current input
 * mode. Must be called periodically (every dt_ms milliseconds) from the
 * application tick. */
void button_handler_tick(uint32_t dt_ms, hsv_t * p_hsv);

#endif /* BUTTON_HANDLER_H */