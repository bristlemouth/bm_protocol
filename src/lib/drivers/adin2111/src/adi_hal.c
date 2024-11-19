#include "adi_hal.h"
#include "bsp.h"
#include <string.h>

uint32_t HAL_DisableIrq(void) {
  NVIC_DisableIRQ(ADIN_INT_EXTI_IRQn);
  return ADI_HAL_SUCCESS;
}

uint32_t HAL_EnableIrq(void) {
  NVIC_EnableIRQ(ADIN_INT_EXTI_IRQn);
  return ADI_HAL_SUCCESS;
}

uint32_t HAL_GetPendingIrq(void) { return NVIC_GetPendingIRQ(ADIN_INT_EXTI_IRQn); }

uint32_t HAL_GetEnableIrq(void) { return NVIC_GetEnableIRQ(ADIN_INT_EXTI_IRQn); }

/*
 * @brief  Register SPI Callback function
 *
 * @param [in] spiCallback - Register SPI IRQ callback function
 * @param [in] hDevice - Pointer to Phy device handler
 * @param [out] none
 * @return none
 */

uint32_t HAL_Init_Hook(void) { return ADI_HAL_SUCCESS; }

uint32_t HAL_UnInit_Hook(void) { return ADI_HAL_SUCCESS; }
