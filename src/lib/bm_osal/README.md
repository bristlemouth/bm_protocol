# Bristlemouth OSAL - OS Abstraction Layer

This directory implements the OS Abstraction Layer (OSAL) for Bristlemouth firmware.

It provides a unified interface to RTOS features such as tasks, mutexes, queues, timers, and delays.

The goal is to isolate Bristlemouth system code from RTOS (currently FreeRTOS) internals.

This could also support bare-metal implementations.

# Supported Implementations

* freertos – Real implementation for FreeRTOS (default)

* mock – Dummy implementation for unit testing (no OS required)

* posix – Pthread-based implementation for host builds (Linux/macOS)

# Directory Structure

```
bm_osal/
├── bm_osal.h              # Public OSAL API
├── bm_osal_freertos.c     # FreeRTOS-based implementation
├── bm_osal_mock.c         # Stubbed/mock implementation
├── bm_osal_mock_defs.h    # Dummy handles for mock mode
├── bm_osal_posix.c        # Pthread implementation (incomplete)
└── tests/
    ├── test_osal_mock.cpp # Google Test unit tests (mock backend)
    └── CMakeLists.txt      # Test build rules
```

# CMake Configuration

Choose the backend by setting -DBM_OSAL when building:

```
cmake -DBM_OSAL=freertos ..   # use FreeRTOS (default)
cmake -DBM_OSAL=mock     ..   # for unit testing
cmake -DBM_OSAL=posix    ..   # for host testing
```

If this parameter is not specified, the default will be `freertos`.

# API Summary

* bm_osal_task_create, bm_osal_task_delete, bm_osal_task_delay

* bm_osal_mutex_create, bm_osal_mutex_lock, bm_osal_mutex_unlock, etc.

* bm_osal_queue_create, bm_osal_queue_send, bm_osal_queue_receive, etc.

* bm_osal_timer_create, bm_osal_timer_start, etc.

* bm_osal_delay_ms

* BM_OSAL_ASSERT() macro and bm_osal_assert()

Refer to bm_osal.h for full documentation.

# Running Unit Tests

TBD -- currently not functional.

# Limitations

* Not all FreeRTOS features (e.g., event groups, stream buffers) are currently abstracted
* POSIX implementation is minimal
    * Currently it will not compile, as posix is not included in the build system
    * It would take some work to rework the cmake configuration

# Future Work

* Add support for stream buffers or event groups

* Add more unit testing

* Expand POSIX implementation

* Currently, the API's mirror FreeRTOS. This is a good first step to break dependencies. But these apis' would likely need to change to accomodate other RTOS's in the future.

* To truly port other RTOS, there would be more work thank just implementing the OSAL api's
    * FreeRTOS requires FreeRTOSConfig.h, others would have similar configuration files
    * eg Zephyr uses Kconfig and CMake
    * Would need to investigate startup sequence, task priorities, and interrupt handling
    * Default sizes for heap, task stack, etc may be different

# Contributing

When writing new Bristlemouth code, prefer using bm_osal_* APIs instead of direct FreeRTOS calls. This ensures portability and testability.
