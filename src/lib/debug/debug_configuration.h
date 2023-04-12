#pragma once

#include <stdint.h>
#include "configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace cfg;

void debugConfigurationInit(Configuration *user_config, Configuration *hardware_config ,Configuration *system_config);

#ifdef __cplusplus
}
#endif
