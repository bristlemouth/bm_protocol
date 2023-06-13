/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2015 Damien P. George
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

#include <string.h>

#include "modmachine.h"
#include "py/gc.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"
// #include "drivers/dht/dht.h"
// #include "extmod/machine_bitstream.h"
// #include "extmod/machine_mem.h"
// #include "extmod/machine_signal.h"
// #include "extmod/machine_pulse.h"
// #include "extmod/machine_i2c.h"
// #include "extmod/machine_spi.h"
#include "shared/runtime/pyexec.h"
// #include "lib/oofatfs/ff.h"
// #include "extmod/vfs.h"
// #include "extmod/vfs_fat.h"
// #include "gccollect.h"
// #include "irq.h"
// #include "powerctrl.h"
// #include "boardctrl.h"
// #include "pybthread.h"
// #include "rng.h"
// #include "storage.h"
// #include "pin.h"
// #include "timer.h"
// #include "usb.h"
// #include "rtc.h"
// #include "i2c.h"
// #include "spi.h"
// #include "uart.h"
// #include "wdt.h"

#include "device_info.h"
#include "reset_reason.h"
#include "bsp.h"


STATIC uint32_t reset_cause;

void machine_init(void) {
    reset_cause = checkResetReason();
}

void machine_deinit(void) {

}

// machine.info([dump_alloc_table])
// Print out lots of information about the board.
STATIC mp_obj_t machine_info(size_t n_args, const mp_obj_t *args) {
    const mp_print_t *print = &mp_plat_print;

    // get and print unique id; 96 bits
    {
        // mp_printf(print, "Reset Reason: %s\n", getResetReasonString());
        mp_printf(print, "Node ID: %s\n", getNodeIdStr());

        // TODO - add more info
    }


    // qstr info
    {
        size_t n_pool, n_qstr, n_str_data_bytes, n_total_bytes;
        qstr_pool_info(&n_pool, &n_qstr, &n_str_data_bytes, &n_total_bytes);
        mp_printf(print, "qstr:\n  n_pool=%u\n  n_qstr=%u\n  n_str_data_bytes=%u\n  n_total_bytes=%u\n", n_pool, n_qstr, n_str_data_bytes, n_total_bytes);
    }

    // GC info
    {
        gc_info_t info;
        gc_info(&info);
        mp_printf(print, "GC:\n");
        mp_printf(print, "  %u total\n", info.total);
        mp_printf(print, "  %u : %u\n", info.used, info.free);
        mp_printf(print, "  1=%u 2=%u m=%u\n", info.num_1block, info.num_2block, info.max_block);
    }

    // free space on flash
    {
        #if MICROPY_VFS_FAT
        for (mp_vfs_mount_t *vfs = MP_STATE_VM(vfs_mount_table); vfs != NULL; vfs = vfs->next) {
            if (strncmp("/flash", vfs->str, vfs->len) == 0) {
                // assumes that it's a FatFs filesystem
                fs_user_mount_t *vfs_fat = MP_OBJ_TO_PTR(vfs->obj);
                DWORD nclst;
                f_getfree(&vfs_fat->fatfs, &nclst);
                mp_printf(print, "LFS free: %u bytes\n", (uint)(nclst * vfs_fat->fatfs.csize * 512));
                break;
            }
        }
        #endif
    }

    #if MICROPY_PY_THREAD
    pyb_thread_dump(print);
    #endif

    if (n_args == 1) {
        // arg given means dump gc allocation table
        gc_dump_alloc_table(print);
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_info_obj, 0, 1, machine_info);

// Returns a string of 12 bytes (96 bits), which is the unique ID for the MCU.
STATIC mp_obj_t machine_unique_id(void) {
    byte *id = (byte *)getUID();
    return mp_obj_new_bytes(id, 12);
}
MP_DEFINE_CONST_FUN_OBJ_0(machine_unique_id_obj, machine_unique_id);

// Resets the pyboard in a manner similar to pushing the external RESET button.
STATIC mp_obj_t machine_reset(void) {
    resetSystem(RESET_REASON_MICROPYTHON);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(machine_reset_obj, machine_reset);

STATIC mp_obj_t machine_soft_reset(void) {
    pyexec_system_exit = PYEXEC_FORCED_EXIT;
    mp_raise_type(&mp_type_SystemExit);
}
MP_DEFINE_CONST_FUN_OBJ_0(machine_soft_reset_obj, machine_soft_reset);

// Activate the bootloader without BOOT* pins.
// NORETURN mp_obj_t machine_bootloader(size_t n_args, const mp_obj_t *args) {
//     (void)n_args;
//     (void)args;

//     const mp_print_t *print = &mp_plat_print;
//     mp_printf(print, "Not yet implemented");
// }
// MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_bootloader_obj, 0, 1, machine_bootloader);

// get or set the MCU frequencies
STATIC mp_obj_t machine_freq(size_t n_args, const mp_obj_t *args) {
    (void)args;

    if (n_args == 0) {
        return mp_obj_new_int(HAL_RCC_GetSysClockFreq());
    } else {
        const mp_print_t *print = &mp_plat_print;
        mp_printf(print, "Setting frequency not allowed.");
        return mp_const_none;
    }
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_freq_obj, 0, 4, machine_freq);

// idle()
// This executies a wfi machine instruction which reduces power consumption
// of the MCU until an interrupt occurs, at which point execution continues.
STATIC mp_obj_t machine_idle(void) {
    __WFI();
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(machine_idle_obj, machine_idle);

STATIC mp_obj_t machine_lightsleep(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    (void)args;
    const mp_print_t *print = &mp_plat_print;
    mp_printf(print, "Not yet implemented");
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_lightsleep_obj, 0, 1, machine_lightsleep);

STATIC mp_obj_t machine_deepsleep(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    (void)args;
    const mp_print_t *print = &mp_plat_print;
    mp_printf(print, "Not yet implemented");
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_deepsleep_obj, 0, 1, machine_deepsleep);

STATIC mp_obj_t machine_reset_cause(void) {
    return mp_obj_new_str(getResetReasonString(), strlen(getResetReasonString()));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_reset_cause_obj, machine_reset_cause);

#if MICROPY_PY_MACHINE

STATIC const mp_rom_map_elem_t machine_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),            MP_ROM_QSTR(MP_QSTR_umachine) },
    { MP_ROM_QSTR(MP_QSTR_info),                MP_ROM_PTR(&machine_info_obj) },
    { MP_ROM_QSTR(MP_QSTR_unique_id),           MP_ROM_PTR(&machine_unique_id_obj) },
    { MP_ROM_QSTR(MP_QSTR_reset),               MP_ROM_PTR(&machine_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_soft_reset),          MP_ROM_PTR(&machine_soft_reset_obj) },
    // { MP_ROM_QSTR(MP_QSTR_bootloader),          MP_ROM_PTR(&machine_bootloader_obj) },
    { MP_ROM_QSTR(MP_QSTR_freq),                MP_ROM_PTR(&machine_freq_obj) },
    #if MICROPY_HW_ENABLE_RNG
    { MP_ROM_QSTR(MP_QSTR_rng),                 MP_ROM_PTR(&pyb_rng_get_obj) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_idle),                MP_ROM_PTR(&machine_idle_obj) },
    // { MP_ROM_QSTR(MP_QSTR_sleep),               MP_ROM_PTR(&machine_lightsleep_obj) },
    // { MP_ROM_QSTR(MP_QSTR_lightsleep),          MP_ROM_PTR(&machine_lightsleep_obj) },
    // { MP_ROM_QSTR(MP_QSTR_deepsleep),           MP_ROM_PTR(&machine_deepsleep_obj) },
    { MP_ROM_QSTR(MP_QSTR_reset_cause),         MP_ROM_PTR(&machine_reset_cause_obj) },
    #if 0
    { MP_ROM_QSTR(MP_QSTR_wake_reason),         MP_ROM_PTR(&machine_wake_reason_obj) },
    #endif

    // { MP_ROM_QSTR(MP_QSTR_disable_irq),         MP_ROM_PTR(&machine_disable_irq_obj) },
    // { MP_ROM_QSTR(MP_QSTR_enable_irq),          MP_ROM_PTR(&machine_enable_irq_obj) },

    #if MICROPY_PY_MACHINE_BITSTREAM
    { MP_ROM_QSTR(MP_QSTR_bitstream),           MP_ROM_PTR(&machine_bitstream_obj) },
    #endif
    #if MICROPY_PY_MACHINE_PULSE
    { MP_ROM_QSTR(MP_QSTR_time_pulse_us),       MP_ROM_PTR(&machine_time_pulse_us_obj) },
    #endif
    // { MP_ROM_QSTR(MP_QSTR_dht_readinto),        MP_ROM_PTR(&dht_readinto_obj) },

    // { MP_ROM_QSTR(MP_QSTR_mem8),                MP_ROM_PTR(&machine_mem8_obj) },
    // { MP_ROM_QSTR(MP_QSTR_mem16),               MP_ROM_PTR(&machine_mem16_obj) },
    // { MP_ROM_QSTR(MP_QSTR_mem32),               MP_ROM_PTR(&machine_mem32_obj) },

    // { MP_ROM_QSTR(MP_QSTR_Pin),                 MP_ROM_PTR(&pin_type) },
    // { MP_ROM_QSTR(MP_QSTR_Signal),              MP_ROM_PTR(&machine_signal_type) },

    // { MP_ROM_QSTR(MP_QSTR_RTC),                 MP_ROM_PTR(&pyb_rtc_type) },
    // { MP_ROM_QSTR(MP_QSTR_ADC),                 MP_ROM_PTR(&machine_adc_type) },
    #if MICROPY_PY_MACHINE_I2C
    #if MICROPY_HW_ENABLE_HW_I2C
    { MP_ROM_QSTR(MP_QSTR_I2C),                 MP_ROM_PTR(&machine_i2c_type) },
    #else
    { MP_ROM_QSTR(MP_QSTR_I2C),                 MP_ROM_PTR(&mp_machine_soft_i2c_type) },
    #endif
    // { MP_ROM_QSTR(MP_QSTR_SoftI2C),             MP_ROM_PTR(&mp_machine_soft_i2c_type) },
    #endif
    #if MICROPY_PY_MACHINE_SPI
    { MP_ROM_QSTR(MP_QSTR_SPI),                 MP_ROM_PTR(&machine_spi_type) },
    { MP_ROM_QSTR(MP_QSTR_SoftSPI),             MP_ROM_PTR(&mp_machine_soft_spi_type) },
    #endif
    #if MICROPY_HW_ENABLE_I2S
    { MP_ROM_QSTR(MP_QSTR_I2S),                 MP_ROM_PTR(&machine_i2s_type) },
    #endif
    // { MP_ROM_QSTR(MP_QSTR_UART),                MP_ROM_PTR(&pyb_uart_type) },
    // { MP_ROM_QSTR(MP_QSTR_WDT),                 MP_ROM_PTR(&pyb_wdt_type) },
    // { MP_ROM_QSTR(MP_QSTR_Timer),               MP_ROM_PTR(&machine_timer_type) },
    #if 0
    { MP_ROM_QSTR(MP_QSTR_HeartBeat),           MP_ROM_PTR(&pyb_heartbeat_type) },
    { MP_ROM_QSTR(MP_QSTR_SD),                  MP_ROM_PTR(&pyb_sd_type) },

    // class constants
    { MP_ROM_QSTR(MP_QSTR_IDLE),                MP_ROM_INT(PYB_PWR_MODE_ACTIVE) },
    { MP_ROM_QSTR(MP_QSTR_SLEEP),               MP_ROM_INT(PYB_PWR_MODE_LPDS) },
    { MP_ROM_QSTR(MP_QSTR_DEEPSLEEP),           MP_ROM_INT(PYB_PWR_MODE_HIBERNATE) },
    #endif
    // { MP_ROM_QSTR(MP_QSTR_PWRON_RESET),         MP_ROM_INT(PYB_RESET_POWER_ON) },
    // { MP_ROM_QSTR(MP_QSTR_HARD_RESET),          MP_ROM_INT(PYB_RESET_HARD) },
    // { MP_ROM_QSTR(MP_QSTR_WDT_RESET),           MP_ROM_INT(PYB_RESET_WDT) },
    // { MP_ROM_QSTR(MP_QSTR_DEEPSLEEP_RESET),     MP_ROM_INT(PYB_RESET_DEEPSLEEP) },
    // { MP_ROM_QSTR(MP_QSTR_SOFT_RESET),          MP_ROM_INT(PYB_RESET_SOFT) },
    #if 0
    { MP_ROM_QSTR(MP_QSTR_WLAN_WAKE),           MP_ROM_INT(PYB_SLP_WAKED_BY_WLAN) },
    { MP_ROM_QSTR(MP_QSTR_PIN_WAKE),            MP_ROM_INT(PYB_SLP_WAKED_BY_GPIO) },
    { MP_ROM_QSTR(MP_QSTR_RTC_WAKE),            MP_ROM_INT(PYB_SLP_WAKED_BY_RTC) },
    #endif
};

STATIC MP_DEFINE_CONST_DICT(machine_module_globals, machine_module_globals_table);

const mp_obj_module_t mp_module_machine = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&machine_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_umachine, mp_module_machine);

#endif // MICROPY_PY_MACHINE
