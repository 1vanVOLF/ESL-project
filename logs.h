/* logs.h
 *
 * Thin wrapper around NRF_LOG initialization, using the SDK's default
 * backends (configured via sdk_config.h to go out over USB CDC-ACM, so
 * logs can be read with `cat /dev/ttyACM0` without any external adapter).
 */

#ifndef LOGS_H
#define LOGS_H

/* Initializes the NRF_LOG module and its default backend(s). Must be
 * called once, early in main(), before any NRF_LOG_* macro is used. */
void logs_init(void);

/* Must be called periodically (e.g. once per application tick) to let
 * the USB CDC-ACM backend process its events and actually flush
 * buffered log messages out over USB. */
void logs_process(void);

#endif /* LOGS_H */