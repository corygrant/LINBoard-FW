# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LINBoard-FW is an embedded firmware project for the LINBoard v1 hardware, built on ChibiOS RTOS. The firmware targets STM32F303 microcontrollers and implements CAN bus communication functionality with LED status indicators.

## Build System

This project uses GNU Make with ChibiOS integration:

- **Build command**: `make` (builds for linboard_v1 board by default)
- **Clean build**: `make clean && make`
- **Build directory**: `./build/` (generated artifacts)
- **Board selection**: Set `BOARD=linboard_v1` (default) or override for different boards
- **Configuration**: Uses STM32F303xC linker script and Cortex-M4 target

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
- **Mailbox System**: Message passing implemented in `mailbox.cpp/mailbox.h`
- **Configuration**: Version and timing parameters in `linboard_config.h`

### Key Files Structure
- `main.cpp`: Application entry point with LED blinking loop
- `can.cpp/can.h`: CAN bus initialization and communication
- `mailbox.cpp/mailbox.h`: Inter-thread messaging system
- `enums.h`: System-wide enumerations (CAN bitrates, error types)
- `boards/linboard_v1/`: Board-specific HAL configuration and pin definitions
- `cfg/`: ChibiOS HAL and MCU configuration files

### Hardware Features
- **CAN Interface**: Supports 125K, 250K, 500K, 1000K bitrates
- **Status LEDs**: CAN_LED, LIN_LED, STATUS_LED for visual feedback
- **I2C Interface**: Configured for 400kHz fast mode
- **Target MCU**: STM32F303 with hardware FPU support

### ChibiOS Integration
- Uses ChibiOS RT kernel with C++ wrappers (`chcpp.mk`)
- HAL subsystems: PAL (GPIO), CAN, I2C enabled in `halconf.h`
- Threading primitives: `chThdSleepMilliseconds()` for delays
- System time: `chVTGetSystemTimeX()` with TIME_I2MS conversion

## Development Notes

- The firmware currently implements a simple LED blinking pattern while initializing CAN at 500K bitrate
- Error handling framework is defined but not fully implemented (see `FatalErrorType` enum)
- CAN filtering capabilities are available but disabled by default
- The build system supports multiple board configurations through the board.mk system