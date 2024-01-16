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

/** @addtogroup hal HAL API
 *  @{
 */

#ifndef __ADIN1100_HAL_H__
#define __ADIN1100_HAL_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "adin_eth_common.h"
#include "adin_hal_port_specific.h"
#include "bsp.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*! HAL return code indicating successful execution. */
#define ADI_HAL_SUCCESS         (0)
/*! HAL return code indicating an error has been encountered. */
#define ADI_HAL_ERROR           (1)

#define ADI_PHY_REG_ACCESS_CL22  0x001F

#if defined(ADI_HAL_USE_DEVICE_HANDLE)

#define     ADI_HAL_INIT(...)                       HAL_ADIN1200_Init_Hook(__VA_ARGS__)
#define     ADI_HAL_UNINIT(...)                     HAL_ADIN1200_UnInit_Hook(__VA_ARGS__)
#define     ADI_HAL_PHY_READ(...)                   HAL_ADIN1200_PhyRead(__VA_ARGS__)
#define     ADI_HAL_PHY_WRITE(...)                  HAL_ADIN1200_PhyWrite(__VA_ARGS__)
#define     ADI_HAL_ENABLE_IRQ(...)                 HAL_ADIN1200_EnableIrq(__VA_ARGS__)
#define     ADI_HAL_DISABLE_IRQ(...)                HAL_ADIN1200_DisableIrq(__VA_ARGS__)
#define     ADI_HAL_GET_ENABLE_IRQ(...)             HAL_ADIN1200_GetEnableIrq(__VA_ARGS__)
#define     ADI_HAL_SET_PENDING_IRQ(...)            HAL_ADIN1200_SetPendingIrq(__VA_ARGS__)
#define     ADI_HAL_GET_PENDING_IRQ(...)            HAL_ADIN1200_GetPendingIrq(__VA_ARGS__)
#define     ADI_HAL_REGISTER_CALLBACK(...)          HAL_ADIN1200_RegisterCallback(__VA_ARGS__)
// #define     ADI_HAL_SPI_REGISTER_CALLBACK(...)      HAL_SpiRegisterCallback(__VA_ARGS__)
// #define     ADI_HAL_SPI_READ_WRITE(...)             HAL_SpiReadWrite(__VA_ARGS__)
#define     ADI_HAL_FCS_INIT(...)                   HAL_ADIN1200_FcsInit(__VA_ARGS__)
#define     ADI_HAL_FCS_UNINIT...)                  HAL_ADIN1200_FcsUnInit(__VA_ARGS__)
#define     ADI_HAL_FCS_CALCULATE(...)              HAL_ADIN1200_FcsCalculate(__VA_ARGS__)

#else

#define     ADI_HAL_INIT(dev, ...)                  HAL_ADIN1200_Init_Hook(__VA_ARGS__)
#define     ADI_HAL_UNINIT(dev, ...)                HAL_ADIN1200_UnInit_Hook(__VA_ARGS__)
#define     ADI_HAL_PHY_READ(dev, ...)              HAL_ADIN1200_PhyRead(__VA_ARGS__)
#define     ADI_HAL_PHY_WRITE(dev, ...)             HAL_ADIN1200_PhyWrite(__VA_ARGS__)
#define     ADI_HAL_ENABLE_IRQ(dev, ...)            HAL_ADIN1200_EnableIrq(__VA_ARGS__)
#define     ADI_HAL_DISABLE_IRQ(dev, ...)           HAL_ADIN1200_DisableIrq(__VA_ARGS__)
#define     ADI_HAL_GET_ENABLE_IRQ(dev, ...)        HAL_ADIN1200_GetEnableIrq(__VA_ARGS__)
#define     ADI_HAL_SET_PENDING_IRQ(dev, ...)       HAL_ADIN1200_SetPendingIrq(__VA_ARGS__)
#define     ADI_HAL_GET_PENDING_IRQ(dev, ...)       HAL_ADIN1200_GetPendingIrq(__VA_ARGS__)
#define     ADI_HAL_REGISTER_CALLBACK(dev, ...)     HAL_ADIN1200_RegisterCallback(__VA_ARGS__)
// #define     ADI_HAL_SPI_REGISTER_CALLBACK(dev, ...) HAL_SpiRegisterCallback(__VA_ARGS__)
// #define     ADI_HAL_SPI_READ_WRITE(dev, ...)        HAL_SpiReadWrite(__VA_ARGS__)
#define     ADI_HAL_FCS_INIT(dev, ...)              HAL_ADIN1200_FcsInit(__VA_ARGS__)
#define     ADI_HAL_FCS_UNINIT(dev, ...)            HAL_ADIN1200_FcsUnInit(__VA_ARGS__)
#define     ADI_HAL_FCS_CALCULATE(dev, ...)         HAL_ADIN1200_FcsCalculate(__VA_ARGS__)

#endif

#define     ADI_HAL_ENTER_CRITICAL_SECTION(...)     __disable_irq()
#define     ADI_HAL_EXIT_CRITICAL_SECTION(...)      __enable_irq()

typedef void (*HAL_Callback_t) (    /*!< Callback function pointer */
    void      *pCBParam,            /*!< Client supplied callback param */
    uint32_t   Event,               /*!< Event ID specific to the Driver/Service */
    void      *pArg);               /*!< Pointer to the event specific argument */

#if defined(ADIN1100)

/*!
 * @brief           HAL read from PHY register.
 *
 * @param [in]      phyAddr     PHY address.
 * @param [in]      regAddr     Register address as represented in the header file (DEVTYPE + ADDR).
 * @param [out]     data        16-bit data read from the register.
 *
 * @return          Status
 *                  - #ADI_HAL_SUCCESS      Call completed successfully.
 *                  - #ADI_HAL_ERROR        Error encountered during function execution.
 *
 * @details         Reads from a PHY register using the MDIO interface.
 *
 *                  Implements a blocking call.
 *
 * @sa              HAL_ADIN1200_PhyWrite()
 */
uint32_t        HAL_ADIN1200_PhyRead             (uint8_t phyAddr, uint32_t regAddr, uint16_t *data);

/*!
 * @brief           HAL write to PHY register.
 *
 * @param [in]      phyAddr     PHY address.
 * @param [in]      regAddr     Register address as represented in the header file (DEVTYPE + ADDR).
 * @param [out]     data        16-bit data read from the register.
 *
 * @return          Status
 *                  - #ADI_HAL_SUCCESS      Call completed successfully.
 *                  - #ADI_HAL_ERROR        Error encountered during function execution.
 *
 * @details         Write to a PHY register using the MDIO interface.
 *
 *                  Implements a blocking call.
 *
 * @sa              HAL_ADIN1200_PhyRead()
 */
uint32_t        HAL_ADIN1200_PhyWrite            (uint8_t phyAddr, uint32_t regAddr, uint16_t data);
#endif

/*!
 * @brief           Interrupt enable.
 *
 * @return          Status
 *                  - #ADI_HAL_SUCCESS      Call completed successfully.
 *                  - #ADI_HAL_ERROR        Error encountered during function execution.
 *
 * @details         Enables the external interrupt (INT_N) in the host processor.
 *
 * @sa              HAL_ADIN1200_DisableIrq()
 */
uint32_t        HAL_ADIN1200_EnableIrq           (void);

/*!
 * @brief           Interrupt disable.
 *
 * @return          Status
 *                  - #ADI_HAL_SUCCESS      Call completed successfully.
 *                  - #ADI_HAL_ERROR        Error encountered during function execution.
 *
 * @details         Disables the external interrupt (INT_N) in the host processor.
 *
 * @sa              HAL_ADIN1200_EnableIrq()
 */
uint32_t        HAL_ADIN1200_DisableIrq          (void);

/*!
 * @brief           Get interrupt enable status.
 *
 * @return          Enable status
 *                  - 1         Interrupt enabled.
 *                  - 0         Interrupt disabled.
 *
 * @details         Disables the external interrupt (INT_N) in the host processor.
 *
 * @sa
 */
uint32_t        HAL_ADIN1200_GetEnableIrq        (void);

/*!
 * @brief           Set pending interrupt.
 *
 * @return          Status
 *                  - #ADI_HAL_SUCCESS      Call completed successfully.
 *                  - #ADI_HAL_ERROR        Error encountered during function execution.
 *
 * @details         Sets the external interrupt (INT_N) as pending causing the IRQ handler
 *                  to be executed. This is used for deferred operations.
 *
 * @sa
 */
uint32_t        HAL_ADIN1200_SetPendingIrq       (void);


uint32_t        HAL_ADIN1200_GetPendingIrq       (void);


/*!
 * @brief           Register device IRQ callback function.
 *
 * @param [in]      intCallback     Callback function.
 * @param [in]      hDevice         Pointer to device handler.
 * @return          Status
 *                  - #ADI_HAL_SUCCESS      Call completed successfully.
 *                  - #ADI_HAL_ERROR        Error encountered during function execution.
 *
 * @details         Registers a callback for the PHY interrupt.
 *
 * @sa
 */
uint32_t        HAL_ADIN1200_RegisterCallback    (HAL_Callback_t const *intCallback, void *hDevice);


#if defined(ADIN1110) || defined(ADIN2111)
/*!
 * @brief           Register SPI IRQ callback function.
 *
 * @param [in]      intCallback     Callback function.
 * @param [in]      hDevice         Pointer to device handler.
 * @return          Status
 *                  - #ADI_HAL_SUCCESS      Call completed successfully.
 *                  - #ADI_HAL_ERROR        Error encountered during function execution.
 *
 * @details         Registers a callback for the SPI interrupt.
 *
 * @sa
 */
// uint32_t        HAL_SpiRegisterCallback (HAL_Callback_t const *spiCallback, void * hDevice);

/*!
 * @brief           HAL spi read/write.
 *
 * @param [in]      pBufferTx   Pointer to transmit data buffer.
 * @param [in]      nbBytes   Number bytes to transfer.
 * @param [in]      useDma   Enable/disable DMA for SPI.
 * @param [out]     pBufferRx   Pointer to reveive data buffer.
 *
 * @return          Status
 *                  - #ADI_HAL_SUCCESS      Call completed successfully.
 *                  - #ADI_HAL_ERROR        Error encountered during function execution.
 *
 * @details         Initiate data transfer using the SPI interface.
 *
 *                  Implements a non blocking call.
 *
 * @sa              HAL_SpiReadWrite()
 */
// uint32_t        HAL_SpiReadWrite (uint8_t *pBufferTx, uint8_t *pBufferRx, uint32_t nbBytes, bool useDma);

/*!
 * @brief           HAL Frame check sequence (FCS) initialization.
 *
 * @return          Status
 *                  - #ADI_HAL_SUCCESS      Call completed successfully.
 *                  - #ADI_HAL_ERROR        Error encountered during function execution.
 *
 * @details         Initialize FCS calculator.
 *
 * @sa
 */
uint32_t        HAL_ADIN1200_FcsInit (void);

/*!
 * @brief           HAL Frame check sequence (FCS) uninitialization.
 *
 * @return          Status
 *                  - #ADI_HAL_SUCCESS      Call completed successfully.
 *                  - #ADI_HAL_ERROR        Error encountered during function execution.
 *
 * @details         Uninitialize FCS calculator.
 *
 * @sa
 */
uint32_t        HAL_ADIN1200_FcsUnInit (void);

/*!
 * @brief           HAL Frame check sequence (FCS) calculation.
 *
 * @param [in]      pBuf      Pointer to data buffer on which to perfrom the calculation.
 * @param [in]      nbBytes   Number of bytes on which to perfrom the calculation.
 *
 * @return          4-byte Ethernet FCS as U32.
 *
 * @details         Calculate and return the 32-bit FCS. Per the 802.3 standard,
 *                  the computation is via CRC32 BZIP2 with polynomial = 0x4C11DB7,
 *                  initial CRC = 0xFFFFFFFF, and with the CRC post-complemented.
 *
 * @sa
 */
uint32_t        HAL_ADIN1200_FcsCalculate (uint8_t *pBuf, uint32_t nbBytes);

#endif

/*!
 * @brief           HAL initialization hook.
 *
 * @return          Status
 *                  - #ADI_HAL_SUCCESS      Call completed successfully.
 *                  - #ADI_HAL_ERROR        Error encountered during function execution.
 *
 * @details         Called during the driver initialization, to allow the user to run application-specific
 *                  configuration, if needed. Can be left empty if such functionality is not required.
 *
 * @sa              HAL_ADIN1200_UnInit_Hook()
 */
uint32_t        HAL_ADIN1200_Init_Hook(void);

/*!
 * @brief           HAL uninitialization hook.
 *
 * @return          Status
 *                  - #ADI_HAL_SUCCESS      Call completed successfully.
 *                  - #ADI_HAL_ERROR        Error encountered during function execution.
 *
 * @details         Called during the driver uninitialization, to allow the user to run application-specific
 *                  configuration, if needed. Can be left empty if such functionality is not required.
 *
 * @sa              HAL_ADIN1200_Init_Hook()
 */
uint32_t        HAL_ADIN1200_UnInit_Hook(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADIN1100_HAL_H__ */

/** @}*/
