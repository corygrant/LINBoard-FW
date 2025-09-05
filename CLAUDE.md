# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LINBoard-FW is an embedded firmware project for the LINBoard v1 hardware, built on ChibiOS RTOS. The firmware targets STM32F302CC microcontrollers and implements CAN bus communication functionality with USB CDC (Virtual COM Port) bridge and LED status indicators.

## Build System

This project uses GNU Make with ChibiOS integration:

- **Build command**: `make` (builds for linboard_v1 board by default)
- **Clean build**: `make clean && make`
- **Build directory**: `./build/` (generated artifacts)
- **Board selection**: Set `BOARD=linboard_v1` (default) or override for different boards
- **Configuration**: Uses STM32F302xC linker script and Cortex-M4 target

### Build Configuration
- Default board: `linboard_v1`
- MCU: Cortex-M4 with FPU (hard float)
- Optimization: `-O0` with debug symbols (for debugging)
- C++ Standard: C++20 with RTTI/exceptions disabled
- ChibiOS features: RT kernel with C++ wrappers enabled

## Architecture

### Core Components
- **ChibiOS RTOS**: Real-time operating system providing threading, synchronization, and HAL
- **Hardware Abstraction**: Board-specific files in `boards/linboard_v1/`
- **CAN Communication**: Implemented in `can.cpp/can.h` with configurable bitrates
- **USB CDC Bridge**: Virtual COM port implementation in `usb.cpp/usb.h` for CAN-to-USB communication
- **Mailbox System**: Message passing implemented in `mailbox.cpp/mailbox.h`
- **Configuration**: Version and timing parameters in `linboard_config.h`

### Key Files Structure
- `main.cpp`: Application entry point with LED blinking loop
- `can.cpp/can.h`: CAN bus initialization and communication
- `usb.cpp/usb.h`: USB CDC Virtual COM port implementation
- `mailbox.cpp/mailbox.h`: Inter-thread messaging system for CAN/USB data exchange
- `enums.h`: System-wide enumerations (CAN bitrates, error types)
- `boards/linboard_v1/`: Board-specific HAL configuration and pin definitions
- `cfg/`: ChibiOS HAL and MCU configuration files

### Hardware Features
- **CAN Interface**: Supports 125K, 250K, 500K, 1000K bitrates
- **USB Interface**: CDC Virtual COM port for CAN data bridging (USBv1 peripheral)
- **Status LEDs**: CAN_LED, LIN_LED, STATUS_LED for visual feedback
- **I2C Interface**: Configured for 400kHz fast mode
- **Target MCU**: STM32F302CC with hardware FPU support

### ChibiOS Integration
- Uses ChibiOS RT kernel with C++ wrappers (`chcpp.mk`)
- HAL subsystems: PAL (GPIO), CAN, USB, Serial USB, I2C enabled in `halconf.h`
- USB implementation: USBv1 driver for STM32F302CC with CDC class support
- Threading primitives: `chThdSleepMilliseconds()` for delays
- System time: `chVTGetSystemTimeX()` with TIME_I2MS conversion

## Development Notes

- The firmware implements CAN-to-USB bridge functionality with LED status indicators
- CAN bus runs at 500K bitrate by default with configurable rates
- USB CDC provides virtual COM port interface for host communication
- Multi-threaded architecture with separate threads for CAN TX/RX and USB TX/RX
- Error handling framework is defined but not fully implemented (see `FatalErrorType` enum)
- CAN filtering capabilities are available but disabled by default
- The build system supports multiple board configurations through the board.mk system

### USB Implementation Notes
- Uses ChibiOS USBv1 driver (not OTGv1) for STM32F302CC
- EP0 max packet size: 8 bytes (required for reliable enumeration)
- EP1: Interrupt IN (CDC notifications)
- EP2: Bulk IN/OUT (data transfer)
- Custom requests hook handles SET_INTERFACE for proper CDC enumeration
- Hardware D+ pullup resistor enables USB device detection