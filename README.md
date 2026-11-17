# Workshop 4: PWM LED Sequence with GPIOTE Double-Click Control

## Overview

This project is a continuation of the previous workshop. The goal is to replace direct GPIO toggling with PWM-driven LEDs, so that each LED smoothly ramps its brightness up and down instead of simply switching on and off. Button handling is moved from polling to the GPIOTE peripheral, and a new double-click gesture is used to start and stop the whole blinking sequence. The project is split into three modules — `pwm_led`, `button`, and `logs` — on top of `main.c`.

## Project Requirements

### 1. PWM-Driven LEDs

LEDs are no longer switched fully on/off. Each LED's brightness is controlled by the `nrf_drv_pwm` driver:

- **PWM Frequency**: 1 kHz, configured via `NRF_PWM_CLK_1MHz` base clock and a `PWM_TOP_VALUE` of `1000`.
- **Duty Cycle Ramp**: the duty cycle smoothly increases from 0% to 100% and back down to 0% in 1% steps (`RAMP_STEP_PERCENT`), with a new step every 8 ms (`RAMP_STEP_MS`), driven by a repeating `app_timer`.
- **LED Sequence**: identical to the previous workshop — each LED blinks the number of times given by one digit of the device ID, then control moves to the next LED.
- Only one PWM channel is active at a time; `pwm_reinit_for_pin()` re-targets the single PWM instance (`NRF_DRV_PWM_INSTANCE(0)`) at the LED pin for the current digit, using `NRF_DRV_PWM_PIN_INVERTED` for the active-low LEDs.

### 2. GPIOTE Button Handling

The button is no longer polled in the main loop. `nrf_drv_gpiote_in_init()` registers an interrupt-driven handler on a falling edge (`GPIOTE_CONFIG_IN_SENSE_HITOLO`, internal pull-up), so a press is detected asynchronously via `button_event_handler()`.

### 3. Double-Click Start/Stop

Button behavior changed from "advance while held" to a double-click toggle:

- Each press increments a click counter and (re)starts a 400 ms window (`DOUBLE_CLICK_WINDOW_MS`) using a single-shot `app_timer`.
- If a second click lands inside that window, the sequence's running state is flipped.
- **No blinking + double-click** → smooth blinking starts.
- **Smooth blinking + double-click** → the sequence freezes: the ramp timer stops updating the duty cycle, and the PWM peripheral keeps replaying the last value, so the LED holds exactly the brightness it had at the moment of the double-click.
- A single click within the window is ignored, since only double-click is defined for this workshop.

### 4. Log Messages (Optional)

`NRF_LOG_INFO()` is used to report ramp progress and double-click events. Logging goes out over the SDK's USB log backend (`nrf_log_backend_usb.c`), the same mechanism used by the `examples/peripheral/dongle_logs` sample — the dongle has no on-board UART-to-USB bridge or debugger, so the virtual COM port it exposes (`/dev/ttyACM0`) is the only way to see log output. `sdk_config.h` was taken from the `dongle_logs` sample and extended with an `app_config.h` (enabled via `-DUSE_APP_CONFIG`) holding the PWM, GPIOTE, and `app_timer` settings this project needs.

## Device ID Sequence

The device ID is `#6578`. The sequence is defined in `pwm_led.c` and follows the same logic as in the previous workshops:

- **LED 1 (Green)**: 6 smooth blinks
- **LED 2 (Red)**: 5 smooth blinks
- **LED 3 (Green)**: 7 smooth blinks
- **LED 4 (Blue)**: 8 smooth blinks

## Implementation Details

- **`pwm_led.c` / `pwm_led.h`**: owns the PWM instance, the duty-cycle ramp state machine, and the LED/digit sequence. `ramp_timer_handler()` only advances the ramp while `button_is_sequence_running()` reports `true`; otherwise it returns immediately, freezing the LED at its current duty cycle.
- **`button.c` / `button.h`**: owns GPIOTE initialization, the double-click detection window, and the `m_blinking_active` flag exposed read-only via `button_is_sequence_running()`.
- **`logs.c` / `logs.h`**: thin wrapper around `NRF_LOG_INIT()` and `NRF_LOG_DEFAULT_BACKENDS_INIT()`, called once from `main()`.
- **`main.c`**: initializes the low-frequency clock (required for `app_timer`) via `nrf_drv_clock_lfclk_request()`, then `app_timer_init()`, then the three modules above, and finally runs the main loop, pumping `app_usbd_event_queue_process()` and `NRF_LOG_FLUSH()` so USB and logging keep working without blocking.

## How to Build and Flash

1. Open the terminal in the root directory of the project (where the `Makefile` is located).
2. Put the dongle into bootloader mode by pressing the **RESET** button (the red LED should start blinking).
3. Run the flash command (this automatically builds the project, packages it, and flashes the dongle):

   ```
   make dfu
   ```

4. After flashing, the dongle exposes a virtual COM port. Connect to it to see log output:

   ```
   picocom /dev/ttyACM0
   ```

   (To exit `picocom`, use `Ctrl+a`, `Ctrl+q`. If you have problems with `picocom`, try `cat /dev/ttyACM0`.)
