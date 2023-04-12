#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

#define RAM_HARDWARE_CONFIG_SIZE_BYTES  (10 * 1024)
#define RAM_SYSTEM_CONFIG_SIZE_BYTES    (10 * 1024)
#define RAM_USER_CONFIG_SIZE_BYTES      (10 * 1024)

extern uint8_t ram_hardware_configuration[RAM_HARDWARE_CONFIG_SIZE_BYTES];
extern uint8_t ram_system_configuration[RAM_SYSTEM_CONFIG_SIZE_BYTES];
extern uint8_t ram_user_configuration[RAM_USER_CONFIG_SIZE_BYTES];

#ifdef __cplusplus
}
#endif
