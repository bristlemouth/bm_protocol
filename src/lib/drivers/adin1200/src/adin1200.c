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

/** @addtogroup phydriver ADIN PHY Driver
 *  @{
 */

#include "adin1200.h"

/*!
 * @brief           PHY driver initialization.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      cfg         Device driver configuration.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *                  - #ADI_ETH_INVALID_PARAM        Configured memory size too small.
 *                  - #ADI_ETH_COMM_ERROR           MDIO communication failure.
 *                  - #ADI_ETH_UNSUPPORTED_DEVICE   Device not supported.
 *
 * @details         Initializes the driver and the PHY hardware.
 *                  When the function finishes execution, the device is in software powerdown state.
 *
 * @sa              adin_UnInit()
 * @sa              adin_ReInitPhy()
 */
adi_eth_Result_e adin_Init(adinPhy_DeviceHandle_t hDevice, adi_phy_DriverConfig_t *cfg)
{
    return adin1200PhyDriverEntry.Init(&hDevice->pPhyDevice, cfg, hDevice, HAL_ADIN1200_PhyRead, HAL_ADIN1200_PhyWrite);
}

/*!
 * @brief           PHY driver uninitialization.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS          Call completed successfully.
 *                  - #ADI_ETH_INVALID_HANDLE   Invalid device handle.
 *
 * @details         Uninitializes the PHY device.
 *                  Disables the PHY host interrupt and initializes the driver structure.
 *
 * @sa              adin_Init()
 */
adi_eth_Result_e adin_UnInit(adinPhy_DeviceHandle_t hDevice)
{
    return adin1200PhyDriverEntry.UnInit(hDevice->pPhyDevice);
}

/*!
 * @brief           Re-initializes the PHY after a reset event.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details         This function initializes the PHY hardware by performing
 *                  the same steps as adin_Init().
 *
 * @sa              adin_Init()
 */
adi_eth_Result_e adin_ReInitPhy(adinPhy_DeviceHandle_t hDevice)
{
    return adin1200PhyDriverEntry.ReInitPhy(hDevice->pPhyDevice);
}

/*!
 * @brief           Get device identity.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     pDevId      Device identity.
 *
 * @details        Read the PHY device ID
 *
 */
adi_eth_Result_e adin_GetDeviceId(adinPhy_DeviceHandle_t hDevice, adinPhy_DeviceId_t *pDevId)
{
    return adin1200PhyDriverEntry.GetDeviceId(hDevice->pPhyDevice, pDevId);
}

/*!
 * @brief           Register callback for the PHY interrupt.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      cbFunc      Callback function.
 * @param [in]      cbEvents    Callback events.
 *
 * @details         Registers a callback for the PHY interrupt. If any of the events
 *                  defined in cbEvents is detected, the application is notified by
 *                  calling the callback function cbFunc.
 *
 * @sa              adin_ReadIrqStatus()
 */
adi_eth_Result_e adin_RegisterCallback(adinPhy_DeviceHandle_t hDevice, adi_eth_Callback_t cbFunc, uint32_t cbEvents)
{
    return adin1200PhyDriverEntry.RegisterCallback(hDevice->pPhyDevice, cbFunc, cbEvents, (void *)hDevice);
}

/*!
 * @brief           Read interrupt status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      Interrupt status.
 *
 *
 * @sa              adin_RegisterCallback()
 */
adi_eth_Result_e adin_ReadIrqStatus(adinPhy_DeviceHandle_t hDevice, uint32_t *status)
{
    return adin1200PhyDriverEntry.ReadIrqStatus(hDevice->pPhyDevice, status);
}

/*!
 * @brief           SoftwarePowerDown mode Entry/Exit
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      swpd        Software Powerdown enable or disable.
 *
 * @details         Enables or Disables Software powerdown mode
 *
 * @sa              adin_GetSoftwarePowerdown()
 */
adi_eth_Result_e adin_SoftwarePowerdown(adinPhy_DeviceHandle_t hDevice, adi_phy_swpd_e swpd)
{
    return adin1200PhyDriverEntry.SoftwarePowerdown(hDevice->pPhyDevice,swpd);
}

/*!
 * @brief           Determine if the PHY is in software powerdown or not.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     flag        Indicate if the PHY is in software powerdown or not.
 *
 * @details         Gets SWPD status
 *
 * @sa              adin_EnterSoftwarePowerdown()
 */
adi_eth_Result_e adin_GetSoftwarePowerdown(adinPhy_DeviceHandle_t hDevice, bool *flag)
{
    return adin1200PhyDriverEntry.GetSoftwarePowerdown(hDevice->pPhyDevice, flag);
}

/*!
 * @brief           Read link status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      Link status.
 *
 * @details         This function reads the link status if Up/Down
 *
 */
adi_eth_Result_e adin_GetLinkStatus(adinPhy_DeviceHandle_t hDevice, adi_phy_LinkStatus_e *status)
{
    return adin1200PhyDriverEntry.GetLinkStatus(hDevice->pPhyDevice, status);
}

/*!
 * @brief           Configures the Link
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      enable      Link enable or disable.
 *
 * @details         Enables or Disables the Link
 *
 * @sa              adin_GetLinkStatus()
 */
adi_eth_Result_e adin_LinkConfig(adinPhy_DeviceHandle_t hDevice, bool enable)
{
    return adin1200PhyDriverEntry.LinkConfig(hDevice->pPhyDevice, enable);
}

/*!
 * @brief           Restart auto-negotiation.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details         Restart auto-negotiation.
 *
 */
adi_eth_Result_e adin_AnegRestart(adinPhy_DeviceHandle_t hDevice)
{
    return adin1200PhyDriverEntry.AnegRestart(hDevice->pPhyDevice);
}

/*!
 * @brief           Enable/disable auto-negotiation.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      enable      Enable/disable auto-negotiation.
 *
 * @details         It is STRONGLY recommended to never disable auto-negotiation in the ADINPHY device!
 *
 */
adi_eth_Result_e adin_AnegEnable(adinPhy_DeviceHandle_t hDevice, adi_phy_AnegEn_e enable)
{
    return adin1200PhyDriverEntry.AnegEnable(hDevice->pPhyDevice, enable);
}

/*!
 * @brief           Get auto-negotiation status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      Auto-negotiation status.
 *
 * @details         This function returns the auto-negotiation status.
 *
 */
adi_eth_Result_e adin_GetAnStatus(adinPhy_DeviceHandle_t hDevice, adi_phy_AnegStatus_e *status)
{
    return adin1200PhyDriverEntry.GetAnStatus(hDevice->pPhyDevice, status);
}

/*!
 * @brief           Config Aneg Advertise register
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      anegAdver   Sets Advertise speeds.
 *
 * @details         This function sets the bits to advertise
 *                  -  1000BASE-TX full duplex (only for ADINPHY 1G)
 *                  -  1000BASE-TX half duplex (only for ADINPHY 1G)
 *                  -  100BASE-TX full duplex.
 *                  -  100BASE-TX half duplex.
 *                  -  10BASE-TX full duplex.
 *                  -  10BASE-TX half duplex.
 *
 */
adi_eth_Result_e adin_AnegAdvertiseConfig(adinPhy_DeviceHandle_t hDevice, adi_phy_AnegAdv_e anegAdver)
{
    return adin1200PhyDriverEntry.AnegAdverConfig(hDevice->pPhyDevice, anegAdver);
}

/*!
 * @brief           Aneg Advertise
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     physpeed    Advertised PHY Speeds.
 * @param [out]     phy_speed1G  Set advertise speed for 1G
 *
 * @details         This function return the advertised PHY speeds
 *
 */
adi_eth_Result_e adin_AnegAdvertised(adinPhy_DeviceHandle_t hDevice, uint16_t *phy_speed, uint16_t *phy_speed1G)
{
    return adin1200PhyDriverEntry.AnegAdvertised(hDevice->pPhyDevice, phy_speed, phy_speed1G);
}

/*!
 * @brief           Auto MDIX Configuration
 *
 * @param [in]      hDevice     Device driver handle
 * @param [in]      autoMdixConfig MDIX Config type
 *
 * @details         Configure MDIX to one of the following
 *                  - Manual MDI.
 *                  - Manual MDIX.
 *                  - Auto MDIX Prefer MDIX.
 *                  - Auto MDIX Prefer MDI.
 *
 */
adi_eth_Result_e adin_AutoMDIXConfig(adinPhy_DeviceHandle_t hDevice, adi_phy_AutoMDIXConfig_e autoMdixConfig)
{
    return adin1200PhyDriverEntry.AutoMDIXConfig(hDevice->pPhyDevice,autoMdixConfig );
}

/*!
 * @brief           Config Energy Detect
 *
 * @param [in]      hDevice   Device driver handle
 * @param [in]      EnergyDetectPWD    Energy Detect PWD
 *
 * @details         Config Energy Detect.
 *
 */
adi_eth_Result_e adin_ConfigEnergyDetectPWD(adinPhy_DeviceHandle_t hDevice, adi_Energy_Detect_PWD_e EnergyDetectPWD)
{
    return adin1200PhyDriverEntry.ConfigEnergyDetectPWD(hDevice->pPhyDevice,EnergyDetectPWD);
}

/*!
 * @brief           Get Energy Detect PowerDown status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      Energy Detect Power down status.
 *
 * @details        This status bit indicates that the PHY is in energy detect power-down mode.
 *                 - 1: PHY is in energy detect power-down mode.
 *                 - 0: PHY is not in energy detect power-down mode.
 *
 */
adi_eth_Result_e adin_GetEDetectPWDStatus(adinPhy_DeviceHandle_t hDevice,adi_Energy_Detect_PWD_Status_e *status)
{
    return adin1200PhyDriverEntry.GetEDetectPWDStatus(hDevice->pPhyDevice, status);
}

/*!
 * @brief           Config Downspeed.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     downspeedConfig    Enable or Disable Downspeed.
 *
 * @details        Configure Downspeed
 *
 */
adi_eth_Result_e adin_ConfigDownspeed(adinPhy_DeviceHandle_t hDevice, adi_phy_downspeed_e downspeedConfig)
{
    return adin1200PhyDriverEntry.ConfigDownspeed(hDevice->pPhyDevice, downspeedConfig);
}

/*!
 * @brief           Set Downspeed retries.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     downspeedRetiries     No of Downspeed Retries
 *
 * @details        If downspeed is enabled, this register bit specifies the
 *                 number of retries the PHY must attempt to bring up a link at
 *                 the advertised speed before advertising a lower speed
 *
 */
adi_eth_Result_e adin_SetDownspeedRetries(adinPhy_DeviceHandle_t hDevice, uint16_t downspeedRetiries)
{
    return adin1200PhyDriverEntry.SetDownspeedRetries(hDevice->pPhyDevice, downspeedRetiries);
}

/*!
 * @brief           Force speed.
 *
 * @param [in]      hDevice      Device driver handle.
 * @param [in]      forceSpeed   Force Speed values.
 *
 * @details         Set the speed to one of the following
 *                  - 10BASE_T HD.
 *                  - 10BASE_T FD.
 *                  - 100BASE_TX HD.
 *                  - 100BASE_TX FD.
 *
 */
adi_eth_Result_e adin_ForceSpeed(adinPhy_DeviceHandle_t hDevice, adi_force_speed_e forceSpeed)
{
    return adin1200PhyDriverEntry.ForceSpeed(hDevice->pPhyDevice, forceSpeed);
}

/*!
 * @brief           Energy Efficient Ethernet.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]     enableEEE   Enable or Disable EEE.
 *
 * @details        This status bit indicates that the PHY is in energy detect power-down mode.
 *                 - 1: advertise that the 100BASE-TX/1000BASE-T has EEE capability.
 *                 - 0: do not advertise that the 100BASE-TX/1000BASE-T has EEE capability.
 *
 */
adi_eth_Result_e adin_EnableEEE(adinPhy_DeviceHandle_t hDevice, adi_phy_eee_en_e enableEEE)
{
    return adin1200PhyDriverEntry.EnableEEE(hDevice->pPhyDevice, enableEEE);
}

/*!
 * @brief           Get Energy Efficient Ethernet Status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      EEE status.
 *
 * @details        This status bit indicates that the PHY is in energy detect power-down mode.
 *                 - 1: advertise that the 100BASE-TX/1000BASE-T has EEE capability.
 *                 - 0: do not advertise that the 100BASE-TX/1000BASE-T has EEE capability.
 *
 */
adi_eth_Result_e adin_GetEEE(adinPhy_DeviceHandle_t hDevice, adi_phy_eee_en_e *status)
{
    return adin1200PhyDriverEntry.GetEEE(hDevice->pPhyDevice, status);
}

/*!
 * @brief           Select MAC interface
 *
 * @param [in]      hDevice   Device driver handle
 * @param [in]      MACInterface   MAC interface type
 * @details         Select MAC Interface.
 *
 */
adi_eth_Result_e adin_SelectMACInterface(adinPhy_DeviceHandle_t hDevice, adi_MAC_Interface_e MACInterface)
{
    return adin1200PhyDriverEntry.SelMACInterface(hDevice->pPhyDevice,MACInterface);
}

/*!
 * @brief           MAC RGMII TX and RX enable.
 *
 * @param [in]      hDevice   Device driver handle
 * @param [in]      EnableRGMIITxRx   Enable/Disable RGMIITxRx
 *
 * @details        Enable/disable receive clock internal 2 ns delay in RGMII mode
 *
 */
adi_eth_Result_e adin_EnableRGMIITxRx(adinPhy_DeviceHandle_t hDevice, adi_MAC_RGMII_config_e EnableRGMIITxRx)
{
    return adin1200PhyDriverEntry.EnableRGMIITxRx(hDevice->pPhyDevice, EnableRGMIITxRx);
}

/*!
 * @brief           MAC RMII FIFO Reset.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details        Reset the RMII FIFO
 *
 */
adi_eth_Result_e adin_ResetRMIIFIFO(adinPhy_DeviceHandle_t hDevice)
{
    return adin1200PhyDriverEntry.ResetRMIIFIFO(hDevice->pPhyDevice);
}

/*!
 * @brief           Get Master Slave Status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      Master Slave status.
 *
 * @details         Get master slave Status
 *
 * @sa
 */
adi_eth_Result_e adin_GetMasterSlaveStatus(adinPhy_DeviceHandle_t hDevice, adi_Master_Slave_Status_e *status)
{
    return adin1200PhyDriverEntry.GetMasterSlaveStatus(hDevice->pPhyDevice, status);
}

/*!
 * @brief           Master Slave Config.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     masterSlaveConfig      Master Slave config.
 *
 * @details         Configure PHY as Master or Slave only for ADINPHY 1G
 *
 * @sa
 */
adi_eth_Result_e adin_MasterSlaveConfig(adinPhy_DeviceHandle_t hDevice, adi_phy_AdvMasterSlaveCfg_e masterSlaveConfig)
{
    return adin1200PhyDriverEntry.MasterSlaveConfig(hDevice->pPhyDevice, masterSlaveConfig);
}

/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice  Device driver handle.
 *
 * @details        the 25 MHz reference clock from the crystal oscillator is driven at the GP_CLK pin
 *
 */
adi_eth_Result_e adin_GeClk25En(adinPhy_DeviceHandle_t hDevice)
{
    return adin1200PhyDriverEntry.GeClk25En(hDevice->pPhyDevice);
}

/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details        The PHY provides a digital free running heartbeat clock.
 *
 */
adi_eth_Result_e adin_GeClkHrtFreeEn(adinPhy_DeviceHandle_t hDevice)
{
    return adin1200PhyDriverEntry.GeClkHrtFreeEn(hDevice->pPhyDevice);
}

/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details        The PHY provides a digital recovered heartbeat clock.
 *
 */
adi_eth_Result_e adin_GeClkRCVREn(adinPhy_DeviceHandle_t hDevice)
{
    return adin1200PhyDriverEntry.GeClkRCVREn(hDevice->pPhyDevice);
}

/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details        The PHY provides a digital reference clock. Only for ADINPHY 1G
 *
 */
adi_eth_Result_e adin_GeClkRefEn(adinPhy_DeviceHandle_t hDevice)
{
    return adin1200PhyDriverEntry.GeClkRefEn(hDevice->pPhyDevice);
}

/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details        the 125 MHz PHY free running clock is driven at the GP_CLK pin.
 *
 */
adi_eth_Result_e adin_GeClkFree125En(adinPhy_DeviceHandle_t hDevice)
{
    return adin1200PhyDriverEntry.GeClkFree125En(hDevice->pPhyDevice);
}

/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details        The 125 MHz PHY recovered clock (or PLL clock) is driven at the GP_CLK pin.
 *
 */
adi_eth_Result_e adin_GeClkRCVR125En(adinPhy_DeviceHandle_t hDevice)
{
    return adin1200PhyDriverEntry.GeClkRCVR125En(hDevice->pPhyDevice);
}

/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details        Disable GP_CLK.
 *
 */
adi_eth_Result_e adin_GeClkDisable(adinPhy_DeviceHandle_t hDevice)
{
    return adin1200PhyDriverEntry.GeClkDisable(hDevice->pPhyDevice);
}

/*!
 * @brief           Reset the PHY device.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      resetType   Reset type.
 *
 * @details        3 Types of Reset
 *                 - Subsystem SW Reset with pin configuration
 *                 - Subsystem SW Reset
 *                 - PHY Core SW Reset
 *
 */
adi_eth_Result_e adin_Reset(adinPhy_DeviceHandle_t hDevice, adin1200_phy_ResetType_e resetType)
{
    return adin1200PhyDriverEntry.Reset(hDevice->pPhyDevice, resetType);
}

/*!
 * @brief           Write PHY register.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      regAddr     Register address in the header file format (DEVTYPE+ADDR).
 * @param [in]      regData     Value written to register.
 *
 * @details
 *
 * @sa              adin_PhyRead()
 */
adi_eth_Result_e adin_PhyWrite(adinPhy_DeviceHandle_t hDevice, uint32_t regAddr, uint16_t regData)
{
    return adin1200PhyDriverEntry.PHY_Write(hDevice->pPhyDevice, regAddr, regData);
}

/*!
 * @brief           Read PHY register.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      regAddr     Register address in the header file format (DEVTYPE+ADDR).
 * @param [out]     regData     Value read from register.
 *
 * @details
 *
 * @sa              adin_PhyWrite()
 */
adi_eth_Result_e adin_PhyRead(adinPhy_DeviceHandle_t hDevice, uint32_t regAddr, uint16_t *regData)
{
    return adin1200PhyDriverEntry.PHY_Read(hDevice->pPhyDevice, regAddr, regData);
}

/*!
 * @brief           Enable or disable the LED.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      ledSel      Selection of the LED port.
 * @param [in]      enable      Enable/disable the LED.
 *
 * @details         The LED is enabled by default during driver initialization.
 *
 * @sa
 */
adi_eth_Result_e adin_LedEn(adinPhy_DeviceHandle_t hDevice, adi_phy_LedPort_e ledSel, bool enable)
{
    return adin1200PhyDriverEntry.LedEn(hDevice->pPhyDevice, ledSel, enable);
}


/** @}*/
