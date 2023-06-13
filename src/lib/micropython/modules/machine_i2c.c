#include <stdio.h>
#include <string.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "extmod/machine_i2c.h"
#include "modmachine.h"

#include "bsp.h"

#if MICROPY_HW_ENABLE_HW_I2C

#define MICROPY_HW_MAX_I2C 1

#define I2C_TIMEOUT_MS 100
#define I2C_POLL_DEFAULT_TIMEOUT_US (50000) // 50ms

typedef struct _machine_hard_i2c_obj_t {
    mp_obj_base_t base;
    I2CInterface_t *i2c;
} machine_hard_i2c_obj_t;

STATIC const machine_hard_i2c_obj_t machine_hard_i2c_obj[MICROPY_HW_MAX_I2C] = {
    [0] = {{&machine_i2c_type}, &i2c1},
};

STATIC void machine_hard_i2c_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    // machine_hard_i2c_obj_t *self = MP_OBJ_TO_PTR(self_in);

    // TODO - print actual info
    // Right now we're just going with the current default
    mp_printf(print, "I2C(N/A, scl=N/A, sda=n/A, freq=N/A)");

    (void)self_in;
    (void)kind;
}

void machine_hard_i2c_init(machine_hard_i2c_obj_t *self, uint32_t freq, uint32_t timeout_us) {
    // TODO - i2c already initialized right now
    (void)self;
    (void)freq;
    (void)timeout_us;
    const mp_print_t *print = &mp_plat_print;

    mp_printf(print, "TODO: Initialize with freq and timeout\n");
}

STATIC int machine_i2c_transfer_single(mp_obj_base_t *self_in, uint16_t addr, size_t len, uint8_t *buf, unsigned int flags) {
    machine_hard_i2c_obj_t *self = (machine_hard_i2c_obj_t *)self_in;
    // const mp_print_t *print = &mp_plat_print;

    //
    // TODO - add I2C MUX change here if needed!
    // Maybe have a weak function from the BSP or something like that
    //

    I2CResponse_t i2c_rval;
    if(len == 0) {
        i2c_rval = i2cProbe(self->i2c, addr, I2C_TIMEOUT_MS);
    } else if(flags & MP_MACHINE_I2C_FLAG_READ) {
        // mp_printf(print, "i2c read 0x%02X (%u)\n", addr, len);
        i2c_rval = i2cRx(self->i2c, addr, buf, len, I2C_TIMEOUT_MS);
    } else {
        // mp_printf(print, "i2c write 0x%02X (%u)\n", addr, len);
        i2c_rval = i2cTx(self->i2c, addr, buf, len, I2C_TIMEOUT_MS);
    }

    int rval;
    if(i2c_rval == I2C_OK) {
        rval = len;
    } else if(i2c_rval == I2C_TIMEOUT) {
        rval = MP_ETIMEDOUT;
    } else if (i2c_rval == I2C_NACK) {
        rval = MP_ENODEV;
    } else if (i2c_rval == I2C_MUTEX) {
        rval = MP_EBUSY;
    } else {
        // Negative return value since positive return values should signify
        // number of bytes transfered
        rval = -(int)i2c_rval;
    }

    return rval;
}

/******************************************************************************/
/* MicroPython bindings for machine API                                       */


mp_obj_t machine_hard_i2c_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    // MP_MACHINE_I2C_CHECK_FOR_LEGACY_SOFTI2C_CONSTRUCTION(n_args, n_kw, all_args);

    // parse args
    enum { ARG_id, ARG_scl, ARG_sda, ARG_freq, ARG_timeout, ARG_timingr };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_id, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_scl, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_sda, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_freq, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 100000} },
        { MP_QSTR_timeout, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = I2C_POLL_DEFAULT_TIMEOUT_US} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // get static peripheral object
    // int i2c_id = i2c_find_peripheral(args[ARG_id].u_obj);
    machine_hard_i2c_obj_t *self = (machine_hard_i2c_obj_t *)&machine_hard_i2c_obj[0];

    // here we would check the scl/sda pins and configure them, but it's not implemented
    if (args[ARG_scl].u_obj != MP_OBJ_NULL || args[ARG_sda].u_obj != MP_OBJ_NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("explicit choice of scl/sda is not implemented"));
    }

    // initialise the I2C peripheral
    machine_hard_i2c_init(self, args[ARG_freq].u_int, args[ARG_timeout].u_int);

    return MP_OBJ_FROM_PTR(self);
}

STATIC const mp_machine_i2c_p_t machine_hard_i2c_p = {
    .transfer = mp_machine_i2c_transfer_adaptor,
    .transfer_single = machine_i2c_transfer_single,
};

MP_DEFINE_CONST_OBJ_TYPE(
    machine_i2c_type,
    MP_QSTR_I2C,
    MP_TYPE_FLAG_NONE,
    make_new, machine_hard_i2c_make_new,
    print, machine_hard_i2c_print,
    protocol, &machine_hard_i2c_p,
    locals_dict, &mp_machine_i2c_locals_dict
    );

#endif // MICROPY_HW_ENABLE_HW_I2C
