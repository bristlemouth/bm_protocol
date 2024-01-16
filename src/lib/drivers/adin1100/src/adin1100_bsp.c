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

#include "adin1100_common.h"
#include "adin1100_bsp.h"

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


// TODO:
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
        // adi_gpio_SetHigh(MDC_PORT, MDC_PIN);
    }
    else
    {
        // adi_gpio_SetLow(MDC_PORT, MDC_PIN);
    }
    return 0;
}

// TODO:
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
//   uint16_t data = 0;
  uint16_t val = 0;
//   adi_gpio_GetData ( MDIO_PORT, MDIO_PIN, &data);
//   if((data & MDIO_PIN) == MDIO_PIN)
//   {
//     val = 1;
//   }
//   else
//   {
//     val = 0;
//   }

return val;
}

// TODO:
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
  if (output == true)
  {
    // adi_gpio_OutputEnable(MDIO_PORT, MDIO_PIN, true);
    // adi_gpio_InputEnable(MDIO_PORT, MDIO_PIN, false);

  }
  else
  {
    // adi_gpio_OutputEnable(MDIO_PORT, MDIO_PIN, false);
    // adi_gpio_InputEnable(MDIO_PORT, MDIO_PIN, true);

  }

}

// TODO:
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
        // adi_gpio_SetHigh(MDIO_PORT, MDIO_PIN);
    }
    else
    {
        // adi_gpio_SetLow(MDIO_PORT, MDIO_PIN);
    }
    return 0;
}

// TODO:
uint32_t BSP_RegisterIRQCallback(ADI_CB const *intCallback, void * hDevice)
{
  uint32_t error = SUCCESS;
  do
  {
    NVIC_DisableIRQ(ADIN1100_INT_N_EXTI_IRQn);
    (void)intCallback;
    (void)hDevice;
    //  if(ADI_GPIO_SUCCESS != adi_gpio_InputEnable(PHY_IRQ_PORT, PHY_IRQ_PIN, true))
    // {
    //     error = FAILURE;
    // }
    // /* Enable pin interrupt on group interrupt A */
    // if(ADI_GPIO_SUCCESS != (adi_gpio_SetGroupInterruptPins(PHY_IRQ_PORT, ADI_GPIO_INTA_IRQ, PHY_IRQ_PIN )))
    // {
    //     break;
    // }
    // /* Register the callback */
    // if(ADI_GPIO_SUCCESS != (adi_gpio_RegisterCallback (ADI_GPIO_INTA_IRQ, (ADI_CALLBACK)intCallback, (void*)hDevice)))
    // {
    //     break;
    // }
    // GPIO priority must be lower then SPI priority!!!
    NVIC_SetPriority(ADIN1100_INT_N_EXTI_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),6, 0));
    NVIC_DisableIRQ(ADIN1100_INT_N_EXTI_IRQn);
  }while(0);
  return error;
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
