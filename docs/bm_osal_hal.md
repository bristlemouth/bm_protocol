# OSAL and HAL Abstraction Layers in Bristlemouth

This document explains the design, rationale, and usage of the OS Abstraction Layer (OSAL) and Hardware Abstraction Layer (HAL) introduced into the bm_protocol codebase.

These abstraction layers are intended to improve portability, testability, and maintainability of the Bristlemouth firmware stack. This will also improve code quality and flexibility.

Contributors should use OSAL and HAL APIs instead of direct FreeRTOS or STM32 calls when possible.

For more details, refer to:

[src/lib/bm_osal/README.md](../src/lib/bm_osal/README.md)

[src/lib/bm_hal/README.md](../src/lib/bm_hal/README.md)

# Design Goals

The OSAL and HAL provide a uniform interface between Bristlemouth core libraries and the underlying operating system and hardware platform. This decouples application logic from specific platform implementations (e.g., FreeRTOS, STM32), allowing for:

* **Portability**:

    * Bristlemouth firmware can compile and run on multiple targets (FreeRTOS, host, test harnesses)

    * Easier to port Bristlemouth to new platforms or RTOSs


* **Testability**:
    * Core logic can be tested without running on embedded hardware

    * Easier unit testing using mocks or POSIX on host systems

* **Separation of concerns**:
    * Cleaner dependency boundaries and encapsulation

    * Break tight coupling and dependency on specific RTOS's and target chips

    * Platform-specific logic (e.g., STM32 hal, FreeRTOS) is hidden behind well-defined APIs

* **Minimal overhead**:
    * All abstractions are implemented as thin wrappers around native RTOS or hardware calls

# Abstraction Layers

## OSAL (src/lib/bm_osal)

The OSAL defines common interfaces for RTOS features:

* Task creation and delays

* Mutexes

* Queues

* Timers

* Millisecond delays

### Implementations

* bm_osal_freertos.c – wraps FreeRTOS primitives

* bm_osal_mock.c – dummy mock layer for unit testing

* bm_osal_posix.c – pthread-based stub for POSIX systems

## HAL (src/lib/bm_hal)

The HAL provides hardware-related abstractions, currently:

* Watchdog instance access and reload

* Watchdog IRQ handler (STM32-specific)

This layer is intended to expand to other peripherals such as GPIO or UART.

### Implementations

* bm_hal_stm32u5xx.c – STM32 IWDG-based watchdog

# Build Configuration

The following CMake variables determine which implementation to use:

```
cmake -D BM_OSAL=freertos    # or mock, posix
cmake -D BM_HAL_TARGET=stm32u5xx
```

# Usage Example

## Example use of OSAL
```
#include "bm_osal.h"

void app_task(void* arg) {
    while (1) {
        // do work
        bm_osal_task_delay(100);
    }
}

void start_app() {
    bm_osal_task_handle_t handle;
    bm_osal_task_create(app_task, "app", 256, NULL, 1, &handle);
}
```

## Example use of HAL
```
#include "bm_hal.h"

void watchdogFeed() {
    bm_hal_wd_reload_counter(bm_hal_wd_get_handle());
}
```

# Future Work

* **bm_hal**
    * Abstract additional hardware peripherals (GPIO, UART, SPI)

    * Support more targets

    * Add unit testing

    * Currently, the API's mirror the ST hal. This is a good first step to break dependencies. But these apis' would likely need to change to accomodate other targets in the future.

* **bm_osal**
    * Add support for stream buffers or event groups

    * Add more unit testing

    * Expand POSIX implementation

    * Currently, the API's mirror FreeRTOS. This is a good first step to break dependencies. But these apis' would likely need to change to accomodate other RTOS's in the future.

# Limitations

* Not all FreeRTOS features are abstracted (e.g., stream buffers still used directly)

* Did not test on any actual platforms yet

* Unit tests only cover task abstraction; others are stubbed
