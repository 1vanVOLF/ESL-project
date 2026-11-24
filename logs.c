/* logs.c
 *
 * Same approach as used in Workshop_4: NRF_LOG_INIT + the SDK's default
 * backends, selected entirely through sdk_config.h.
 */

#include "logs.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "app_error.h"
#include "app_usbd.h"

void logs_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

void logs_process(void)
{
    while (app_usbd_event_queue_process())
    {
        /* drain all queued USB events */
    }

    NRF_LOG_PROCESS();
}