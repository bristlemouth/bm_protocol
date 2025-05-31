# Bristlemouth HAL - Hardware Abstraction Layer

This directory contains the Hardware Abstraction Layer (HAL) for Bristlemouth firmware.

It abstracts platform-specific hardware behavior behind a common interface, allowing system-level code to remain portable and hardware-agnostic.

# Purpose

* Provide clean, minimal access to platform hardware (e.g., watchdog)

* Allow alternate hardware targets to implement the same interfaces

* Encourage separation between hardware access and business logic

# Current Scope

The HAL currently defines a simple Watchdog API:

* `bm_hal_wd_get_handle()` – returns opaque watchdog handle

* `bm_hal_wd_reload_counter()` – reloads (kicks) the watchdog

* `bm_hal_wd_log()` – optional hook for diagnostics (e.g., Memfault)

# Supported Targets

* `stm32h5xx` – STM32 HAL-based implementation using IWDG

# Directory Structure

```
bm_hal/
├── bm_hal.h               # Public HAL interface
└── bm_hal_stm32h5xx.c     # STM32H5xx watchdog implementation
```

# CMake Configuration

Specify the hardware target using:

```
cmake -DBM_HAL_TARGET=stm32h5xx ..
```

If this parameter is not specified, the default will be `stm32h5xx`.

# Usage Example

```
#include "bm_hal.h"

bm_hal_wd_t wd = bm_hal_wd_get_handle();
if (wd) {
    bm_hal_wd_reload_counter(wd);
    bm_hal_wd_log();
}
```

# Future Work

* Abstract additional hardware peripherals (GPIO, UART, SPI)

* Support more targets

* Add unit testing

* Currently, the API's mirror the ST hal. This is a good first step to break dependencies. But these apis' would likely need to change to accomodate other targets in the future.

# Notes

* HAL implementations should avoid leaking vendor-specific types in the public API.

* All structures should be opaque to allow platform substitution.

# Contributing

When accessing hardware features (like watchdogs or GPIO), prefer using bm_hal_* APIs.

Avoid direct use of target HAL headers, such as the STM32 HAL or CMSIS, outside of HAL implementations.
