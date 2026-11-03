# Workshop 3: Button-Controlled LED Sequence with GPIO HAL

## Overview
This project is a continuation of the previous workshop. The goal is to implement a state machine that controls the LED blinking sequence based on button presses. Additionally, the project requires replacing the high-level board support package (`boards.h`) with direct GPIO Hardware Abstraction Layer (HAL) calls to control the pins manually.

## Project Requirements

### 1. Direct GPIO HAL Control
The application no longer uses `boards.h` or the `bsp_board_*` functions. Instead, all hardware interactions are handled directly via the Nordic `nrf_gpio.h` HAL.
*   **LEDs Configuration**: Pins are configured as outputs using `nrf_gpio_cfg_output()`. The active state is low (`0`), meaning `nrf_gpio_pin_clear()` turns the LED on, and `nrf_gpio_pin_set()` turns it off.
*   **Button Configuration**: The button pin is configured as an input with an internal pull-up resistor using `nrf_gpio_cfg_input(BUTTON_PIN, NRF_GPIO_PIN_PULLUP)`.

### 2. Button-Controlled Iteration (State Machine)
The LED sequence only advances while the button is pressed. The logic is implemented as a non-blocking state machine in the main loop, polling the button every 10ms (`TICK_MS`).
*   When the button is **pressed**, the timer advances, and the LEDs blink according to the sequence.
*   When the button is **released**, the sequence freezes.
*   If the button is pressed, released, and pressed again, the sequence continues from exactly where it left off.

### 3. Pause While LED is On
If the button is released precisely at the moment when an LED is currently on, the LED will remain illuminated. The timer stops counting, and the state machine freezes in the current state until the button is pressed again.

## Device ID Sequence
The device ID is `#6578`. The sequence is defined in `main.c` and follows the same logic as in Workshop 2:
*   **LED 1 (Green)**: 6 blinks
*   **LED 2 (Red)**: 5 blinks
*   **LED 3 (Green)**: 7 blinks
*   **LED 4 (Blue)**: 8 blinks

## Implementation Details
*   **Custom Timers**: Instead of blocking the CPU with `nrf_delay_ms()`, the code uses a software timer (`timer_ms`) incremented by the main loop tick (`TICK_MS`). This allows the program to continuously check the button state without freezing.
*   **State Variables**: Global variables such as `led_is_on`, `in_pause`, `current_led_idx`, and `blinks_done` keep track of the current progress.
*   **Blink Phases**: The logic is split into three main phases (blocks):
    1.  **Global Pause**: Waiting between switching to a new LED color.
    2.  **LED On**: The LED is currently lit, counting down the `BLINK_TIME_MS`.
    3.  **LED Off**: The LED is off, counting down the `BLINK_OFF_MS` before the next blink.

## How to Build and Flash
1. Open the terminal in the directory containing the `Makefile` (`.../blinky/pca10059/blank/armgcc`).
2. Put the dongle into bootloader mode by pressing the **RESET** button (the red LED should start blinking).
3. Run the flash command (this automatically builds the project, packages it, and flashes the dongle):
   ```bash
   make dfu
