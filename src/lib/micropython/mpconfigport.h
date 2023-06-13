/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2021 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>

// Options to control how MicroPython is built

// Use the minimal starting configuration (disables all optional features).
#define MICROPY_CONFIG_ROM_LEVEL                (MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES)

// Compiler configuration
#define MICROPY_ENABLE_COMPILER                 (1)

// Python internal features
#define MICROPY_ERROR_REPORTING                 (MICROPY_ERROR_REPORTING_DETAILED)

// Enable garbage collector
#define MICROPY_ENABLE_GC 1
#define MICROPY_HELPER_REPL 1

#define MICROPY_LONGINT_IMPL        (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_FLOAT_IMPL          (MICROPY_FLOAT_IMPL_FLOAT)

#define MICROPY_EPOCH_IS_1970 1

#define MICROPY_MODULE_FROZEN_MPY               (1)
//#define MICROPY_MODULE_FROZEN_STR               (1)
#define MICROPY_QSTR_EXTRA_POOL                 mp_qstr_frozen_const_pool
// must match MPY_TOOL_FLAGS or defaults for mpy-tool.py arguments
#define MPZ_DIG_SIZE                            (16)
#define MICROPY_ENABLE_EXTERNAL_IMPORT    (1)

#define MP_SSIZE_MAX (0x7fffffff)

#define MICROPY_PY_TIME_TICKS 1

// Type definitions for the specific machine

typedef int32_t mp_int_t; // must be pointer size
typedef uint32_t mp_uint_t; // must be pointer size
typedef long mp_off_t;

// Need to provide a declaration/definition of alloca()
#include <alloca.h>

#define MICROPY_HW_BOARD_NAME "bristlemouth"
#define MICROPY_HW_MCU_NAME "stm32u5xx"

#ifdef __thumb__
#define MICROPY_MIN_USE_CORTEX_CPU (1)
#define MICROPY_MIN_USE_STM32_MCU (1)
#endif

#define MP_STATE_PORT MP_STATE_VM

#define MICROPY_PORT_ROOT_POINTERS \
    const char *readline_hist[8]; \
    void *mp_lv_user_data; \

#define MICROPY_PY_MACHINE 1

#define MICROPY_PY_MACHINE_I2C 1
#define MICROPY_HW_ENABLE_HW_I2C 1


// External modules

#define MICROPY_PY_UASYNCIO 1

#define MICROPY_PY_UTIME 1
#define MICROPY_PY_UTIME_MP_HAL 1
#define MICROPY_PY_UTIMEQ 1

#define MICROPY_PY_USELECT 0
#define MICROPY_PY_SYS_PS1_PS2 0
#define MICROPY_PY_SYS_STDFILES 0
