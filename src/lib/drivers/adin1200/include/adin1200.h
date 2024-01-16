/*
 *---------------------------------------------------------------------------
 *
 * Copyright (c) 2022 Analog Devices, Inc. All Rights Reserved.
 * This software is proprietary to Analog Devices, Inc.
 * and its licensors.By using this software you agree to the terms of the
 * associated Analog Devices Software License Agreement.
 *
 *---------------------------------------------------------------------------
 */

#ifndef ADINPHY_H
#define ADINPHY_H

#include "adin1200_phy.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** @addtogroup phy PHY Definitions
 *  @{
 */

/*! ADINPHY driver major version. */
#define ADINPHY_VERSION_MAJOR      (1)
/*! ADINPHY driver minor version. */
#define ADINPHY_VERSION_MINOR      (0)
/*! ADINPHY driver patch version. */
#define ADINPHY_VERSION_PATCH      (0)
/*! ADINPHY driver extra version. */
#define ADINPHY_VERSION_EXTRA      (0)

/*! ADINPHY driver version. */
#define ADINPHY_VERSION              ((ADINPHY_VERSION_MAJOR << 24) | \
                                     (ADINPHY_VERSION_MINOR << 16) | \
                                     (ADINPHY_VERSION_PATCH << 8) | \
                                     (ADINPHY_VERSION_EXTRA))





/*!
* @brief ADINPHY device structure.
*/
typedef struct
{
    adin1200_phy_Device_t    *pPhyDevice;    /*!< Pointer to the PHY device structure.   */
} adinPhy_DeviceStruct_t;

/*!
* @brief ADINPHY device structure handle.
*/
typedef adinPhy_DeviceStruct_t*    adinPhy_DeviceHandle_t;

/* The PHY driver functions exposed to the user application */
adi_eth_Result_e    adin_Init                    (adinPhy_DeviceHandle_t hDevice, adi_phy_DriverConfig_t *cfg);
adi_eth_Result_e    adin_UnInit                  (adinPhy_DeviceHandle_t hDevice);
adi_eth_Result_e    adin_ReInitPhy               (adinPhy_DeviceHandle_t hDevice);
adi_eth_Result_e    adin_GetDeviceId             (adinPhy_DeviceHandle_t hDevice, adinPhy_DeviceId_t *pDevId);

adi_eth_Result_e    adin_RegisterCallback        (adinPhy_DeviceHandle_t hDevice, adi_eth_Callback_t cbFunc, uint32_t cbEvents);
adi_eth_Result_e    adin_ReadIrqStatus           (adinPhy_DeviceHandle_t hDevice, uint32_t *status);

adi_eth_Result_e    adin_SoftwarePowerdown       (adinPhy_DeviceHandle_t hDevice, adi_phy_swpd_e swpd);
adi_eth_Result_e    adin_GetSoftwarePowerdown    (adinPhy_DeviceHandle_t hDevice, bool *flag);

adi_eth_Result_e    adin_GetLinkStatus           (adinPhy_DeviceHandle_t hDevice, adi_phy_LinkStatus_e *status);
adi_eth_Result_e    adin_LinkConfig              (adinPhy_DeviceHandle_t hDevice, bool enable);

adi_eth_Result_e    adin_AnegEnable              (adinPhy_DeviceHandle_t hDevice, adi_phy_AnegEn_e enable);
adi_eth_Result_e    adin_AnegAdvertised          (adinPhy_DeviceHandle_t hDevice, uint16_t *phy_speed, uint16_t *phy_speed1G);
adi_eth_Result_e    adin_AnegRestart             (adinPhy_DeviceHandle_t hDevice);
adi_eth_Result_e    adin_GetAnStatus             (adinPhy_DeviceHandle_t hDevice, adi_phy_AnegStatus_e *status);
adi_eth_Result_e    adin_AnegAdvertiseConfig     (adinPhy_DeviceHandle_t hDevice, adi_phy_AnegAdv_e anegAdver);

adi_eth_Result_e    adin_AutoMDIXConfig          (adinPhy_DeviceHandle_t hDevice, adi_phy_AutoMDIXConfig_e autoMdixConfig);

adi_eth_Result_e    adin_GetEDetectPWDStatus     (adinPhy_DeviceHandle_t hDevice, adi_Energy_Detect_PWD_Status_e *status);
adi_eth_Result_e    adin_ConfigEnergyDetectPWD   (adinPhy_DeviceHandle_t hDevice, adi_Energy_Detect_PWD_e EnergyDetectPWD);

adi_eth_Result_e    adin_ConfigDownspeed         (adinPhy_DeviceHandle_t hDevice, adi_phy_downspeed_e downspeedConfig);
adi_eth_Result_e    adin_SetDownspeedRetries     (adinPhy_DeviceHandle_t hDevice, uint16_t downspeedRetiries);

adi_eth_Result_e    adin_ForceSpeed              (adinPhy_DeviceHandle_t hDevice, adi_force_speed_e userChoice);

adi_eth_Result_e    adin_EnableEEE               (adinPhy_DeviceHandle_t hDevice, adi_phy_eee_en_e enableEEE);
adi_eth_Result_e    adin_GetEEE                  (adinPhy_DeviceHandle_t hDevice, adi_phy_eee_en_e *status);

adi_eth_Result_e    adin_SelectMACInterface      (adinPhy_DeviceHandle_t hDevice, adi_MAC_Interface_e MACInterface);
adi_eth_Result_e    adin_EnableRGMIITxRx         (adinPhy_DeviceHandle_t hDevice, adi_MAC_RGMII_config_e EnableRGMIITxRx);
adi_eth_Result_e    adin_ResetRMIIFIFO           (adinPhy_DeviceHandle_t hDevice);

adi_eth_Result_e    adin_GetMasterSlaveStatus    (adinPhy_DeviceHandle_t hDevice, adi_Master_Slave_Status_e *status);
adi_eth_Result_e    adin_MasterSlaveConfig       (adinPhy_DeviceHandle_t hDevice, adi_phy_AdvMasterSlaveCfg_e masterSlaveConfig);

adi_eth_Result_e    adin_GeClk25En               (adinPhy_DeviceHandle_t hDevice);
adi_eth_Result_e    adin_GeClkHrtFreeEn          (adinPhy_DeviceHandle_t hDevice);
adi_eth_Result_e    adin_GeClkRCVREn             (adinPhy_DeviceHandle_t hDevice);
adi_eth_Result_e    adin_GeClkRefEn              (adinPhy_DeviceHandle_t hDevice);
adi_eth_Result_e    adin_GeClkFree125En          (adinPhy_DeviceHandle_t hDevice);
adi_eth_Result_e    adin_GeClkRCVR125En          (adinPhy_DeviceHandle_t hDevice);
adi_eth_Result_e    adin_GeClkDisable            (adinPhy_DeviceHandle_t hDevice);

adi_eth_Result_e    adin_Reset                   (adinPhy_DeviceHandle_t hDevice, adin1200_phy_ResetType_e resetType);

adi_eth_Result_e    adin_PhyWrite                (adinPhy_DeviceHandle_t hDevice, uint32_t regAddr, uint16_t regData);
adi_eth_Result_e    adin_PhyRead                 (adinPhy_DeviceHandle_t hDevice, uint32_t regAddr, uint16_t *regData);

adi_eth_Result_e    adin_LedEn                   (adinPhy_DeviceHandle_t hDevice, adi_phy_LedPort_e ledSel, bool enable);

/** @}*/

#ifdef __cplusplus
}
#endif

#endif /* ADINPHY_H */
