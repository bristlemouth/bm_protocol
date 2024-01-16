/*
 *---------------------------------------------------------------------------
 *
 * Copyright (c) 2021 Analog Devices, Inc. All Rights Reserved.
 * This software is proprietary to Analog Devices, Inc.
 * and its licensors.By using this software you agree to the terms of the
 * associated Analog Devices Software License Agreement.
 *
 *---------------------------------------------------------------------------
 */

#ifndef __ADIN1100_H__
#define __ADIN1100_H__

#include "adin1100_phy.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** @addtogroup adin1100 ADIN1100 PHY Driver
 *  @{
 */

/*! ADIN1100 driver major version. */
#define ADIN1100_VERSION_MAJOR      (0)
/*! ADIN1100 driver minor version. */
#define ADIN1100_VERSION_MINOR      (4)
/*! ADIN1100 driver patch version. */
#define ADIN1100_VERSION_PATCH      (0)
/*! ADIN1100 driver extra version. */
#define ADIN1100_VERSION_EXTRA      (0)

/*! ADIN1100 driver version. */
#define ADIN1100_VERSION            ((ADIN1100_VERSION_MAJOR << 24) | \
                                     (ADIN1100_VERSION_MINOR << 16) | \
                                     (ADIN1100_VERSION_PATCH << 8) | \
                                     (ADIN1100_VERSION_EXTRA))


/*!
* @brief ADIN1100 device identification.
*/
typedef struct
{
    union {
        struct {
            uint32_t    revNum      : 4;    /*!< Revision number.           */
            uint32_t    modelNum    : 6;    /*!< Model number.              */
            uint32_t    oui         : 22;   /*!< OUI.                       */
        };
        uint32_t phyId;
    };
    uint16_t        digRevNum;              /*!< Digital revision number.   */
    uint8_t         pkgType;                /*!< Package type.              */
} adin1100_DeviceId_t;

/*!
* @brief ADIN1100 device structure.
*/
typedef struct
{
    adi_phy_Device_t    *pPhyDevice;    /*!< Pointer to the PHY device structure.   */
} adin1100_DeviceStruct_t;

/*!
* @brief ADIN1100 device structure handle.
*/
typedef adin1100_DeviceStruct_t*    adin1100_DeviceHandle_t;

adi_eth_Result_e    adin1100_Init                    (adin1100_DeviceHandle_t hDevice, adi_phy_DriverConfig_t *cfg);
adi_eth_Result_e    adin1100_UnInit                  (adin1100_DeviceHandle_t hDevice);
adi_eth_Result_e    adin1100_ReInitPhy               (adin1100_DeviceHandle_t hDevice);

adi_eth_Result_e    adin1100_GetDeviceId             (adin1100_DeviceHandle_t hDevice, adin1100_DeviceId_t *pDevId);

adi_eth_Result_e    adin1100_RegisterCallback        (adin1100_DeviceHandle_t hDevice, adi_eth_Callback_t cbFunc, uint32_t cbEvents);
adi_eth_Result_e    adin1100_ReadIrqStatus           (adin1100_DeviceHandle_t hDevice, uint32_t *status);

adi_eth_Result_e    adin1100_AnAdvTxMode             (adin1100_DeviceHandle_t hDevice, adi_phy_AnAdvTxMode_e txMode);
adi_eth_Result_e    adin1100_AnAdvMstSlvCfg          (adin1100_DeviceHandle_t hDevice, adi_phy_AnAdvMasterSlaveCfg_e msCfg);
adi_eth_Result_e    adin1100_AnEnable                (adin1100_DeviceHandle_t hDevice, bool enable);

adi_eth_Result_e    adin1100_EnterSoftwarePowerdown  (adin1100_DeviceHandle_t hDevice);
adi_eth_Result_e    adin1100_ExitSoftwarePowerdown   (adin1100_DeviceHandle_t hDevice);
adi_eth_Result_e    adin1100_GetSoftwarePowerdown    (adin1100_DeviceHandle_t hDevice, bool *flag);

adi_eth_Result_e    adin1100_GetLinkStatus           (adin1100_DeviceHandle_t hDevice, adi_phy_LinkStatus_e *status);

adi_eth_Result_e    adin1100_Renegotiate             (adin1100_DeviceHandle_t hDevice);
adi_eth_Result_e    adin1100_GetAnStatus             (adin1100_DeviceHandle_t hDevice, adi_phy_AnStatus_t *status);

adi_eth_Result_e    adin1100_Reset                   (adin1100_DeviceHandle_t hDevice, adi_phy_ResetType_e resetType);
adi_eth_Result_e    adin1100_SetLoopbackMode         (adin1100_DeviceHandle_t hDevice, adi_phy_LoopbackMode_e loopbackMode);
adi_eth_Result_e    adin1100_SetTestMode             (adin1100_DeviceHandle_t hDevice, adi_phy_TestMode_e testMode);

#if defined(ADIN_S1)
adi_eth_Result_e    adin1100_LedEn                   (adin1100_DeviceHandle_t hDevice, bool enable);
adi_eth_Result_e    adin1100_LedBlinkTime            (adin1100_DeviceHandle_t hDevice, uint32_t blinkOn, uint32_t blinkOff);
#else
adi_eth_Result_e    adin1100_LedEn                   (adin1100_DeviceHandle_t hDevice, adi_phy_LedPort_e ledSel, bool enable);
adi_eth_Result_e    adin1100_LedBlinkTime            (adin1100_DeviceHandle_t hDevice, adi_phy_LedPort_e ledSel, uint32_t blinkOn, uint32_t blinkOff);
#endif

adi_eth_Result_e    adin1100_GetCapabilities         (adin1100_DeviceHandle_t hDevice, uint16_t *capabilities);

adi_eth_Result_e    adin1100_PhyWrite                (adin1100_DeviceHandle_t hDevice, uint32_t regAddr, uint16_t regData);
adi_eth_Result_e    adin1100_PhyRead                 (adin1100_DeviceHandle_t hDevice, uint32_t regAddr, uint16_t *regData);

adi_eth_Result_e    adin1100_GetMseLinkQuality       (adin1100_DeviceHandle_t hDevice, adi_phy_MseLinkQuality_t *mseLinkQuality);

adi_eth_Result_e    adin1100_FrameGenEn              (adin1100_DeviceHandle_t hDevice, bool enable);
adi_eth_Result_e    adin1100_FrameGenSetMode         (adin1100_DeviceHandle_t hDevice, adi_phy_FrameGenMode_e mode);
adi_eth_Result_e    adin1100_FrameGenSetFrameCnt     (adin1100_DeviceHandle_t hDevice, uint32_t frameCnt);
adi_eth_Result_e    adin1100_FrameGenSetPayload      (adin1100_DeviceHandle_t hDevice, adi_phy_FrameGenPayload_e payload);
adi_eth_Result_e    adin1100_FrameGenSetFrameLen     (adin1100_DeviceHandle_t hDevice, uint16_t frameLen);
adi_eth_Result_e    adin1100_FrameGenSetIfgLen       (adin1100_DeviceHandle_t hDevice, uint16_t ifgLen);
adi_eth_Result_e    adin1100_FrameGenRestart         (adin1100_DeviceHandle_t hDevice);
adi_eth_Result_e    adin1100_FrameGenDone            (adin1100_DeviceHandle_t hDevice, bool *fgDone);

adi_eth_Result_e    adin1100_FrameChkEn              (adin1100_DeviceHandle_t hDevice, bool enable);
adi_eth_Result_e    adin1100_FrameChkSourceSelect    (adin1100_DeviceHandle_t hDevice, adi_phy_FrameChkSource_e source);
adi_eth_Result_e    adin1100_FrameChkReadFrameCnt    (adin1100_DeviceHandle_t hDevice, uint32_t *cnt);
adi_eth_Result_e    adin1100_FrameChkReadRxErrCnt    (adin1100_DeviceHandle_t hDevice, uint16_t *cnt);
adi_eth_Result_e    adin1100_FrameChkReadErrorCnt    (adin1100_DeviceHandle_t hDevice, adi_phy_FrameChkErrorCounters_t *cnt);

/** @}*/

#ifdef __cplusplus
}
#endif

#endif /* __ADIN1100_H__ */
