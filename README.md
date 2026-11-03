# Workshop 2: Nordic SDK Blinky & Custom LED Sequence

## Overview
This project demonstrates how to work with the Nordic SDK (nRF5 SDK) and the nRF52840 Dongle. The goal is to compile, flash, and modify a basic `blinky` example, adapting it to a standalone project and customizing the LED sequence according to a unique device ID.

## Prerequisites
* **Virtual Machine** (recommended): The provided ESL VirtualBox image with pre-installed Nordic SDK and toolchain.
* **Hardware**: nRF52840 Dongle (PCA10059).
* **IDE**: Visual Studio Code (with SSH Remote connection to the VM).

## 1. Initial Setup & Flashing Blinky
The original `blinky` sample was used as a starting point. 

### Flashing Firmware
1. Connect the nRF52840 Dongle via USB.
2. Press the **RESET** button on the dongle to enter the bootloader mode (the red LED starts blinking).
3. Navigate to the `Makefile` directory in the terminal.
4. Flash the firmware using the `make dfu` command.

> ⚠️ **VirtualBox Note:** If the flashing process gets stuck at **~97%**, you need to click the dongle **RESET** button. This happens because VirtualBox sometimes fails to handle the fast USB device reconnection when the Nordic SoftDevice is flashed first, causing the dongle to restart.

## 2. Creating a Standalone Project
The sample was extracted from the Nordic SDK to create a standalone project:
1. The contents of the `blinky` sample were moved to a separate project directory.
2. The build system (`Makefile`) was adapted to work outside the SDK. The path to the SDK is now specified via the `SDK_ROOT` parameter.
3. Unnecessary files were removed:
   * Non-Makefile build files (e.g., IAR, Keil projects).
   * All platform configurations except the target board (`pca10059`).
   * Unused or empty directories.

> 💡 **Note on flashing:** After the first successful flash, the `--softdevice $(SDK_ROOT)/.../s113_nrf52_7.2.0_softdevice.hex` line was removed from the `Makefile` (while keeping `$@` at the end). This is because the SoftDevice only needs to be flashed once, making subsequent flashing faster and more stable.

## 3. Custom LED Blinking Sequence
* **Device ID**: `#6578`
* **Interpretation**: The digits of the device ID represent how many times each corresponding LED should blink:
  * LED 1 (Green/Yellow): **6** blinks
  * LED 2 (Red): **5** blinks
  * LED 3 (Green): **7** blinks
  * LED 4 (Blue): **8** blinks

The `main.c` file was updated to implement this logic. A short pause is added between the blinking sequences of different LEDs to distinguish them clearly.

## How to Build and Flash
1. Open the terminal in the directory containing the `Makefile`.
2. Build the project:
   ```bash
   make
3. Put the dongle into bootloader mode (press RESET).
4. Flash the firmware:
   ```bash
   make dfu
5. Verify that the dongle blinks the sequence: 6 (LED1) -> pause -> 5 (LED2) -> pause -> 7 (LED3) -> pause -> 8 (LED4) in a loop.
