#include <stdbool.h>
#include "nrf_drv_clock.h"
#include "app_error.h"
#include "app_timer.h"

#include "board_config.h"
#include "hsv_color.h"
#include "rgb_led.h"
#include "input_mode.h"
#include "button_handler.h"
#include "app_timer_tick.h"
#include "logs.h"
#include "nrf_log_ctrl.h"

static hsv_t g_hsv = {
    .hue        = DEFAULT_HUE_DEG,
    .saturation = DEFAULT_SATURATION,
    .value      = DEFAULT_VALUE
};

static void clock_init(void)
{
    ret_code_t err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);
    nrf_drv_clock_lfclk_request(NULL);
    while (!nrf_drv_clock_lfclk_is_running()) { __WFE(); }
}

static void on_app_tick(uint32_t dt_ms)
{
    button_handler_tick(dt_ms, &g_hsv);
    input_mode_led_update(dt_ms);
    rgb_led_set_hsv(g_hsv);
    NRF_LOG_FLUSH(); // Принудительно сбрасываем логи каждый тик
}

int main(void)
{
    ret_code_t err_code;

    clock_init();                 // 1. LFCLK

    err_code = app_timer_init();  // 2. app_timer
    APP_ERROR_CHECK(err_code);

    logs_init();                  // 3. USB логи

    input_mode_init();
    button_handler_init();
    rgb_led_init();

    rgb_led_set_hsv(g_hsv);

    app_timer_tick_init(on_app_tick);

    while (true)
    {
        logs_process();
        __WFE();
    }
}