/*
 *---------------------------------------------------------------------------
 *
 * Copyright (c) 2020, 2021 Analog Devices, Inc. All Rights Reserved.
 * This software is proprietary to Analog Devices, Inc.
 * and its licensors.By using this software you agree to the terms of the
 * associated Analog Devices Software License Agreement.
 *
 *---------------------------------------------------------------------------
 */

#ifndef __ADI_BSP_H__
#define __ADI_BSP_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef void (* adi_cb_t) (  /*!< Callback function pointer */
    void      *pCBParam,         /*!< Client supplied callback param */
    uint32_t   Event,            /*!< Event ID specific to the Driver/Service */
    void      *pArg);            /*!< Pointer to the event specific argument */

/*Functions prototypes*/
uint32_t adi_bsp_register_irq_callback (adi_cb_t const *intCallback, void * hDevice);
uint32_t adi_bsp_spi_write_and_read (uint8_t *pBufferTx, uint8_t *pBufferRx, uint32_t nbBytes, bool useDma);
uint32_t adi_bsp_spi_register_callback (adi_cb_t const *pfCallback, void *const pCBParam);

void adi_bsp_hw_reset();
void adi_bsp_int_n_set_pending_irq(void);
uint32_t adi_bsp_init(void);

#endif /* __ADI_BSP_H__ */
