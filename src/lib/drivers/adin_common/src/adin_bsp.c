/*
 *---------------------------------------------------------------------------
 *
 * Copyright (c) 2020 Analog Devices, Inc. All Rights Reserved.
 * This software is proprietary to Analog Devices, Inc.
 * and its licensors.By using this software you agree to the terms of the
 * associated Analog Devices Software License Agreement.
 *
 *---------------------------------------------------------------------------
 */

#include <ctype.h>

#include "adin_common.h"
#include "adin_bsp.h"
#include "stm32_io.h"

static ADI_CB gpfIntCallback = NULL;
static void *gpIntCBParam = NULL;


static bool adin1100_bsp_gpio_callback_fromISR(const void *pinHandle, uint8_t value, void *args);

//
// IRQ callback (called from eth_adin2111 event task)
//
void adin1100_bsp_irq_callback() {
  if (gpfIntCallback) {
    (*gpfIntCallback)(gpIntCBParam, 0, NULL);
  } else {
    /* No GPIO Callback function assigned */
  }
}

//
// ADIN GPIO ISR callback (All calls must be ISR safe!)
//
static bool adin1100_bsp_gpio_callback_fromISR(const void *pinHandle, uint8_t value, void *args)
{
  (void)pinHandle;

  if (gpfIntCallback) {
    (*gpfIntCallback)(gpIntCBParam, (uint32_t)value, args);
  } else {
    /* No GPIO Callback function assigned */
  }

  return false;
}

void BSP_ClrPendingIRQ(void)
{
    NVIC_ClearPendingIRQ(ADIN1100_INT_N_EXTI_IRQn);
}
void BSP_EnableIRQ(void)
{
    NVIC_EnableIRQ(ADIN1100_INT_N_EXTI_IRQn);
}
void BSP_DisableIRQ(void)
{
    NVIC_DisableIRQ(ADIN1100_INT_N_EXTI_IRQn);
}

/*
 * @brief Helper for MDIO GPIO Clock toggle
 *
 * @param [in]      set - true/false
 * @param [out]     none
 * @return 0
 *
 * @details
 *
 * @sa
 */
uint32_t BSP_SetPinMDC(bool set)
{
    if (set)
    {
        IOWrite(&ADIN1100_MDC, 1);
    }
    else
    {
        IOWrite(&ADIN1100_MDC, 0);
    }
    return 0;
}

/*
 * @brief Helper for MDIO GPIO Read Pin value
 *
 * @param [in]      none
 * @param [out]     none
 * @return pin value
 *
 * @details
 *
 * @sa
 */
uint16_t BSP_GetPinMDInput(void)
{
  uint8_t data = 0;

  IORead(&ADIN1100_MDIO, &data);

  return (uint16_t)data;
}

/*
 * @brief Helper for MDIO GPIO Changes pin in/out
 *
 * @param [in]      output - true/false
 * @param [out]     none
 * @return none
 *
 * @details
 *
 * @sa
 */
void BSP_ChangeMDIPinDir(bool output)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
  const STM32Pin_t* pin = ADIN1100_MDIO.pin;

  if (output == true)
  {
    // // Bring the level back to high
    // LL_GPIO_SetOutputPin(pin->gpio, pin->pinmask);

    GPIO_InitStruct.Pin = pin->pinmask;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_MEDIUM;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(pin->gpio, &GPIO_InitStruct);
  }
  else
  {
    GPIO_InitStruct.Pin = pin->pinmask;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(pin->gpio, &GPIO_InitStruct);
  }

}

/*
 * @brief Helper for MDIO GPIO Changes ouptut pin value
 *
 * @param [in]      output - true/false
 * @param [out]     none
 * @return 0
 *
 * @details
 *
 * @sa
 */
uint32_t BSP_SetPinMDIO(bool set)
{
    if (set)
    {
        IOWrite(&ADIN1100_MDIO, 1);
    }
    else
    {
        IOWrite(&ADIN1100_MDIO, 0);
    }
    return 0;
}

uint32_t BSP_RegisterIRQCallback(ADI_CB const *intCallback, void * hDevice)
{
    NVIC_DisableIRQ(ADIN1100_INT_N_EXTI_IRQn);

    IORegisterCallback(&ADIN1100_INT_N, adin1100_bsp_gpio_callback_fromISR, NULL);
    gpfIntCallback = (ADI_CB)intCallback;
    gpIntCBParam = hDevice;

    NVIC_SetPriority(ADIN1100_INT_N_EXTI_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),6, 0));
    NVIC_DisableIRQ(ADIN1100_INT_N_EXTI_IRQn);

    return 0;
}

/*
* Debug outout to UART
*/
void msgWrite(char * ptr)
{
   printf("%s", ptr );
}

void common_Fail(char *FailureReason)
{
    char fail[] = "Failed: ";
    char term[] = "\n\r";

    /* Ignore return codes since there's nothing we can do if it fails */
    msgWrite(fail);
    msgWrite(FailureReason);
    msgWrite(term);
 }

void common_Perf(char *InfoString)
{
    char term[] = "\n\r";

    /* Ignore return codes since there's nothing we can do if it fails */
    msgWrite(InfoString);
    msgWrite(term);
}
