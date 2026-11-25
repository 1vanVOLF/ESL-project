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
#include "color_storage.h"
#include "logs.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "app_usbd.h"

#if !NRF_LOG_ENABLED
#error "NRF_LOG is DISABLED - app_config.h not picked up!"
#endif
#if !NRF_LOG_BACKEND_USB_ENABLED
#error "USB log backend is DISABLED!"
#endif

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
    input_mode_led_update(dt_ms);
    rgb_led_set_hsv(g_hsv);
}

int main(void)
{
    ret_code_t err_code;

    clock_init();

    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    logs_init();

    if (color_storage_load(&g_hsv))
    {
        NRF_LOG_INFO("Startup: restored saved color, H=%d S=%d V=%d", g_hsv.hue, g_hsv.saturation, g_hsv.value);
    }
    else
    {
        NRF_LOG_INFO("Startup: no saved color, using default H=%d S=%d V=%d", g_hsv.hue, g_hsv.saturation, g_hsv.value);
    }
    NRF_LOG_FLUSH();

    input_mode_init();
    button_handler_init(&g_hsv);
    rgb_led_init();

    rgb_led_set_hsv(g_hsv);


    app_timer_tick_init(on_app_tick);

    while (true)
    {
        while (app_usbd_event_queue_process()) {}
        NRF_LOG_PROCESS();
        NRF_LOG_FLUSH();
    }
}