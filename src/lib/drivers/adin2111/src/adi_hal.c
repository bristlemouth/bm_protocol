#include "adi_hal.h"
#include "adi_bsp.h"
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
 * @brief  Register Phy IRQ Callback function
 *
 * @param [in] intCallback
 * @param [in] hDevice - Pointer to Phy device handler
 * @param [out] none
 * @return none
 */
uint32_t HAL_RegisterCallback(HAL_Callback_t const *intCallback, void *hDevice) {
  return adi_bsp_register_irq_callback(intCallback, hDevice);
}

/*
 * @brief SPI write/read
 *
 * @param [in] pBufferTx - Pointer to transmit buffer
 * @param [in] nbBytes - Number bytes to send
 * @param [in] useDma - Enable/Disable DMA transfer for SPI
 * @param [out] pBufferRx - Pointer to receive buffer
 * @return none
 */
uint32_t HAL_SpiReadWrite(uint8_t *pBufferTx, uint8_t *pBufferRx, uint32_t nbBytes,
                          bool useDma) {
  return adi_bsp_spi_write_and_read(pBufferTx, pBufferRx, nbBytes, useDma);
}

/*
 * @brief  Register SPI Callback function
 *
 * @param [in] spiCallback - Register SPI IRQ callback function
 * @param [in] hDevice - Pointer to Phy device handler
 * @param [out] none
 * @return none
 */
uint32_t HAL_SpiRegisterCallback(HAL_Callback_t const *spiCallback, void *hDevice) {
  return adi_bsp_spi_register_callback(spiCallback, hDevice);
}

uint32_t HAL_Init_Hook(void) { return ADI_HAL_SUCCESS; }

uint32_t HAL_UnInit_Hook(void) { return ADI_HAL_SUCCESS; }
