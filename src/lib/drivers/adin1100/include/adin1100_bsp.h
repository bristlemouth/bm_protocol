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

#ifndef __ADIN1100_BSP_H__
#define __ADIN1100_BSP_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "bsp.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (* ADI_CB) (  /*!< Callback function pointer */
    void      *pCBParam,         /*!< Client supplied callback param */
    uint32_t   Event,            /*!< Event ID specific to the Driver/Service */
    void      *pArg);            /*!< Pointer to the event specific argument */

// Function pointer to spi/irq event callback
typedef void (*adin1100_irq_evt_t)(BaseType_t *pxHigherPriorityTaskWoken);

/*Functions prototypes*/
uint32_t BSP_RegisterIRQCallback(ADI_CB const *intCallback, void * hDevice);
void BSP_DisableIRQ(void);
void BSP_EnableIRQ(void);
uint32_t BSP_SetPinMDC(bool set);
uint32_t BSP_SetPinMDIO(bool set);
uint16_t BSP_GetPinMDInput(void);
void BSP_ChangeMDIPinDir(bool output);

void adin1100_bsp_register_irq_evt(adin1100_irq_evt_t irq_evt_cb);

extern void msgWrite(char * ptr);

#ifdef __cplusplus
}
#endif

#endif /* __ADIN1100_BSP_H__ */
