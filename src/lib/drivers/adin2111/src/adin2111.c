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

#include "adin2111.h"

/** @addtogroup adin2111 ADIN2111 LES Software Driver
 *  @{
 */

/*! @cond PRIVATE */

adin2111_DeviceHandle_t     pDeviceHandle;
static adi_eth_Result_e     configureTsPins                 (adin2111_DeviceHandle_t hDevice);
static adin2111_TxPort_e    dynamicTableLookup              (adin2111_DeviceHandle_t hDevice, uint8_t *macAddress);
static void                 dynamicTableLearn               (adin2111_DeviceHandle_t hDevice, adin2111_TxPort_e port, uint8_t *macAddress);

static uint32_t PhyWrite(uint8_t hwAddr, uint32_t regAddr, uint16_t data)
{
    return (uint32_t)macDriverEntry.PhyWrite(pDeviceHandle->pMacDevice , hwAddr, regAddr, data);
}

static uint32_t PhyRead(uint8_t hwAddr, uint32_t regAddr, uint16_t *data)
{
    return (uint32_t)macDriverEntry.PhyRead(pDeviceHandle->pMacDevice , hwAddr, regAddr, data);
}

/* Callback from frame receive, used to update the dynamic forwarding table. */
static void cbRx(void *pCBParam, uint32_t Event, void *pArg)
{
    (void) Event;
    adin2111_DeviceHandle_t     hDevice;
    adi_eth_BufDesc_t           *pBufDesc;

    hDevice = (adin2111_DeviceHandle_t)pCBParam;
    pBufDesc = (adi_eth_BufDesc_t *)pArg;

    dynamicTableLearn(hDevice, (adin2111_TxPort_e)pBufDesc->port, &pBufDesc->pBuf[6]);
}

/*! @endcond */

/////////////////////////////

/*!
 * @brief           ADIN2111 driver initialization.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      pCfg        Pointer to device configuration structure.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *                  - #ADI_ETH_INVALID_PARAM        Configured memory size too small.
 *                  - #ADI_ETH_UNSUPPORTED_DEVICE   Device not supported.
 *
 * @details         Initializes the driver and the ADIN2111 hardware, includingthe MAC and PHYs corresponding
 *                  to the 2 ports.
 *
 *                  The configuration structure includes a pointer to a memory chunk to be used for the driver structure.
 *
 *                  If the destination address of an incoming frame, received on either port, does not match any
 *                  of the addresses configured in the MAC_ADDR_FILT_* registers, the switch is configured
 *                  to forward such frames to the other port.
 *
 *                  The function initializes the dynamic forwarding table by setting the age of all entries to 0.
 *
 *                  When the function finishes execution, both PHYs are in software powerdown state with
 *                  auto-negotiation enabled. Use adin2111_Enable() to exit software powerdown and establish the link.
 *
 * @sa              adin2111_UnInit()
 */
adi_eth_Result_e adin2111_Init(adin2111_DeviceHandle_t hDevice, adin2111_DriverConfig_t *pCfg)
{
    adi_eth_Result_e        result = ADI_ETH_SUCCESS;
    adi_mac_DriverConfig_t  macDrvConfig;
    adi_phy_DriverConfig_t  phyDrvConfig[ADIN2111_PORT_NUM];
    uint16_t                phyRegData;
    uint32_t                retryCount;
    ADI_MAC_CONFIG2_t       macConfig2;

    if (pCfg->devMemSize < ADIN2111_DEVICE_SIZE)
    {
        result = ADI_ETH_INVALID_PARAM;
        goto end;
    }

    /* Initialize TS pin assignments from configuration structure. */
    hDevice->tsTimerPin = pCfg->tsTimerPin;
    hDevice->tsCaptPin = pCfg->tsCaptPin;

    /* Initialize the MAC configuration structure. */
    macDrvConfig.pDevMem = (void *)pCfg->pDevMem;
    macDrvConfig.devMemSize = ADI_MAC_DEVICE_SIZE;
    macDrvConfig.fcsCheckEn = pCfg->fcsCheckEn;

    /* Initialize the PHY configuration structure. */
    phyDrvConfig[ADIN2111_PORT_1].addr = 1;
    phyDrvConfig[ADIN2111_PORT_1].pDevMem = (void *)((uint8_t *)pCfg->pDevMem + ADI_MAC_DEVICE_SIZE);
    phyDrvConfig[ADIN2111_PORT_1].devMemSize = ADI_PHY_DEVICE_SIZE;
    phyDrvConfig[ADIN2111_PORT_1].enableIrq  = false;

    phyDrvConfig[ADIN2111_PORT_2].addr = 2;
    phyDrvConfig[ADIN2111_PORT_2].pDevMem = (void *)((uint8_t *)pCfg->pDevMem + ADI_MAC_DEVICE_SIZE + ADI_PHY_DEVICE_SIZE);
    phyDrvConfig[ADIN2111_PORT_2].devMemSize = ADI_PHY_DEVICE_SIZE;
    phyDrvConfig[ADIN2111_PORT_2].enableIrq  = false;

    pDeviceHandle = hDevice;

    /* Initialize MAC */
    result = macDriverEntry.Init(&hDevice->pMacDevice, &macDrvConfig, (void *)hDevice);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    /* Dynamic forward table initialization. */
    result = macDriverEntry.RegisterCallback(hDevice->pMacDevice, cbRx, ADI_MAC_EVT_DYN_TBL_UPDATE, (void *)hDevice);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    adin2111_ClearDynamicTable(hDevice);
    hDevice->dynamicTableReplaceLoc = 0;

    hDevice->portEnabled[ADIN2111_PORT_1] = false;
    hDevice->portEnabled[ADIN2111_PORT_2] = false;

    /* Configure forwarding of frames with unknown destination address */
    /* to the other port. This forwarding is done in hardware.         */
    /* Note that this setting will only take effect after the ports    */
    /* have been enabled (PHYs out of software powerdown).             */

    /* Configure the switch with cut through between ports enabled */
    /* and frames with unknown DAs forwarded to the other port.    */
    /* This is done in hardware and will take effect after the     */
    /* ports have been enabled (PHYs out of software powerdown).   */
    /* Note it's a read-modify-write because CONFIG2 maybe have    */
    /* been updated before (e.g. CRC_APPEND during MAC init.       */
    result = adin2111_ReadRegister(hDevice, ADDR_MAC_CONFIG2, &macConfig2.VALUE32);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    // macConfig2.P1_FWD_UNK2P2 = 1;
    // macConfig2.P2_FWD_UNK2P1 = 1;
    macConfig2.P1_FWD_UNK2HOST = 1;
    macConfig2.P2_FWD_UNK2HOST = 1;
    macConfig2.PORT_CUT_THRU_EN = 0;
    result = adin2111_WriteRegister(hDevice, ADDR_MAC_CONFIG2, macConfig2.VALUE32);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    /* Port 2 PHY comes out of reset after Port 1 PHY, wait with PHY initialization */
    /* until both PHYs are out of reset. Note that the value read from Port 2 PHY   */
    /* registers before coming out of reset is all 0s.                              */
    retryCount = 0;
    do
    {
        result = macDriverEntry.PhyRead(hDevice->pMacDevice, phyDrvConfig[ADIN2111_PORT_2].addr, ADDR_CRSM_IRQ_MASK, &phyRegData);
        retryCount++;
    } while (((result != ADI_ETH_SUCCESS) || (!phyRegData)) && (retryCount < ADIN2111_PHY_POWERUP_MAX_RETRIES));

    if (retryCount >= ADIN2111_PHY_POWERUP_MAX_RETRIES)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    /* Initialize port 1 PHY */
    result = phyDriverEntry.Init(&hDevice->pPhyDevice[ADIN2111_PORT_1], &phyDrvConfig[ADIN2111_PORT_1], hDevice, PhyRead, PhyWrite);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    /* Initialize port 2 PHY */
    result = phyDriverEntry.Init(&hDevice->pPhyDevice[ADIN2111_PORT_2], &phyDrvConfig[ADIN2111_PORT_2], hDevice, PhyRead, PhyWrite);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    /* Now enable the PHY interrupt sources in the MAC status registers */
    hDevice->pMacDevice->irqMask0 &= ~BITM_MAC_IMASK0_PHYINTM;
    result = adin2111_WriteRegister(hDevice, ADDR_MAC_IMASK0, hDevice->pMacDevice->irqMask0);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    hDevice->pMacDevice->irqMask1 &= ~BITM_MAC_IMASK1_P2_PHYINT_MASK;
    //hDevice->pMacDevice->irqMask1 &= ~BITM_MAC_IMASK1_P1_RX_RDY_MASK;
    //hDevice->pMacDevice->irqMask1 &= ~BITM_MAC_IMASK1_P2_RX_RDY_MASK;
    result = adin2111_WriteRegister(hDevice, ADDR_MAC_IMASK1, hDevice->pMacDevice->irqMask1);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    /* Default mask for PHY interrupts set by the PHY init function */
    hDevice->pMacDevice->phyIrqMask = ADI_PHY_CRSM_HW_ERROR | BITM_CRSM_IRQ_MASK_CRSM_HRD_RST_IRQ_EN;

    /* Configure pins for TS_TIMER and TS_CAPT capability */
    configureTsPins(hDevice);
end:
    return result;
}

/*!
 * @brief           ADIN2111 driver uninitialization.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
 * @details         Uninitializes the driver and the ADIN2111 hardware.
 *
 * @sa              adin2111_Init()
 */
adi_eth_Result_e adin2111_UnInit(adin2111_DeviceHandle_t hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    result = phyDriverEntry.UnInit(hDevice->pPhyDevice[ADIN2111_PORT_1]);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    result = phyDriverEntry.UnInit(hDevice->pPhyDevice[ADIN2111_PORT_2]);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    result = macDriverEntry.UnInit(hDevice->pMacDevice);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    ADI_HAL_UNINIT(hDevice);

    hDevice->pPhyDevice[ADIN2111_PORT_1] = NULL;
    hDevice->pPhyDevice[ADIN2111_PORT_2] = NULL;
    hDevice->pMacDevice = NULL;

end:
    return result;
}

/*!
 * @brief           ADIN2111 driver enable.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
 * @details         Enable the switch operation by bringing both PHYs out of software powerdown and establishing links.
 *
 * @sa              adin2111_Disable()
 * @sa              adin2111_DisablePort()
 * @sa              adin2111_EnablePort()
 */
adi_eth_Result_e adin2111_Enable(adin2111_DeviceHandle_t hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    /* Powerup Port 1. Note this needs to be done before Port 2. */
    result = adin2111_EnablePort(hDevice, ADIN2111_PORT_1);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    /* Now powerup Port 2. */
    result = adin2111_EnablePort(hDevice, ADIN2111_PORT_2);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

end:
    return result;
}


/*!
 * @brief           ADIN2111 driver enable for a single port.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port to be enabled.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
 * @details         Enable a single port by bringing the PHY out of software powerdown and establishing link.
 *
 * @sa              adin2111_Enable()
 * @sa              adin2111_Disable()
 * @sa              adin2111_DisablePort()
 */
adi_eth_Result_e adin2111_EnablePort(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    result = phyDriverEntry.ExitSoftwarePowerdown(hDevice->pPhyDevice[port]);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    hDevice->portEnabled[port] = true;

end:
    return result;
}

/*!
 * @brief           ADIN2111 driver disable.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
 * @details         Disable the switch operation by bringing both PHYs into software powerdown.
 *
 * @sa              adin2111_Enable()
 * @sa              adin2111_EnablePort()
 * @sa              adin2111_DisablePort()
 */
adi_eth_Result_e adin2111_Disable(adin2111_DeviceHandle_t hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    result = adin2111_DisablePort(hDevice, ADIN2111_PORT_1);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    result = adin2111_DisablePort(hDevice, ADIN2111_PORT_2);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

end:
    return result;
}

/*!
 * @brief           ADIN2111 driver disable for a single port.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port to be disabled.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
 * @details         Disable a single port by bringing the PHY into software powerdown.
 *
 * @sa              adin2111_EnablePort()
 * @sa              adin2111_Enable()
 * @sa              adin2111_Disable()
 */
adi_eth_Result_e adin2111_DisablePort(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    result = phyDriverEntry.EnterSoftwarePowerdown(hDevice->pPhyDevice[port]);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    hDevice->portEnabled[(uint32_t)port] = false;

end:
    return result;
}

/*!
 * @brief           Get device identity.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     pDevId      Device identity.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
 * @details         This function reads device identity from port 1 PHY.
 *
 */
adi_eth_Result_e adin2111_GetDeviceId(adin2111_DeviceHandle_t hDevice, adin2111_DeviceId_t *pDevId)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;

    result = adin2111_PhyRead(hDevice, ADIN2111_PORT_1, ADDR_MMD1_DEV_ID1, &val16);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    pDevId->phyId = (val16 << 16);

    result = adin2111_PhyRead(hDevice, ADIN2111_PORT_1, ADDR_MMD1_DEV_ID2, &val16);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    pDevId->phyId |= val16;

    result = adin2111_PhyRead(hDevice, ADIN2111_PORT_1, ADDR_MGMT_PRT_PKG, &val16);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    pDevId->pkgType = val16;

    result = adin2111_PhyRead(hDevice, ADIN2111_PORT_1, 0x1E900E, &val16);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    pDevId->digRevNum = val16 & 0xFF;

end:
    return result;
}

/*!
 * @brief           ADIN2111 reset.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      resetType   Reset type.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
 * @details         Software reset, only MAC reset currently supported.
 *                  Resetting the MAC clears all user-configured MAC settings, and reinitializes interrupt masks to the driver defaults.
 *                  Before resuming frame transmission/reception, reconfigure the MAC as needed and then call adin2111_SyncConfig().
 *
 * @sa
 */
adi_eth_Result_e adin2111_Reset(adin2111_DeviceHandle_t hDevice, adi_eth_ResetType_e resetType)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if (resetType == ADI_ETH_RESET_TYPE_MAC_ONLY)
    {
        result = macDriverEntry.Reset(hDevice->pMacDevice, resetType);
    }
    else if (resetType == ADI_ETH_RESET_TYPE_MAC_PHY)
    {
        /* Need to separately reset PHYs because the MAC software reset can only reset one port. */
        result = phyDriverEntry.Reset(hDevice->pPhyDevice[ADIN2111_PORT_1], ADI_PHY_RESET_TYPE_SW);
        if (result != ADI_ETH_SUCCESS)
        {
            goto end;
        }
        result = phyDriverEntry.Reset(hDevice->pPhyDevice[ADIN2111_PORT_2], ADI_PHY_RESET_TYPE_SW);
        if (result != ADI_ETH_SUCCESS)
        {
            goto end;
        }
        result = macDriverEntry.Reset(hDevice->pMacDevice, ADI_ETH_RESET_TYPE_MAC_ONLY);
    }
    else
    {
        result = ADI_ETH_INVALID_PARAM;
    }

    /* Re-initialize the PHY if needed */
    if ((result == ADI_ETH_SUCCESS) && (resetType == ADI_ETH_RESET_TYPE_MAC_PHY))
    {
        result = phyDriverEntry.ReInitPhy(hDevice->pPhyDevice[ADIN2111_PORT_1]);
        if (result != ADI_ETH_SUCCESS)
        {
            goto end;
        }
        result = phyDriverEntry.ReInitPhy(hDevice->pPhyDevice[ADIN2111_PORT_2]);
        if (result != ADI_ETH_SUCCESS)
        {
            goto end;
        }
        /* Pin configuration is initially done in adin2111_Init, so we must reapply it here */
        result = configureTsPins(hDevice);
    }
end:
    return result;
}

/*!
 * @brief           ADIN2111 configuration synchronization.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
* @details         This is a requirement for both OPEN Alliance SPI and Generic SPI. It will set \ref adi_mac_Device_t.configSync to TRUE,
 *                  which prevents modification of device configuration options via the APIs listed below until adin2111_Reset() or adin2111_Init() is called:
 *                  - adin2111_SetChunkSize()
 *                  - adin2111_SetFifoSizes()
 *                  - adin2111_SetCutThroughMode()
 *
 *                  For OPEN Alliance SPI, this function will aditionally set the bit CONFIG0.SYNC to indicate the device is configured.
 *                  CONFIG0.SYNC is cleared on reset, and the MAC device will not transmit or receive frames until CONFIG0.SYNC is set.
 *
 * @sa              adin2111_Reset()
 */
adi_eth_Result_e adin2111_SyncConfig(adin2111_DeviceHandle_t hDevice)
{
    return macDriverEntry.SyncConfig(hDevice->pMacDevice);
}

/*!
 * @brief           Read link status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [out]     linkStatus  Link status.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
 * @details         This function uses the link status information available
 *                  in the MAC STATUS register. It does not read from the PHY registers.
 *
 * @sa
 */
adi_eth_Result_e adin2111_GetLinkStatus(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_eth_LinkStatus_e *linkStatus)
{
    return phyDriverEntry.GetLinkStatus(hDevice->pPhyDevice[port], (adi_phy_LinkStatus_e *)linkStatus);
}

/*!
 * @brief           Read MAC statistics counters.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [out]     stat        Statistics counters values.
 *
 * @details
 *
 * @sa
 */
adi_eth_Result_e adin2111_GetStatCounters(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_eth_MacStatCounters_t *stat)
{
    return macDriverEntry.GetStatCounters(hDevice->pMacDevice, port, stat);
}

/*!
 * @brief           Enable/disable the status LED.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [out]     enable      Enable/disable the status LED.
 *
 * @details
 *
 * @sa
 */
adi_eth_Result_e adin2111_LedEn(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool enable)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.LedEn(hDevice->pPhyDevice[port], ADI_PHY_LED_0, enable);
    }

    return result;
}

/*!
 * @brief           Set loopback mode.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      port            Port number.
 * @param [in]      loopbackMode    Loopback mode.
 *
 * @details         The ADIN2111 features a number of loopback modes that can
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
adi_eth_Result_e adin2111_SetLoopbackMode(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_LoopbackMode_e loopbackMode)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.SetLoopbackMode(hDevice->pPhyDevice[port], loopbackMode);
    }

    return result;
}

/*!
 * @brief           Set test mode.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      port            Port number.
 * @param [in]      testMode        Test mode.
 *
 * @details         The ADIN2111 features a number of test modes that can
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
adi_eth_Result_e adin2111_SetTestMode(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_TestMode_e testMode)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.SetTestMode(hDevice->pPhyDevice[port], testMode);
    }

    return result;
}

/*!
 * @brief           Set up MAC address filter and corresponding address rules.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      macAddr         MAC address.
 * @param [in]      macAddrMask     MAC address mask.
 * @param [in]      addrRule        Address rules.
 *
 * @details         Search for an available entry in the address filter table implemented in hardware. If no available entry is found,
 *                  it returns ADI_ETH_ADDRESS_FILTER_TABLE_FULL, otherwise the MAC address and corresponding address rule
 *                  will be written to the hardware registers. The MAC address mask can be used to indicate how many bits,
 *                  from left to right, the filter checks against the MAC address.
 *
 *                  If the address filter table is full, an entry can be made available using adin2111_ClearAddressFilter().
 *
 * @sa              adin2111_ClearAddressFilter()
 */
adi_eth_Result_e adin2111_AddAddressFilter(adin2111_DeviceHandle_t hDevice, uint8_t *macAddr, uint8_t *macAddrMask, adi_mac_AddressRule_t addrRule)
{
    return macDriverEntry.AddAddressFilter(hDevice->pMacDevice, macAddr, macAddrMask, addrRule.VALUE16);
}

/*!
 * @brief           Clear entry in the address filter table.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      addrIndex       Address index.
 *
 * @details         Frees up an entry in the hardware address filter table by resetting the address rules.
 *
 * @sa              adin2111_AddAddressFilter()
 */
adi_eth_Result_e adin2111_ClearAddressFilter(adin2111_DeviceHandle_t hDevice, uint32_t addrIndex)
{
    return macDriverEntry.ClearAddressFilter(hDevice->pMacDevice, addrIndex);
}

/*!
 * @brief           Submit Tx buffer.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      port            Port number.
 * @param [in]      pBufDesc        Pointer to the buffer descriptor.
 *
 * @details         Submits a new Tx buffer to the Tx queue, to be scheduled for transmission.
 *                  The buffer information is supplied in the form of a buffer descriptor.
 *                  If the Tx queue is full, an error code is returned to the caller.
 *                  Once the buffer is successfully downloaded to the ADIN2111 Tx FIFO, the
 *                  callback configured as part of the buffer descriptor (if not NULL) will be called.
 *                  Note this indicates the buffer is in the Tx FIFO, not that the transmission was
 *                  successful!
 *
 *                  The port argument instructs the driver to send the buffer to a specific port (port 1, port 2 or both),
 *                  or use the dynamic forwarding table to determine which port should be used. In the latter case, if the
 *                  destination MAC address does not exist in the dinamic forwarding table, it will send the buffer contents
 *                  through both ports.
 *
 *                  Note that in case the frame is sent to both ports, 2 consecutive entries in the Tx queue will be used.
 *
 * @sa
 */
adi_eth_Result_e adin2111_SubmitTxBuffer(adin2111_DeviceHandle_t hDevice, adin2111_TxPort_e port, adi_eth_BufDesc_t *pBufDesc)
{
    adi_eth_Result_e        result = ADI_ETH_SUCCESS;
    adi_mac_FrameHeader_t   header;
    adin2111_TxPort_e       txPort;

    header.VALUE16 = 0x0000;
    header.EGRESS_CAPTURE = pBufDesc->egressCapt;

    /* Most likely the buffer needs to be sent to either port 1 or port 2 */
    pBufDesc->refCount = 1;

    if ((port == ADIN2111_TX_PORT_1) || (port == ADIN2111_TX_PORT_2))
    {
        /* Single specified port to be used */
        txPort = port;
    }
    else
    {
        if (pBufDesc->pBuf[0] & 0x01)
        {
            /* Destination address is multicast */
            txPort = ADIN2111_TX_PORT_FLOOD;
        }
        else
        {
            /* Lookup the port based on the MAC frame destination address, can return a single port      */
            /* or "flood" if the MAC address does not match any entries in the dynamic forwarding table. */
            txPort = dynamicTableLookup(hDevice, &pBufDesc->pBuf[0]);
        }
    }

    if (txPort == ADIN2111_TX_PORT_FLOOD)
    {
        /* The frame will be sent twice, once for each port. */
        if (queueAvailable(&hDevice->pMacDevice->txQueue) < 2)
        {
            result = ADI_ETH_QUEUE_FULL;
            goto end;
        }

        /* Send frame to the port only if the port is enabled */
        if (hDevice->portEnabled[ADIN2111_TX_PORT_1])
        {
            /* If both ports are enabled, the reference count needs to be increased to 2, */
            /* otherwise the buffer will be returned to the buffer pool after it's sent   */
            /* to port 1 instead of doing it after sending to both port 1 and port 2.     */
            /* Note that the call to SubmitTxBuffer below will either succeed for both    */
            /* ports or fail for both ports. If it fails, the buffer descriptor will not  */
            /* be added to the queue and reference counter value is not applicable.       */
            if (hDevice->portEnabled[ADIN2111_TX_PORT_2])
            {
                pBufDesc->refCount = 2;
            }


            header.PORT = ADIN2111_TX_PORT_1;
            pBufDesc->port = ADIN2111_TX_PORT_1;
            result = macDriverEntry.SubmitTxBuffer(hDevice->pMacDevice, header, pBufDesc);
            if (result != ADI_ETH_SUCCESS)
            {
                goto end;
            }
        }

        /* Send frame to the port only if the port is enabled */
        if (hDevice->portEnabled[ADIN2111_TX_PORT_2])
        {
            header.PORT = ADIN2111_TX_PORT_2;
            pBufDesc->port = ADIN2111_TX_PORT_2;
            result = macDriverEntry.SubmitTxBuffer(hDevice->pMacDevice, header, pBufDesc);
            if (result != ADI_ETH_SUCCESS)
            {
                goto end;
            }
        }

    }
    else
    {
        header.PORT = txPort;
        pBufDesc->port = txPort;
        result = macDriverEntry.SubmitTxBuffer(hDevice->pMacDevice, header, pBufDesc);
    }


end:
    return result;
}

/*!
 * @brief           Submit Rx buffer.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      pBufDesc        Pointer to the buffer descriptor.
 *
 * @details         Submits a new buffer descriptor to the Rx queue. On receiving of a new frame, the buffer
 *                  descriptor will be populated with frame contents before a callback is used to notify the
 *                  application the buffer descriptor is available.
 *
 *                  If high priority queue is enabled by defining the symbol ADI_MAC_ENABLE_RX_QUEUE_HI_PRIO,
 *                  this will submit the buffer to the low (normal) priority queue.
 * @sa
 */
adi_eth_Result_e adin2111_SubmitRxBuffer(adin2111_DeviceHandle_t hDevice, adi_eth_BufDesc_t *pBufDesc)
{
    return macDriverEntry.SubmitRxBuffer(hDevice->pMacDevice, pBufDesc);
}

#if defined(ADI_MAC_ENABLE_RX_QUEUE_HI_PRIO)
/*!
 * @brief           Submit Rx buffer to the high priority queue.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      pBufDesc        Pointer to the buffer descriptor.
 *
 * @details         Submits a new buffer descriptor to the high priority Rx queue. On receiving of a new frame,
 *                  the buffer descriptor will be populated with frame contents before a callback is used to notify
 *                  the application the buffer descriptor is available.
 *
 * @sa
 */
adi_eth_Result_e adin2111_SubmitRxBufferHp(adin2111_DeviceHandle_t hDevice, adi_eth_BufDesc_t *pBufDesc)
{
    return macDriverEntry.SubmitRxBufferHp(hDevice->pMacDevice, pBufDesc);
}
#endif

#if defined(CONFIG_SPI_OA_EN)
/*!
 * @brief           Configure the chunk size used in OPEN Alliance frame transfers.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [out]     cps             Chunk payload selector (CPS) value to use.
 *
 * @details         Chunk size must be set before the configuration is synchronized.
 *                  Attempting to set chunk size after calling adin2111_SyncConfig() will return
 *                  an ADI_ETH_CONFIG_SYNC_ERROR.
 *
 * @sa              adin2111_GetChunkSize()
 */
adi_eth_Result_e adin2111_SetChunkSize(adin2111_DeviceHandle_t hDevice, adi_mac_OaCps_e cps)
{
    return macDriverEntry.SetChunkSize(hDevice->pMacDevice, cps);
}

/*!
 * @brief           Get current chunk size used in OPEN Alliance frame transfers.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [out]     pCps            Returns the current chunk payload selector (CPS) value.
 *
 * @details
 *
 * @sa              adin2111_SetChunkSize()
 */
adi_eth_Result_e    adin2111_GetChunkSize           (adin2111_DeviceHandle_t hDevice, adi_mac_OaCps_e *pCps)
{
    return macDriverEntry.GetChunkSize(hDevice->pMacDevice, pCps);
}
#endif

/*!
 * @brief           Enable or disable cut-through mode.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      txcte           Enable cut-through in transmit.
 * @param [in]      rxcte           Enable cut-through in receive. Only supported in OPEN Alliance SPI.
 * @param [in]      p2pcte          Enable port-to-port cut-through.
 *
 * @details         Cut-through mode must be set before the configuration is synchronized.
 *                  Attempting to set cut-through mode after calling adin2111_SyncConfig() will return
 *                  an ADI_ETH_CONFIG_SYNC_ERROR.
 *
 * @sa              adin2111_GetCutThroughMode()
 */
adi_eth_Result_e adin2111_SetCutThroughMode(adin2111_DeviceHandle_t hDevice, bool txcte, bool rxcte, bool p2pcte)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            addr = ADDR_MAC_CONFIG2;
    uint32_t            mask = (p2pcte << BITP_MAC_CONFIG2_PORT_CUT_THRU_EN);
    uint32_t            val;

    /* Configure txcte and rxcte */
    result = macDriverEntry.SetCutThroughMode(hDevice->pMacDevice, txcte, rxcte);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    /* Configure p2pcte */
    result = adin2111_ReadRegister(hDevice, addr, &val);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    val = (val & ~BITM_MAC_CONFIG2_PORT_CUT_THRU_EN) | mask;

    result = MAC_WriteRegister(hDevice->pMacDevice, addr, val);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
end:
    return result;
}

/*!
 * @brief           Get cut-through mode.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      pTxcte          Returns true if cut-through is enabled in transmit.
 * @param [in]      pRxcte          Returns true if cut-through is enabled in receive. Only supported in OPEN Alliance SPI.
 * @param [in]      pP2pcte         Returns true if port-to-port cut-through is enabled.
 *
 * @details
 *
 * @sa              adin2111_SetCutThroughMode()
 */
adi_eth_Result_e    adin2111_GetCutThroughMode      (adin2111_DeviceHandle_t hDevice, bool *pTxcte, bool *pRxcte, bool *pP2pcte)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            addr    = ADDR_MAC_CONFIG2;
    uint32_t            mask    = BITM_MAC_CONFIG2_PORT_CUT_THRU_EN;
    uint32_t            val;

    /* Get txcte and rxcte */
    result = macDriverEntry.GetCutThroughMode(hDevice->pMacDevice, pTxcte, pRxcte);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    /* Get p2pcte */
    result = MAC_ReadRegister(hDevice->pMacDevice, addr, &val);
    if (result != ADI_ETH_SUCCESS)
    {
        goto end;
    }

    *pP2pcte = (bool)(val & mask);
end:
    return result;
}

/*!
 * @brief           Set the sizes of the FIFOs.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      fifoSizes       Set of FIFO size enumerations to use.
 *
 * @details         FIFO sizes must be set before the configuration is synchronized.
 *                  Attempting to set FIFO sizes after calling adin2111_SyncConfig will return
 *                  an ADI_ETH_CONFIG_SYNC_ERROR.
 *
 * @sa              adin2111_GetFifoSizes()
 */
adi_eth_Result_e adin2111_SetFifoSizes(adin2111_DeviceHandle_t hDevice, adi_mac_FifoSizes_t fifoSizes)
{
    uint32_t writeVal;
    uint32_t totalSize =    (2 * (uint32_t)fifoSizes.htxSize) +
                            (2 * (uint32_t)fifoSizes.p1RxLoSize) + (2 * (uint32_t)fifoSizes.p1RxHiSize) +
                            (2 * (uint32_t)fifoSizes.p2RxLoSize) + (2 * (uint32_t)fifoSizes.p2RxHiSize) +
                            (2 * (uint32_t)fifoSizes.p1TxSize) + (2 * (uint32_t)fifoSizes.p2TxSize);

    if (totalSize > ADI_MAC_FIFO_MAX_SIZE)
    {
        return ADI_ETH_FIFO_SIZE_ERROR;
    }
    else
    {
        writeVal =  (fifoSizes.htxSize    << BITP_MAC_FIFO_SIZE_HTX_SIZE) |
                    (fifoSizes.p1RxLoSize << BITP_MAC_FIFO_SIZE_P1_RX_LO_SIZE) |
                    (fifoSizes.p1RxHiSize << BITP_MAC_FIFO_SIZE_P1_RX_HI_SIZE) |
                    (fifoSizes.p2RxLoSize << BITP_MAC_FIFO_SIZE_P2_RX_LO_SIZE) |
                    (fifoSizes.p2RxHiSize << BITP_MAC_FIFO_SIZE_P2_RX_HI_SIZE) |
                    (fifoSizes.p1TxSize << BITP_MAC_FIFO_SIZE_P1_TX_SIZE) |
                    (fifoSizes.p2TxSize << BITP_MAC_FIFO_SIZE_P2_TX_SIZE);

        return macDriverEntry.SetFifoSizes(hDevice->pMacDevice, writeVal);
    }
}

/*!
 * @brief           Get the current sizes of the FIFOs.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      pFifoSizes      Returns the current set of FIFO size enumerations.
 *
 * @details
 *
 * @sa              adin2111_SetFifoSizes()
 */
adi_eth_Result_e adin2111_GetFifoSizes(adin2111_DeviceHandle_t hDevice, adi_mac_FifoSizes_t *pFifoSizes)
{
    adi_eth_Result_e    result  = ADI_ETH_SUCCESS;
    uint32_t            readVal;

    result = macDriverEntry.GetFifoSizes(hDevice->pMacDevice, &readVal);
    pFifoSizes->htxSize     = (adi_mac_HtxFifoSize_e)((readVal & BITM_MAC_FIFO_SIZE_HTX_SIZE) >> BITP_MAC_FIFO_SIZE_HTX_SIZE);
    pFifoSizes->p1RxLoSize  = (adi_mac_RxFifoSize_e)((readVal & BITM_MAC_FIFO_SIZE_P1_RX_LO_SIZE) >> BITP_MAC_FIFO_SIZE_P1_RX_LO_SIZE);
    pFifoSizes->p1RxHiSize  = (adi_mac_RxFifoSize_e)((readVal & BITM_MAC_FIFO_SIZE_P1_RX_HI_SIZE) >> BITP_MAC_FIFO_SIZE_P1_RX_HI_SIZE);
    pFifoSizes->p2RxLoSize  = (adi_mac_RxFifoSize_e)((readVal & BITM_MAC_FIFO_SIZE_P2_RX_LO_SIZE) >> BITP_MAC_FIFO_SIZE_P2_RX_LO_SIZE);
    pFifoSizes->p2RxHiSize  = (adi_mac_RxFifoSize_e)((readVal & BITM_MAC_FIFO_SIZE_P2_RX_HI_SIZE) >> BITP_MAC_FIFO_SIZE_P2_RX_HI_SIZE);
    pFifoSizes->p1TxSize    = (adi_mac_PtxFifoSize_e)((readVal & BITM_MAC_FIFO_SIZE_P1_TX_SIZE) >> BITP_MAC_FIFO_SIZE_P1_TX_SIZE);
    pFifoSizes->p2TxSize    = (adi_mac_PtxFifoSize_e)((readVal & BITM_MAC_FIFO_SIZE_P2_TX_SIZE) >> BITP_MAC_FIFO_SIZE_P2_TX_SIZE);

    return result;
}

/*!
 * @brief           Clear FIFOs.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      clearMode       Sets which FIFOs will be cleared. Multiple options can be selected with an OR operation.
 *
 * @details
 *
 * @sa
 */
adi_eth_Result_e adin2111_ClearFifos(adin2111_DeviceHandle_t hDevice, adi_mac_FifoClrMode_e clearMode)
{
    return macDriverEntry.ClearFifos(hDevice->pMacDevice, clearMode);
}

/*!
 * @brief           Enable/disable promiscuous mode.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      port            Port number.
 * @param [in]      bFlag           Enable promiscuous mode for the specified port if true, disable if false.
 *
 * @details         Configures the MAC to forward to host all frames received on the specified port
 *                  whose destination address does not match any of of the addresses configured in the address filters.
 *                  Multiple forwarding modes can be configured simultaneously with multiple calls to
 *                  adin2111_SetPortForwardMode() and/or adin2111_SetPromiscuousMode().
 *                  The existing address filters configuration is not changed.
 *                  Also, frames considered invalid by the MAC are not forwarded to the host and
 *                  instead are dropped by the hardware.
 *
 * @sa              adin2111_GetPromiscuousMode()
 */
adi_eth_Result_e adin2111_SetPromiscuousMode(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool bFlag)
{
    adi_eth_Result_e result = ADI_ETH_SUCCESS;
    adi_mac_ForwardMode_t mode;
    uint32_t mask = 0;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        if (port == ADIN2111_PORT_1)
        {
            mode.P1_FWD_UNK_TO_HOST = bFlag;
            mask = BITM_MAC_CONFIG2_P1_FWD_UNK2HOST;
        }
        else if (port == ADIN2111_PORT_2)
        {
            mode.P2_FWD_UNK_TO_HOST = bFlag;
            mask = BITM_MAC_CONFIG2_P2_FWD_UNK2HOST;
        }
        result = macDriverEntry.SetForwardMode(hDevice->pMacDevice, mode, mask);
    }

    return result;
}

/*!
 * @brief           Get promiscuous mode status.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      port            Port number.
 * @param [out]     pFlag           Returns true if promiscuous mode is enabled for the specified port, false if disabled.
 *
 * @details
 *
 * @sa              adin2111_SetPromiscuousMode()
 */
adi_eth_Result_e adin2111_GetPromiscuousMode(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool *pFlag)
{
    adi_eth_Result_e result = ADI_ETH_SUCCESS;
    adi_mac_ForwardMode_t mode;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = macDriverEntry.GetForwardMode(hDevice->pMacDevice, &mode);
        if (port == ADIN2111_PORT_1)
        {
            *pFlag = mode.P1_FWD_UNK_TO_HOST;
        }
        else if (port == ADIN2111_PORT_2)
        {
            *pFlag = mode.P2_FWD_UNK_TO_HOST;
        }

    }
    return result;
}

/*!
 * @brief           Enable/disable port forwarding mode.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      port            Port number.
 * @param [in]      bFlag           Enable port to port forwarding mode for the specified port if true, disable if false.
 *
 * @details         Configures the MAC to forward to the other port all frames received on the specified port
 *                  whose destination address does not match any of of the addresses configured in the address filters.
 *                  Multiple forwarding modes can be configured simultaneously with multiple calls to
 *                  adin2111_SetPortForwardMode() and/or adin2111_SetPromiscuousMode().
 *                  The existing address filters configuration is not changed.
 *                  Also, frames considered invalid by the MAC are not forwarded to the host and
 *                  instead are dropped by the hardware.
 *
 * @sa              adin2111_GetPortForwardMode()
 */
adi_eth_Result_e adin2111_SetPortForwardMode(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool bFlag)
{
    adi_eth_Result_e result = ADI_ETH_SUCCESS;
    adi_mac_ForwardMode_t mode;
    uint32_t mask = 0;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        if (port == ADIN2111_PORT_1)
        {
            mode.P1_FWD_UNK_TO_P2 = bFlag;
            mask = BITM_MAC_CONFIG2_P1_FWD_UNK2P2;
        }
        else if (port == ADIN2111_PORT_2)
        {
            mode.P2_FWD_UNK_TO_P1 = bFlag;
            mask = BITM_MAC_CONFIG2_P2_FWD_UNK2P1;
        }
        result = macDriverEntry.SetForwardMode(hDevice->pMacDevice, mode, mask);
    }

    return result;
}

/*!
 * @brief           Get port forwarding mode status.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      port            Port number.
 * @param [out]     pFlag           Returns true if port to port forwarding mode is enabled for the specified port, false if disabled.
 *
 * @details
 *
 * @sa              adin2111_SetPortForwardMode()
 */
adi_eth_Result_e adin2111_GetPortForwardMode(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool *pFlag)
{
    adi_eth_Result_e result = ADI_ETH_SUCCESS;
    adi_mac_ForwardMode_t mode;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = macDriverEntry.GetForwardMode(hDevice->pMacDevice, &mode);
        if (port == ADIN2111_PORT_1)
        {
            *pFlag = mode.P1_FWD_UNK_TO_P2;
        }
        else if (port == ADIN2111_PORT_2)
        {
            *pFlag = mode.P2_FWD_UNK_TO_P1;
        }

    }
    return result;
}

/*!
 * @brief           Enable timestamp counters and capture of receive timestamps.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      format          Timestamp format:
 *                                  - 32b free-running counter: 1 lsb = 1/120MHz = 8.33ns.
 *                                  - 32b 1588 timestamp: [31:30] seconds count, [29:0] nanoseconds count.
 *                                  - 64b 1588 timestamp: [63:32] seconds count, [31:30] zero, [29:0] nanoseconds count.
 *
 * @details         Timestamps must be configured before the configuration is synchronized.
 *                  Attempting to enable timestamps after calling adin2111_SyncConfig() will return
 *                  an #ADI_ETH_CONFIG_SYNC_ERROR.
 *
 * @sa
 */
adi_eth_Result_e adin2111_TsEnable(adin2111_DeviceHandle_t hDevice, adi_mac_TsFormat_e format)
{
    return macDriverEntry.TsEnable(hDevice->pMacDevice, format);
}

/*!
 * @brief           Synchronously clear all timestamp counters.
 *
 * @param [in]      hDevice         Device driver handle.
 *
 * @details
 *
 * @sa
 */
adi_eth_Result_e adin2111_TsClear(adin2111_DeviceHandle_t hDevice)
{
    return macDriverEntry.TsClear(hDevice->pMacDevice);
}

/*!
 * @brief           Configure and start TS_TIMER waveform generation.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      pTimerConfig    Pointer to a timer configuration structure.
 *
 * @details         It is required to run adin2111_TsEnable() to enable the timer block before running this function.
 *                  Use the timer configuration to set the period (in nanoseconds), duty cycle, idle state, and start time (in nanoseconds).
 *                  Start time must be greater than or equal to 16. Waveform generation will begin when the nanosecond counter
 *                  is equal to the start time.
 *
 * @sa              adin2111_TsEnable()
 * @sa              adin2111_TsTimerStop()
 */
adi_eth_Result_e adin2111_TsTimerStart(adin2111_DeviceHandle_t hDevice, adi_mac_TsTimerConfig_t *pTimerConfig)
{
    return macDriverEntry.TsTimerStart(hDevice->pMacDevice, pTimerConfig);
}

/*!
 * @brief           Halt the TS_TIMER waveform generation.
 *
 * @param [in]      hDevice         Device driver handle.
 *
 * @details
 *
 * @sa              adin2111_TsTimerStart()
 */
adi_eth_Result_e adin2111_TsTimerStop(adin2111_DeviceHandle_t hDevice)
{
    return macDriverEntry.TsTimerStop(hDevice->pMacDevice);
}

/*!
 * @brief           Set the internal seconds and nanoseconds counters to the given values.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      seconds         Seconds value to program to the seconds counter.
 * @param [in]      nanoseconds     Nanoseconds value to program to the nanoseconds counter.
 *
 * @details         It is required to run adin2111_TsEnable() to enable the timer block before running this function.
 *                  Nanoseconds value will automatically be coerced to the resolution and limits of the counter.
 *                  Use adin2111_TsSyncClock() for fine frequency adjustment.
 *
 * @sa              adin2111_TsEnable()
 * @sa              adin2111_TsSyncClock()
 */
adi_eth_Result_e adin2111_TsSetTimerAbsolute(adin2111_DeviceHandle_t hDevice, uint32_t seconds, uint32_t nanoseconds)
{
    return macDriverEntry.TsSetTimerAbsolute(hDevice->pMacDevice, seconds, nanoseconds);
}

/*!
 * @brief           Calculate and adjust the counter accumulator addend to adjust its frequency to zero out the given time difference.
 *
 * @param [in]      hDevice                 Device driver handle.
 * @param [in]      tError                  Time difference to correct, e.g. reference time minus local time. Positive or negative value.
 * @param [in]      referenceTimeNsDiff     Current time minus previous time from the reference clock source. Positive value.
 * @param [in]      localTimeNsDiff         Current time minus previous time from the local clock source. Positive value.
 *
 * @details         It is required to run adin2111_TsEnable() to enable the timer block before running this function.
 *                  Use adin2111_TsSetTimerAbsolute() for coarse timer adjustment to an absolute number of seconds and nanoseconds.
 *                  Use adin2111_TsSubtract() to calculate the difference between parsed timestamps stored in timespec structs.
 *
 * @sa              adin2111_TsEnable()
 * @sa              adin2111_TsSetTimerAbsolute()
 */
adi_eth_Result_e adin2111_TsSyncClock(adin2111_DeviceHandle_t hDevice, int64_t tError, uint64_t referenceTimeNsDiff, uint64_t localTimeNsDiff)
{
    return macDriverEntry.TsSyncClock(hDevice->pMacDevice, tError, referenceTimeNsDiff, localTimeNsDiff);
}

/*!
 * @brief           Retrieve and parse the TTSC* transmit timestamp that is captured if directed by the frame header.
 *
 * @param [in]      hDevice             Device driver handle.
 * @param [in]      egressReg           Enumeration value to select readback of TTSCA, TTSCB, or TTSCC.
 * @param [in]      pCapturedTimespec   Pointer to a timespec struct to hold the parsed timestamp data.
 *
 * @details         The MAC-PHY can be directed to capture a transmit timestamp by setting the \ref adi_eth_BufDesc_t.egressCapt field.
 *                  It is required to run adin2111_TsEnable() to enable the timer block before running this function.
 *                  Uses the timestamp format that was set via adin2111_TsEnable() and adin2111_TsConvert() to parse.
 *
 * @sa              adin2111_TsEnable()
 * @sa              adin2111_TsConvert()
 */
adi_eth_Result_e adin2111_TsGetEgressTimestamp(adin2111_DeviceHandle_t hDevice, adi_mac_EgressCapture_e egressReg, adi_mac_TsTimespec_t *pCapturedTimespec)
{
    return macDriverEntry.TsGetEgressTimestamp(hDevice->pMacDevice, egressReg, pCapturedTimespec);
}

/*!
 * @brief           Parses the a timestamp in a specific format.
 *
 * @param [in]      timestampLowWord    Lower 32 bits of timestamp.
 * @param [in]      timestampHighWord   Upper 32 bits of timestamp. Can be zero if using a 32-bit format.
 * @param [in]      format              Enumeration value to select the timestamp format to be used to parse the raw values.
 * @param [in]      pTimespec           Pointer to a timespec struct to hold the parsed timestamp data.
 *
 * @details
 *
 * @sa
 */
adi_eth_Result_e adin2111_TsConvert(uint32_t timestampLowWord, uint32_t timestampHighWord, adi_mac_TsFormat_e format, adi_mac_TsTimespec_t *pTimespec)
{
    return macDriverEntry.TsConvert(timestampLowWord, timestampHighWord, format, pTimespec);
}

/*!
 * @brief           Calculate the difference between two parsed timestamps in nanoseconds.
 *
 * @param [in]      pTsA        Pointer to timestamp value A.
 * @param [in]      pTsB        Pointer to timestamp value B.
 *
 * @returns         Nanoseconds value representing TsA - TsB.
 *
 * @details         Difference will be negative if TsB is greater than TsA.
 *
 * @sa
 */
int64_t adin2111_TsSubtract(adi_mac_TsTimespec_t *pTsA, adi_mac_TsTimespec_t *pTsB)
{
    return macDriverEntry.TsSubtract(pTsA, pTsB);
}

/*!
 * @brief           Register callback for interrupt events.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      cbFunc      Callback function.
 * @param [in]      cbEvent    Callback event.
 *
 * @details         This is separate from the callbacks configured for the Tx/Rx frames.
 *                  It notifies the application when configured events occur.
 *
 * @sa
 */
adi_eth_Result_e adin2111_RegisterCallback(adin2111_DeviceHandle_t hDevice, adi_eth_Callback_t cbFunc, adi_mac_InterruptEvt_e cbEvent)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;

    if (cbEvent == ADI_MAC_EVT_LINK_CHANGE)
    {
        /* Enable event in PHY registers for both ports */
        result = adin2111_PhyRead(hDevice, ADIN2111_PORT_1, ADDR_PHY_SUBSYS_IRQ_MASK, &val16);
        if (result != ADI_ETH_SUCCESS)
        {
            goto end;
        }

        val16 |= BITM_PHY_SUBSYS_IRQ_MASK_LINK_STAT_CHNG_IRQ_EN;
        result = adin2111_PhyWrite(hDevice, ADIN2111_PORT_1, ADDR_PHY_SUBSYS_IRQ_MASK, val16);
        if (result != ADI_ETH_SUCCESS)
        {
            goto end;
        }

        result = adin2111_PhyRead(hDevice, ADIN2111_PORT_2, ADDR_PHY_SUBSYS_IRQ_MASK, &val16);
        if (result != ADI_ETH_SUCCESS)
        {
            goto end;
        }

        val16 |= BITM_PHY_SUBSYS_IRQ_MASK_LINK_STAT_CHNG_IRQ_EN;
        result = adin2111_PhyWrite(hDevice, ADIN2111_PORT_2, ADDR_PHY_SUBSYS_IRQ_MASK, val16);
        if (result != ADI_ETH_SUCCESS)
        {
            goto end;
        }

        /* Update PHY IRQ mask stored in the driver */
        hDevice->pMacDevice->phyIrqMask |= (BITM_PHY_SUBSYS_IRQ_MASK_LINK_STAT_CHNG_IRQ_EN << 16);
    }

    result = macDriverEntry.RegisterCallback(hDevice->pMacDevice, cbFunc, cbEvent, (void *)hDevice);
end:
    return result;
}

/*!
 * @brief           Write to a MAC register.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      regAddr     Register address.
 * @param [in]      regData     Register data.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
 * @details
 *
 * @sa              adin2111_ReadRegister()
 */
adi_eth_Result_e adin2111_WriteRegister(adin2111_DeviceHandle_t hDevice, uint16_t regAddr, uint32_t regData)
{
    return macDriverEntry.WriteRegister(hDevice->pMacDevice, regAddr, regData);
}

/*!
 * @brief           Read from a MAC register.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      regAddr     Register address.
 * @param [out]     regData     Register data.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
 * @details
 *
 * @sa              adin2111_WriteRegister()
 */
adi_eth_Result_e adin2111_ReadRegister(adin2111_DeviceHandle_t hDevice, uint16_t regAddr, uint32_t *regData)
{
    return macDriverEntry.ReadRegister(hDevice->pMacDevice, regAddr, regData);
}

/*!
 * @brief           Write to a PHY register.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [in]      regAddr     Register address.
 * @param [in]      regData     Register data.
 *
 * @details
 *
 * @sa              adin2111_PhyRead()
 */
adi_eth_Result_e adin2111_PhyWrite(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint32_t regAddr, uint16_t regData)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = macDriverEntry.PhyWrite(hDevice->pMacDevice, hDevice->pPhyDevice[port]->phyAddr, regAddr, regData);;
    }

    return result;
}

/*!
 * @brief           Read from a PHY register.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [in]      regAddr     Register address.
 * @param [out]     regData     Register data.
 *
 * @details
 *
 * @sa              adin2111_PhyWrite()
 */
adi_eth_Result_e adin2111_PhyRead(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint32_t regAddr, uint16_t *regData)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = macDriverEntry.PhyRead(hDevice->pMacDevice, hDevice->pPhyDevice[port]->phyAddr, regAddr, regData);
    }

    return result;
}

/*!
 * @brief           Get link quality measure based on the Mean Square Error (MSE) value.
 *
 * @param [in]      hDevice         Device driver handle.
 * @param [in]      port            Port number.
 * @param [out]     mseLinkQuality  Link quality struct.
 *
 * @details
 *
 */
adi_eth_Result_e adin2111_GetMseLinkQuality(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_MseLinkQuality_t *mseLinkQuality)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.GetMseLinkQuality(hDevice->pPhyDevice[port], mseLinkQuality);
    }

    return result;
}


/*!
 * @brief           Enable/disable frame generator.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [in]      enable      Enable flag.
 *
 * @details         Enables/disables the frame generator based on the flag value.
 *
 */
adi_eth_Result_e adin2111_FrameGenEn(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool enable)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameGenEn(hDevice->pPhyDevice[port], enable);
    }

    return result;
}

/*!
 * @brief           Set frame generator mode.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [in]      mode        Frame generator mode.
 *
 * @details         Sets one of the following frame generator modes:
 *                  - #ADI_PHY_FRAME_GEN_MODE_BURST: burst mode
 *                  - #ADI_PHY_FRAME_GEN_MODE_CONT: continuous mode
 *
 */
adi_eth_Result_e adin2111_FrameGenSetMode(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_FrameGenMode_e mode)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameGenSetMode(hDevice->pPhyDevice[port], mode);
    }

    return result;
}

/*!
 * @brief           Set frame generator frame counter.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [in]      frameCnt    Frame count.
 *
 * @details         Updates the frame counter to the frameCnt value.
 *                  The 32-bit argument is split in 16-bit values in
 *                  order to update the 2 register FG_NFRM_H/FFG_NFRM_L.
 *
 */
adi_eth_Result_e adin2111_FrameGenSetFrameCnt(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint32_t frameCnt)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameGenSetFrameCnt(hDevice->pPhyDevice[port], frameCnt);
    }

    return result;
}

/*!
 * @brief           Set frame generator payload.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
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
adi_eth_Result_e adin2111_FrameGenSetFramePayload(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_FrameGenPayload_e payload)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameGenSetFramePayload(hDevice->pPhyDevice[port], payload);
    }

    return result;
}

/*!
 * @brief           Set frame generator frame length.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
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
adi_eth_Result_e adin2111_FrameGenSetFrameLen(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint16_t frameLen)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameGenSetFrameLen(hDevice->pPhyDevice[port], frameLen);
    }

    return result;
}

/*!
 * @brief           Set frame generator interframe gap.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [in]      ifgLen      Interframe gap.
 *
 * @details
 *
 */
adi_eth_Result_e adin2111_FrameGenSetIfgLen(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint16_t ifgLen)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameGenSetIfgLen(hDevice->pPhyDevice[port], ifgLen);
    }

    return result;
}

/*!
 * @brief           Restart frame generator.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 *
 * @details         Restarts the frame generator, this is required
 *                  the enable frame generation. Before restart,
 *                  the function will read the FG_DONE bit to clear it,
 *                  in case it's still set from a previous run.
 *
 */
adi_eth_Result_e adin2111_FrameGenRestart(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameGenRestart(hDevice->pPhyDevice[port]);
    }

    return result;
}

/*!
 * @brief           Read frame generator status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [out]     fgDone      Frame generator status.
 *
 * @details         Read the FG_DONE bit to determine if the frame generation is complete.
 *
 */
adi_eth_Result_e adin2111_FrameGenDone(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool *fgDone)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameGenDone(hDevice->pPhyDevice[port], fgDone);
    }

    return result;
}

/*!
 * @brief           Enable/disable frame checker.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [in]      enable      Enable flag.
 *
 * @details         Enables/disables the frame checker based on the flag value.
 *
 */
adi_eth_Result_e adin2111_FrameChkEn(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool enable)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameChkEn(hDevice->pPhyDevice[port], enable);
    }

    return result;
}

/*!
 * @brief           Select frame checker source.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [in]      source      Frame checker source.
 *
 * @details         Sets one of the following frame checker sources:
 *                  - #ADI_PHY_FRAME_CHK_SOURCE_PHY
 *                  - #ADI_PHY_FRAME_CHK_SOURCE_MAC
 *
 */
 adi_eth_Result_e adin2111_FrameChkSourceSelect(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_FrameChkSource_e source)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameChkSourceSelect(hDevice->pPhyDevice[port], source);
    }

    return result;
}

/*!
 * @brief           Read frame checker frame count.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [out]     cnt         Frame count.
 *
 * @details         Note the frame count value is latched when reading RX_CNT_ERR, therefore
 *                  a call to this function should be preceded by a call to adin2111_FrameChkReadRxErrCnt().
 *
 * @sa              adin2111_FrameChkReadRxErrCnt()
 */
adi_eth_Result_e adin2111_FrameChkReadFrameCnt(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint32_t *cnt)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameChkReadFrameCnt(hDevice->pPhyDevice[port], cnt);
    }

    return result;
}

/*!
 * @brief           Read frame checker receive errors.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [out]     cnt         Error counter.
 *
 * @details         Note the frame counter/error counter values are latched when reading RX_CNT_ERR, therefore
 *                  a call to this function should preceded calls to adin2111_FrameChkReadFrameCnt()
 *                  or adin2111_FrameChkReadErrorCnt().
 *
 * @sa              adin2111_FrameChkReadFrameCnt()
 * @sa              adin2111_FrameChkReadErrorCnt()
 */
adi_eth_Result_e adin2111_FrameChkReadRxErrCnt(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint16_t *cnt)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameChkReadRxErrCnt(hDevice->pPhyDevice[port], cnt);
    }

    return result;
}

/*!
 * @brief           Read frame checker error counters.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      port        Port number.
 * @param [out]     cnt         Error counters.
 *
 * @details         Note the error counter values are latched when reading RX_CNT_ERR, therefore
 *                  a call to this function should be preceded by a call to adin2111_FrameChkReadRxErrCnt().
 *
 * @sa              adin2111_FrameChkReadRxErrCnt()
 */
adi_eth_Result_e adin2111_FrameChkReadErrorCnt(adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_FrameChkErrorCounters_t *cnt)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if ((port != ADIN2111_PORT_1) && (port != ADIN2111_PORT_2))
    {
        result = ADI_ETH_INVALID_PORT;
    }
    else
    {
        result = phyDriverEntry.FrameChkReadErrorCnt(hDevice->pPhyDevice[port], cnt);
    }

    return result;
}

/*!
 * @brief           Age tick for the dynamic forwarding table.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
 * @details         Indicate that an age tick has elapsed for dynamic forwarding table
 *                  management.
 *                  Should be called periodically.
 *
 */
adi_eth_Result_e adin2111_AgeTick(adin2111_DeviceHandle_t hDevice)
{
    for (uint32_t i = 0; i < ADIN2111_DYNAMIC_TABLE_SIZE; i++) {
        if (hDevice->dynamicTable[i].refresh) {
            hDevice->dynamicTable[i].age = ADIN2111_DYNAMIC_TABLE_MAX_AGE;
            hDevice->dynamicTable[i].refresh = 0;
        }
        else
        {
            if (hDevice->dynamicTable[i].age > 0)
            {
                hDevice->dynamicTable[i].age--;
            }
        }
  }

    return ADI_ETH_SUCCESS;
}

/*!
 * @brief           Clear all entries from the dynamic table.
 *
 * @param [in]      hDevice     Device driver handle.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *
 */
adi_eth_Result_e adin2111_ClearDynamicTable(adin2111_DeviceHandle_t hDevice)
{
    for (uint32_t i = 0; i < ADIN2111_DYNAMIC_TABLE_SIZE; i++) {
        hDevice->dynamicTable[i].age = 0;
        hDevice->dynamicTable[i].refresh = 0;
    }

    return ADI_ETH_SUCCESS;
}

/*! @cond PRIVATE */

/*!
 * Uses the Port 1 pinmux to configure TS_TIMER and TS_CAPT pin assignments.
 */
adi_eth_Result_e configureTsPins(adin2111_DeviceHandle_t hDevice)
{
    adi_eth_Result_e result = ADI_ETH_SUCCESS;
    adin2111_Port_e port = ADIN2111_PORT_1;
    uint16_t val;
    uint16_t mask = (BITM_DIGIO_PINMUX_DIGIO_TSTIMER_PINMUX | BITM_DIGIO_PINMUX_DIGIO_TSCAPT_PINMUX);

    /* Check for invalid pin assignments */
    if (((hDevice->tsTimerPin < ADIN2111_TS_TIMER_MUX_LED_0) || (hDevice->tsTimerPin > ADIN2111_TS_TIMER_MUX_NA)) ||
        ((hDevice->tsCaptPin  < ADIN2111_TS_CAPT_MUX_LED_1)  || (hDevice->tsCaptPin  > ADIN2111_TS_CAPT_MUX_NA)))
    {
        result = ADI_ETH_INVALID_PARAM;
        goto end;
    }
    else
    {
        result = adin2111_PhyRead(hDevice, port, ADDR_DIGIO_PINMUX, &val);
        if (result == ADI_ETH_SUCCESS)
        {
            val &= ~mask;
            val |= (((uint16_t)hDevice->tsTimerPin << BITP_DIGIO_PINMUX_DIGIO_TSTIMER_PINMUX) |
                    ((uint16_t)hDevice->tsCaptPin  << BITP_DIGIO_PINMUX_DIGIO_TSCAPT_PINMUX)) & mask;

            result = adin2111_PhyWrite(hDevice, port, ADDR_DIGIO_PINMUX, val);
        }
    }
end:
    return result;
}

/*!
 * Compare 2 MAC address byte by byte and return the result of comparison.
 */
static bool macAddrCompare(uint8_t *leftAddr, uint8_t *rightAddr)
{

    bool    result = true;

    for (uint32_t i = 0; i < 6; i++)
    {
        if (leftAddr[i] != rightAddr[i])
        {
            result = false;
            goto end;
        }
    }

end:
    return result;
}

/*!
 * Lookup an address in the dynamic table and return
 * the port that that host is on or FLOOD if not found.
 */
static adin2111_TxPort_e dynamicTableLookup(adin2111_DeviceHandle_t hDevice, uint8_t *macAddress)
{
    adin2111_TxPort_e   port;

    for (uint32_t i = 0; i < ADIN2111_DYNAMIC_TABLE_SIZE; i++) {
        if (hDevice->dynamicTable[i].age) {
            if (macAddrCompare(hDevice->dynamicTable[i].macAddress, macAddress))
            {
                port = (adin2111_TxPort_e)hDevice->dynamicTable[i].fromPort;
                goto end;
            }
        }
    }

    port = ADIN2111_TX_PORT_FLOOD;

end:
    return port;
}


/**
 * Lookup an address in the dynamic table and update
 * its source port/age value. If not found insert it into the table.
 */
static void dynamicTableLearn(adin2111_DeviceHandle_t hDevice, adin2111_TxPort_e port, uint8_t *macAddress)
{
    int32_t     firstEmpty = -1;

    for (uint32_t i = 0; i < ADIN2111_DYNAMIC_TABLE_SIZE; i++) {
        if (hDevice->dynamicTable[i].age) {
            if (macAddrCompare(hDevice->dynamicTable[i].macAddress, macAddress))
            {
                  hDevice->dynamicTable[i].fromPort = port;
                  hDevice->dynamicTable[i].refresh = 1;
                  goto end;
            }
        }
        else
        {
            if (firstEmpty < 0)
            {
                firstEmpty = i;
            }
        }
    }

    if (firstEmpty < 0) {
        firstEmpty = hDevice->dynamicTableReplaceLoc;
        hDevice->dynamicTableReplaceLoc++;
        if (hDevice->dynamicTableReplaceLoc >= ADIN2111_DYNAMIC_TABLE_SIZE)
        {
            hDevice->dynamicTableReplaceLoc = 0;
        }
    }

    hDevice->dynamicTable[firstEmpty].fromPort = port;
    for (uint32_t i = 0; i < 6; i++)
    {
        hDevice->dynamicTable[firstEmpty].macAddress[i] = macAddress[i];
    }
    hDevice->dynamicTable[firstEmpty].age = ADIN2111_DYNAMIC_TABLE_MAX_AGE;
    hDevice->dynamicTable[firstEmpty].refresh = 0;

end:
    return;
}

/*! @endcond */

/** @}*/
