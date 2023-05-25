#include <string.h>
#include <stdio.h>

#include "bsp.h"

// Include MicroPython API.
#include "py/runtime.h"

// Used to get the time in the Timer class example.
#include "py/mphal.h"

#include "device_info.h"

STATIC mp_obj_t info() {
  mp_obj_t ret_val = mp_obj_new_dict(0);
  char scratch[128];
  int32_t len = 0;
  len = snprintf(scratch, sizeof(scratch), "%016llx", getNodeId());
  mp_obj_dict_store(ret_val, mp_obj_new_str("node-id", 7), mp_obj_new_str(scratch, len));

  len = snprintf(scratch, sizeof(scratch), "%lX", getGitSHA());
  mp_obj_dict_store(ret_val, mp_obj_new_str("GITSHA", 6), mp_obj_new_str(scratch, len));

  mp_obj_dict_store(ret_val, mp_obj_new_str("fwversion", 9), mp_obj_new_str(getFWVersionStr(), strlen(getFWVersionStr())));

  mp_obj_dict_store(ret_val, mp_obj_new_str("BSP", 3), mp_obj_new_str(BSP_NAME, strlen(BSP_NAME)));
  mp_obj_dict_store(ret_val, mp_obj_new_str("app", 3), mp_obj_new_str(APP_NAME, strlen(APP_NAME)));

  return ret_val;
}
MP_DEFINE_CONST_FUN_OBJ_0(info_obj, info);

// Define all properties of the module.
// Table entries are key/value pairs of the attribute name (a string)
// and the MicroPython object reference.
// All identifiers and strings are written as MP_QSTR_xxx and will be
// optimized to word-sized integers by the build system (interned strings).
STATIC const mp_rom_map_elem_t example_module_globals_table[] = {
  { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_device) },
  { MP_ROM_QSTR(MP_QSTR_info), MP_ROM_PTR(&info_obj) },

};
STATIC MP_DEFINE_CONST_DICT(example_module_globals, example_module_globals_table);

// Define module object.
const mp_obj_module_t example_user_cmodule = {
  .base = { &mp_type_module },
  .globals = (mp_obj_dict_t *)&example_module_globals,
};

// Register the module to make it available in Python.
MP_REGISTER_MODULE(MP_QSTR_device, example_user_cmodule);
