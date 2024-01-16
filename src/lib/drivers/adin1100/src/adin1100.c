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

/** @addtogroup adin1100 ADIN1100 PHY Driver
 *  @{
 */

#include "adin1100.h"


/*!
 * @brief           ADIN1100 driver initialization.
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
 *
 *                  The configuration parameter #cfg include the PHY device address
 *                  and a pointer to a memory chunk to be used for the driver structure.
 *
 *                  The following steps are executed:
 *                  - the driver structure is initialized and host interrupt disabled
 *                  - wait for MDIO interface availability by polling register over
 *                    the MDIO interface until 2nd TA bit in MDIO frame is read as 0
 *                  - check device identity by comparing MMD1_DEV_ID1 and MMD1_DEV_ID2
 *                    contents expected value
 *                  - initiate software powerdown and wait until the device is in software
 *                    software powerdown. This step includes the powerup sequencing of PHY
 *                    hardware
 *                  - if PHY interrupt is enabled, program IRQ mask registers such that
 *                    only hardware reset event is enabled
 *                  - static register configuration updates
 *
 *                  When the function finishes execution, the device is in software powerdown state.
 *
 * @sa              adin1100_UnInit(), adin1100_ReInitPhy()
 */
adi_eth_Result_e adin1100_Init(adin1100_DeviceHandle_t hDevice, adi_phy_DriverConfig_t *cfg)
{

    return adin1100PhyDriverEntry.Init(&hDevice->pPhyDevice, cfg, hDevice, HAL_ADIN1100_PhyRead, HAL_ADIN1100_PhyWrite);

}

/*!
 * @brief           ADIN1100 driver uninitialization.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS          Call completed successfully.
 *                  - #ADI_ETH_INVALID_HANDLE   Invalid device handle.
 *
 * @details         Uninitializes the ADIN1100 PHY device.
 *                  Disables the PHY host interrupt and initializes the driver structure.
 *
 * @sa              adin1100_Init()
 */
adi_eth_Result_e adin1100_UnInit(adin1100_DeviceHandle_t hDevice)
{

    return adin1100PhyDriverEntry.UnInit(hDevice->pPhyDevice);
}

/*!
 * @brief           Re-initializes the PHY after a reset event.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details         This function initializes the PHY hardware by performing
 *                  the same steps as adin1100_Init(). It does not change the
 *                  current setup on the host side, such as driver software settings.
 *
 *                  When the function finishes execution, the device is in software powerdown state.
 *
 * @sa              adin1100_Init()
 */
adi_eth_Result_e adin1100_ReInitPhy(adin1100_DeviceHandle_t hDevice)
{

    return adin1100PhyDriverEntry.ReInitPhy(hDevice->pPhyDevice);
}

/*!
 * @brief           Get device identity.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     pDevId      Device identity.
 *
 * @details
 *
 */
adi_eth_Result_e adin1100_GetDeviceId(adin1100_DeviceHandle_t hDevice, adin1100_DeviceId_t *pDevId)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;

    result = adin1100_PhyRead(hDevice, ADDR_MMD1_DEV_ID1, &val16);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    pDevId->phyId = (val16 << 16);

    result = adin1100_PhyRead(hDevice, ADDR_MMD1_DEV_ID2, &val16);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    pDevId->phyId |= val16;

#if !defined(ADIN_S1)
    result = adin1100_PhyRead(hDevice, ADDR_MGMT_PRT_PKG, &val16);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    pDevId->pkgType = val16;
#else
    /* Not relevant in S1, assign a default value */
    pDevId->pkgType = 0;
#endif

    result = adin1100_PhyRead(hDevice, 0x1E900E, &val16);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    pDevId->digRevNum = val16 & 0xFF;

end:
    return result;
}

/*!
 * @brief           Restart auto-negotiation.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details         Restart auto-negotiation.
 *
 */
adi_eth_Result_e adin1100_Renegotiate(adin1100_DeviceHandle_t hDevice)
{
    return adin1100PhyDriverEntry.Renegotiate(hDevice->pPhyDevice);
}

/*!
 * @brief           Reset the PHY device.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      resetType   Reset type.
 *
 * @details         The only reset option currently supported is #ADI_PHY_RESET_TYPE_SW
 *                  (software reset). A full reset of the device, almost equivalent to a
 *                  hardware reset, will be performed.
 *
 */
adi_eth_Result_e adin1100_Reset(adin1100_DeviceHandle_t hDevice, adi_phy_ResetType_e resetType)
{
    return adin1100PhyDriverEntry.Reset(hDevice->pPhyDevice, resetType);
}

/*!
 * @brief           Set loopback mode.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      loopbackMode    Loopback mode.
 *
 * @details         The ADIN1100 features a number of loopback modes that can
 *                  be configured with this function:
 *
 *                  - #ADI_PHY_LOOPBACK_NONE
 *                  - #ADI_PHY_LOOPBACK_PCS
 *                  - #ADI_PHY_LOOPBACK_PMA
 *                  - #ADI_PHY_LOOPBACK_MACIF
 *                  - #ADI_PHY_LOOPBACK_MACIF_SUPPRESS_TX
 *                  - #ADI_PHY_LOOPBACK_MACIF_REMOTE
 *                  - #ADI_PHY_LOOPBACK_MACIF_REMOTE_SUPPRESS_RX
 *
 *                  To return to normal operation, set loopback mode to #ADI_PHY_LOOPBACK_NONE.
 */
adi_eth_Result_e adin1100_SetLoopbackMode(adin1100_DeviceHandle_t hDevice, adi_phy_LoopbackMode_e loopbackMode)
{
    return adin1100PhyDriverEntry.SetLoopbackMode(hDevice->pPhyDevice, loopbackMode);
}

/*!
 * @brief           Set test mode.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      testMode        Test mode.
 *
 * @details         The ADIN1100 features a number of test modes that can
 *                  be configured with this function:
 *
 *                  - #ADI_PHY_TEST_MODE_NONE
 *                  - #ADI_PHY_TEST_MODE_1
 *                  - #ADI_PHY_TEST_MODE_2
 *                  - #ADI_PHY_TEST_MODE_3
 *                  - #ADI_PHY_TEST_MODE_TX_DISABLE
 *
 *                  To return to normal operation, set loopback mode to #ADI_PHY_TEST_MODE_NONE.
 *
 */
adi_eth_Result_e adin1100_SetTestMode(adin1100_DeviceHandle_t hDevice, adi_phy_TestMode_e testMode)
{
    return adin1100PhyDriverEntry.SetTestMode(hDevice->pPhyDevice, testMode);
}

/*!
 * @brief           Configure the advertised transmit operating mode for auto-negotiation.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      txMode      Transmit operating mode.
 *
 * @details         This function configures the advertised transmit operating mode (signal amplitude).
 *                  The options are:
 *                  - #ADI_PHY_AN_ADV_TX_REQ_1P0V
 *                  - #ADI_PHY_AN_ADV_TX_REQ_2P4V
 *                  - #ADI_PHY_AN_ADV_TX_REQ_1P0V_ABLE_2P4V
 *
 *                  Note that the default transmit operating mode is set by the pin strap option TX2P4_ENB.
 *                  This API can be used to overwrite the value set by the pin strap.
 *
 */
adi_eth_Result_e adin1100_AnAdvTxMode(adin1100_DeviceHandle_t hDevice, adi_phy_AnAdvTxMode_e txMode)
{
    return adin1100PhyDriverEntry.AnAdvTxMode(hDevice->pPhyDevice, txMode);
}

/*!
 * @brief           Configure the advertised master/slave configuration for auto-negotiation.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      msCfg       Master/slave configuration.
 *
 * @details         This function configures the advertised master/slave configuration.
 *                  The options are:
 *                  - #ADI_PHY_AN_ADV_FORCED_MASTER
 *                  - #ADI_PHY_AN_ADV_FORCED_SLAVE
 *                  - #ADI_PHY_AN_ADV_PREFFERED_MASTER
 *                  - #ADI_PHY_AN_ADV_PREFFERED_SLAVE
 *
 *                  Note that the default master/slave configuraton is set by the pin strap option MS_SEL.
 *                  This API can be used to overwrite the value set by the pin strap.
 *
 */
adi_eth_Result_e adin1100_AnAdvMstSlvCfg(adin1100_DeviceHandle_t hDevice, adi_phy_AnAdvMasterSlaveCfg_e msCfg)
{
    return adin1100PhyDriverEntry.AnAdvMstSlvCfg(hDevice->pPhyDevice, msCfg);
}


/*!
 * @brief           Enable/disable auto-negotiation.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      enable      Enable/disable auto-negotiation.
 *
 * @details         It is STRONGLY recommended to never disable auto-negotation in the ADIN1100 device!
 *
 */
adi_eth_Result_e adin1100_AnEnable(adin1100_DeviceHandle_t hDevice, bool enable)
{
    return adin1100PhyDriverEntry.AnEnable(hDevice->pPhyDevice, enable);
}

/*!
 * @brief           Get auto-negotiation status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      Auto-negotiation status.
 *
 * @details         This function return the following auto-negotation results:
 *                  - auto-negotiation complete
 *                  - link status
 *                  - master-slave resolution
 *                  - resolved transmit operation mode
 *
 */
adi_eth_Result_e adin1100_GetAnStatus(adin1100_DeviceHandle_t hDevice, adi_phy_AnStatus_t *status)
{
    return adin1100PhyDriverEntry.GetAnStatus(hDevice->pPhyDevice, status);
}

#include <stdio.h>
adi_eth_Result_e adin1100_EnterMediaConverterMode(adin1100_DeviceHandle_t hDevice )
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;

    // Read the current config register
    result = adin1100_PhyRead(hDevice, ADDR_CRSM_MAC_IF_CFG, &val16);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    printf( "mac reg: %d\n", (int)val16 );

    if ((val16 & BITM_CRSM_MAC_IF_CFG_CRSM_RMII_MEDIA_CNV_EN) != 0) {
        // The MEDIA_CNV_EN bit is set
        printf( "don't set\n" );
    } else {
        // The MEDIA_CNV_EN bit is not set
        val16 |= BITM_CRSM_MAC_IF_CFG_CRSM_RMII_MEDIA_CNV_EN;
        printf( "do set\n" );
    }
    result = adin1100_PhyWrite(hDevice, ADDR_CRSM_MAC_IF_CFG, val16);
    // result = ADI_ETH_SUCCESS;

end:
    return result;
    // return adin1100PhyDriverEntry.EnterMediaConverterMode(hDevice->pPhyDevice);
}


/*!
 * @brief           Enter software powerdown.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details
 *
 * @sa              adin1100_ExitSoftwarePowerdown()
 */
adi_eth_Result_e adin1100_EnterSoftwarePowerdown(adin1100_DeviceHandle_t hDevice)
{
    return adin1100PhyDriverEntry.EnterSoftwarePowerdown(hDevice->pPhyDevice);
}

/*!
 * @brief           Exit software powerdown.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details
 *
 * @sa              adin1100_EnterSoftwarePowerdown()
 */
adi_eth_Result_e adin1100_ExitSoftwarePowerdown(adin1100_DeviceHandle_t hDevice)
{
    return adin1100PhyDriverEntry.ExitSoftwarePowerdown(hDevice->pPhyDevice);
}

/*!
 * @brief           Determine if the ADIN1100 is in software powerdown or not.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     flag        Indicate if the ADIN1100 is in software powerdown or not.
 *
 * @details
 *
 * @sa              adin1100_EnterSoftwarePowerdown(), adin1100_EnterSoftwarePowerdown()
 */
adi_eth_Result_e adin1100_GetSoftwarePowerdown(adin1100_DeviceHandle_t hDevice, bool *flag)
{
    return adin1100PhyDriverEntry.GetSoftwarePowerdown(hDevice->pPhyDevice, flag);
}

/*!
 * @brief           Register callback for the PHY interrupt.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      cbFunc      Callback function.
 * @param [in]      cbEvents    Callback events.
 *
 * @details         Registers a callback forthe PHY interrupt. If any of the events
 *                  defined in cbEvents is detected, the application is notified by
 *                  calling the callback function cbFunc.
 *
 *                  The callback function can be cleared by register a
 *
 * @sa
 */
adi_eth_Result_e adin1100_RegisterCallback(adin1100_DeviceHandle_t hDevice, adi_eth_Callback_t cbFunc, uint32_t cbEvents)
{
    return adin1100PhyDriverEntry.RegisterCallback(hDevice->pPhyDevice, cbFunc, cbEvents, (void *)hDevice);
}

/*!
 * @brief           Read interrupt status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      Interrupt status.
 *
 * @details         The interrupt status returned by the function is based on interrupt
 *                  sources from both registers CRSM_IRQ_STATUS and PHY_SUBSYS_IRQ_STATUS.
 *
 *                  The status is made of the event bits as defined in #adi_phy_InterruptEvt_e.
 *                  More than one event can be active simultaneously.
 *
 * @sa              adin1100_UnInit()
 */
adi_eth_Result_e adin1100_ReadIrqStatus(adin1100_DeviceHandle_t hDevice, uint32_t *status)
{
    return adin1100PhyDriverEntry.ReadIrqStatus(hDevice->pPhyDevice, status);
}

/*!
 * @brief           Read link status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      Link status.
 *
 * @details
 *
 */
adi_eth_Result_e adin1100_GetLinkStatus(adin1100_DeviceHandle_t hDevice, adi_phy_LinkStatus_e *status)
{
    return adin1100PhyDriverEntry.GetLinkStatus(hDevice->pPhyDevice, status);
}

#if defined(ADIN_S1)
/*!
 * @brief           Enable or disable the LED.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      enable      Enable/disable the LED.
 *
 * @details         The LED is enabled by default during driver initialization.
 *
 * @sa
 */
adi_eth_Result_e adin1100_LedEn(adin1100_DeviceHandle_t hDevice, bool enable)
{
    return adin1100PhyDriverEntry.LedEn(hDevice->pPhyDevice, enable);
}

/*!
 * @brief           Configure LED blink time.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      blinkOn     LEN blink ON time.
 * @param [in]      blinkOff    LEN blink OFF time.
 *
 * @details
 *
 * @sa
 */
adi_eth_Result_e adin1100_LedBlinkTime(adin1100_DeviceHandle_t hDevice, uint32_t blinkOn, uint32_t blinkOff)
{
    return adin1100PhyDriverEntry.LedBlinkTime(hDevice->pPhyDevice, blinkOn, blinkOff);
}
#else
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
adi_eth_Result_e adin1100_LedEn(adin1100_DeviceHandle_t hDevice, adi_phy_LedPort_e ledSel, bool enable)
{
    return adin1100PhyDriverEntry.LedEn(hDevice->pPhyDevice, ledSel, enable);
}

/*!
 * @brief           Configure LED blink time.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      ledSel      Enable/disable the LED.
 * @param [in]      blinkOn     LEN blink ON time.
 * @param [in]      blinkOff    LEN blink OFF time.
 *
 * @details
 *
 * @sa
 */
adi_eth_Result_e adin1100_LedBlinkTime(adin1100_DeviceHandle_t hDevice, adi_phy_LedPort_e ledSel, uint32_t blinkOn, uint32_t blinkOff)
{
    return adin1100PhyDriverEntry.LedBlinkTime(hDevice->pPhyDevice, ledSel, blinkOn, blinkOff);
}

#endif
/*!
 * @brief           Get PHY device capabilities.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [out]     capabilities    Device capabilities.
 *
 * @details        Reports if the device support 10BASE-T1L 2.4V transmit level
 *                 and 10BASE-T1L PMA local loopback.
 *
 */
adi_eth_Result_e adin1100_GetCapabilities(adin1100_DeviceHandle_t hDevice, uint16_t *capabilities)
{
    return adin1100PhyDriverEntry.GetCapabilities(hDevice->pPhyDevice, capabilities);
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
 * @sa              adin1100_PhyRead()
 */
adi_eth_Result_e adin1100_PhyWrite(adin1100_DeviceHandle_t hDevice, uint32_t regAddr, uint16_t regData)
{
    return adin1100PhyDriverEntry.PHY_Write(hDevice->pPhyDevice, regAddr, regData);
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
 * @sa              adin1100_PhyWrite()
 */
adi_eth_Result_e adin1100_PhyRead(adin1100_DeviceHandle_t hDevice, uint32_t regAddr, uint16_t *regData)
{
    return adin1100PhyDriverEntry.PHY_Read(hDevice->pPhyDevice, regAddr, regData);
}

/*!
 * @brief           Get link quality measure based on the Mean Square Error (MSE) value.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     linkQuality Link quality struct.
 *
 * @details         This feature does not exist in S1 and calling this function return #ADI_ETH_NOT_IMPLEMENTED.
 *
 */
adi_eth_Result_e adin1100_GetMseLinkQuality(adin1100_DeviceHandle_t hDevice, adi_phy_MseLinkQuality_t *mseLinkQuality)
{
    return adin1100PhyDriverEntry.GetMseLinkQuality(hDevice->pPhyDevice, mseLinkQuality);
}

/*!
 * @brief           Enable/disable frame generator.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      enable      Enable flag.
 *
 * @details         Enables/disables the frame generator based on the flag value.
 *
 */
adi_eth_Result_e adin1100_FrameGenEn(adin1100_DeviceHandle_t hDevice, bool enable)
{
    return adin1100PhyDriverEntry.FrameGenEn(hDevice->pPhyDevice, enable);
}

/*!
 * @brief           Set frame generator mode.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      mode        Frame generator mode.
 *
 * @details         Sets one of the following frame generator modes:
 *                  - #ADI_PHY_FRAME_GEN_MODE_BURST: burst mode
 *                  - #ADI_PHY_FRAME_GEN_MODE_CONT: continuous mode
 *
 */
adi_eth_Result_e adin1100_FrameGenSetMode(adin1100_DeviceHandle_t hDevice, adi_phy_FrameGenMode_e mode)
{
    return adin1100PhyDriverEntry.FrameGenSetMode(hDevice->pPhyDevice, mode);
}

/*!
 * @brief           Set frame generator frame counter.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      frameCnt    Frame count.
 *
 * @details         Updates the frame counter to the frameCnt value.
 *                  The 32-bit argument is split in 16-bit values in
 *                  order to update the 2 register FG_NFRM_H/FFG_NFRM_L.
 *
 */
adi_eth_Result_e adin1100_FrameGenSetFrameCnt(adin1100_DeviceHandle_t hDevice, uint32_t frameCnt)
{
    return adin1100PhyDriverEntry.FrameGenSetFrameCnt(hDevice->pPhyDevice, frameCnt);
}

/*!
 * @brief           Set frame generator payload.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      payload     Payload type.
 *
 * @details         The valid payloads are:
 *                  - #ADI_PHY_FRAME_GEN_PAYLOAD_NONE
 *                  - #ADI_PHY_FRAME_GEN_PAYLOAD_RANDOM
 *                  - #ADI_PHY_FRAME_GEN_PAYLOAD_0X00
 *                  - #ADI_PHY_FRAME_GEN_PAYLOAD_0XFF
 *                  - #ADI_PHY_FRAME_GEN_PAYLOAD_0x55
 *                  - #ADI_PHY_FRAME_GEN_PAYLOAD_DECR
 *
 */
adi_eth_Result_e adin1100_FrameGenSetFramePayload(adin1100_DeviceHandle_t hDevice, adi_phy_FrameGenPayload_e payload)
{
    return adin1100PhyDriverEntry.FrameGenSetFramePayload(hDevice->pPhyDevice, payload);
}

/*!
 * @brief           Set frame generator frame length.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      frameLen    Frame length.
 *
 * @details         Note the length of frames produced by the generator
 *                  has an additional overhead of 18 bytes: 6 bytes for source address,
 *                  6 bytes for destination address, 2 bytes for the length field and
 *                  4 bytes for the FCS field.
 *
 *                  If the frame length is set to a value less than 46 bytes, which
 *                  results in frames with length less than the minimum Ethernet frame
 *                  (64 bytes), the generator willnot implement any padding.
 */
adi_eth_Result_e adin1100_FrameGenSetFrameLen(adin1100_DeviceHandle_t hDevice, uint16_t frameLen)
{
    return adin1100PhyDriverEntry.FrameGenSetFrameLen(hDevice->pPhyDevice, frameLen);
}

/*!
 * @brief           Set frame generator interframe gap.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      ifgLen      Interframe gap.
 *
 * @details
 *
 */
adi_eth_Result_e adin1100_FrameGenSetIfgLen(adin1100_DeviceHandle_t hDevice, uint16_t ifgLen)
{
    return adin1100PhyDriverEntry.FrameGenSetIfgLen(hDevice->pPhyDevice, ifgLen);
}

/*!
 * @brief           Restart frame generator.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @details         Restarts the frame generator, this is required
 *                  the enable frame generation. Before restart,
 *                  the function will read the FG_DONE bit to clear it,
 *                  in case it's still set from a previous run.
 *
 */
adi_eth_Result_e adin1100_FrameGenRestart(adin1100_DeviceHandle_t hDevice)
{
    return adin1100PhyDriverEntry.FrameGenRestart(hDevice->pPhyDevice);
}

/*!
 * @brief           Read frame generator status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     fgDone      Frame generator status.
 *
 * @details         Read the FG_DONE bit to determine if the frame generation is complete.
 *
 */
adi_eth_Result_e adin1100_FrameGenDone(adin1100_DeviceHandle_t hDevice, bool *fgDone)
{
    return adin1100PhyDriverEntry.FrameGenDone(hDevice->pPhyDevice, fgDone);
}

/*!
 * @brief           Enable/disable frame checker.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      enable      Enable flag.
 *
 * @details         Enables/disables the frame checker based on the flag value.
 *
 */
adi_eth_Result_e adin1100_FrameChkEn(adin1100_DeviceHandle_t hDevice, bool enable)
{
    return adin1100PhyDriverEntry.FrameChkEn(hDevice->pPhyDevice, enable);
}

/*!
 * @brief           Select frame checker source.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      mode        Frame checker source.
 *
 * @details         Sets one of the following frame checker sources:
 *                  - #ADI_PHY_FRAME_CHK_SOURCE_PHY
 *                  - #ADI_PHY_FRAME_CHK_SOURCE_MAC
 *
 */
 adi_eth_Result_e adin1100_FrameChkSourceSelect(adin1100_DeviceHandle_t hDevice, adi_phy_FrameChkSource_e source)
{
    return adin1100PhyDriverEntry.FrameChkSourceSelect(hDevice->pPhyDevice, source);
}

/*!
 * @brief           Read frame checker frame count.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     cnt         Frame count.
 *
 * @details         Note the frame count value is latched when reading RX_CNT_ERR, therefore
 *                  a call to this function should be preceded by a call to adin1100_FrameChkReadRxErrCnt().
 *
 * @sa              adin1100_FrameChkReadRxErrCnt
 */
adi_eth_Result_e adin1100_FrameChkReadFrameCnt(adin1100_DeviceHandle_t hDevice, uint32_t *cnt)
{
    return adin1100PhyDriverEntry.FrameChkReadFrameCnt(hDevice->pPhyDevice, cnt);
}

/*!
 * @brief           Read frame checker receive errors.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     cnt         Error counter.
 *
 * @details         Note the frame counter/error counter values are latched when reading RX_CNT_ERR, therefore
 *                  a call to this function should preceded calls to adin1100_FrameChkReadFrameCnt()
 *                  or adin1100_FrameChkReadErrorCnt().
 *
 * @sa              adin1100_FrameChkReadFrameCnt, adin1100_FrameChkReadErrorCnt
 */
adi_eth_Result_e adin1100_FrameChkReadRxErrCnt(adin1100_DeviceHandle_t hDevice, uint16_t *cnt)
{
    return adin1100PhyDriverEntry.FrameChkReadRxErrCnt(hDevice->pPhyDevice, cnt);
}

/*!
 * @brief           Read frame checker error counters.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     cnt         Error counters.
 *
 * @details         Note the error counter values are latched when reading RX_CNT_ERR, therefore
 *                  a call to this function should be preceded by a call to adin1100_FrameChkReadRxErrCnt().
 *
 * @sa              adin1100_FrameChkReadRxErrCnt
 */
adi_eth_Result_e adin1100_FrameChkReadErrorCnt(adin1100_DeviceHandle_t hDevice, adi_phy_FrameChkErrorCounters_t *cnt)
{
    return adin1100PhyDriverEntry.FrameChkReadErrorCnt(hDevice->pPhyDevice, cnt);
}



/** @}*/
