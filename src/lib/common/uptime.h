#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t uptimeGetMicroSeconds();
uint64_t uptimeGetMs();
uint64_t uptimeGetStartTime();
void uptimeInit();
void uptimeUpdate(uint64_t prevUptimeUS);

#ifdef __cplusplus
}
#endif
