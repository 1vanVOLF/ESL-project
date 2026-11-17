#include <stdbool.h>
#include <stdint.h>
#include "nordic_common.h"
#include "nrf_delay.h"
#include "nrf_drv_clock.h"
#include "app_error.h"
#include "app_timer.h"
#include "app_usbd.h"

#include "logs.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "button.h"
#include "pwm_led.h"

static void clock_init(void)
{
    ret_code_t err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);
    nrf_drv_clock_lfclk_request(NULL);
    while (!nrf_drv_clock_lfclk_is_running()) { __WFE(); }
}

int main(void)
{
    ret_code_t err_code;

    clock_init();

    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    logs_init();
    button_init();
    pwm_led_init();

    NRF_LOG_FLUSH();

    while (true)
    {
        while (app_usbd_event_queue_process()) {}
        NRF_LOG_FLUSH();
        __WFE();
    }
}