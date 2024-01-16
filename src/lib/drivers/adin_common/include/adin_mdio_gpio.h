/*
 *---------------------------------------------------------------------------
 *
 * Copyright (c) 2020 Analog Devices, Inc. All Rights Reserved.
 * This software is proprietary <and confidential> to Analog Devices, Inc.
 * and its licensors.By using this software you agree to the terms of the
 * associated Analog Devices Software License Agreement.
 *
 *---------------------------------------------------------------------------
 */

#ifndef __ADIN_COMMON_ADIN_MDIO_GPIO_H__
#define __ADIN_COMMON_ADIN_MDIO_GPIO_H__

#include <stdint.h>
#include "adin_bsp.h"

uint32_t mdioGPIORead(uint8_t phyAddr, uint8_t phyReg, uint16_t *phyData );
uint32_t mdioGPIOWrite(uint8_t phyAddr, uint8_t phyReg, uint16_t phyData );
uint32_t mdioGPIORead_cl45(uint8_t phyAddr, uint32_t phyReg, uint16_t *phyData );
uint32_t mdioGPIOWrite_cl45(uint8_t phyAddr, uint32_t phyReg, uint16_t phyData );
uint32_t mdioGPIORead_cl22(uint8_t phyAddr, uint32_t phyReg, uint16_t *phyData );
uint32_t mdioGPIOWrite_cl22(uint8_t phyAddr, uint32_t phyReg, uint16_t phyData );


#endif /* __ADIN_COMMON_ADIN_MDIO_GPIO_H__ */
