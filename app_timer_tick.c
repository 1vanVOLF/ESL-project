/* app_timer_tick.c
 *
 * Implementation of the app_timer based periodic application tick.
 * LFCLK and app_timer must be initialized in main() before calling this.
 */

#include "app_timer_tick.h"

#include "app_timer.h"
#include "app_error.h"

APP_TIMER_DEF(m_app_tick_timer);

static app_tick_callback_t m_callback;

static void app_tick_handler(void * p_context)
{
    (void)p_context;

    if (m_callback != NULL)
    {
        m_callback(APP_TIMER_TICK_MS);
    }
}

void app_timer_tick_init(app_tick_callback_t callback)
{
    m_callback = callback;

    APP_ERROR_CHECK(app_timer_create(&m_app_tick_timer,
                                      APP_TIMER_MODE_REPEATED,
                                      app_tick_handler));
    APP_ERROR_CHECK(app_timer_start(m_app_tick_timer,
                                     APP_TIMER_TICKS(APP_TIMER_TICK_MS),
                                     NULL));
}