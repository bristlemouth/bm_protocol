#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  ADC_OK = 0,
  ADC_ERR,
}AdcStatus_t;

bool IOAdcInit(void *handle);
bool IOAdcDeInit(void *handle);

AdcStatus_t IOAdcRead(void *handle, int32_t *value);
AdcStatus_t IOAdcReadMv(void *handle, int32_t *valueMv);
AdcStatus_t IOAdcConfig(void *handle, void *config);
AdcStatus_t IOAdcChannelConfig(void *handle, void *channelConfig);

#ifdef __cplusplus
}
#endif
