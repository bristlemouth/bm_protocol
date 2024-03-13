#include "FreeRTOS.h"
#include "io_adc.h"
#include "adc.h"
#include "debug.h"

bool IOAdcInit(void *handle) {
  configASSERT(handle != 0);
  // ADC_HandleTypeDef *adcHandle = (ADC_HandleTypeDef *)handle;

  MX_ADC1_Init();

  // Calibrate ADC
  // HAL_ADCEx_Calibration_Start(adcHandle, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
  return true;
}

bool IOAdcDeInit(void *handle) {
  configASSERT(handle != 0);
  ADC_HandleTypeDef *adcHandle = (ADC_HandleTypeDef *)handle;

  HAL_ADC_DeInit(adcHandle);
  return true;
}

AdcStatus_t IOAdcRead(void *handle, int32_t *value) {
  uint16_t rval = 0;
  configASSERT(handle != 0);
  ADC_HandleTypeDef *adcHandle = (ADC_HandleTypeDef *)handle;

  HAL_ADC_Start(adcHandle);
  // TODO - use interrupts
  uint8_t poll_rval = HAL_ADC_PollForConversion(adcHandle, 2000);
  if (poll_rval != HAL_OK) {
    printf("ADC Poll for conversion failed: %u\n", poll_rval);
    rval = ADC_ERR;
  }
  int32_t lResult = HAL_ADC_GetValue(adcHandle);
  HAL_ADC_Stop(adcHandle);

  if (value != NULL) {
    *value = lResult;
  }

  return rval;
}

AdcStatus_t IOAdcReadMv(void *handle, int32_t *valueMv) {
  configASSERT(handle != NULL);
  configASSERT(valueMv != NULL);

  AdcStatus_t rval = IOAdcRead(handle, valueMv);
  if (rval == ADC_OK) {
    printf("ADC value: %d\n", *valueMv);
    // *valueMv = (*valueMv * 3300)/(1 << 14);
  }

  return rval;
}

AdcStatus_t IOAdcConfig(void *handle, void *config) {
  (void)handle;
  (void)config;

  // Not implemented since adc config is taken care of by the handle and adcinit

  return 0;
}

AdcStatus_t IOAdcChannelConfig(void *handle, void *channelConfig) {
  configASSERT(handle != 0);
  ADC_HandleTypeDef *adcHandle = (ADC_HandleTypeDef *)handle;
  ADC_ChannelConfTypeDef *adcConfig = (ADC_ChannelConfTypeDef *)channelConfig;

  AdcStatus_t rval = ADC_OK;

  if (HAL_ADC_ConfigChannel(adcHandle, adcConfig) != HAL_OK) {
    rval = ADC_ERR;
  }

  return rval;
}
