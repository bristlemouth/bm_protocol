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

#include "adin1200_phy.h"

static adi_eth_Result_e         PHY_Init                    (adin1200_phy_Device_t **hPhyDevice, adi_phy_DriverConfig_t *cfg, void *adinDevice, HAL_ReadFn_t readFn, HAL_WriteFn_t writeFn);
static adi_eth_Result_e         PHY_UnInit                  (adin1200_phy_Device_t *hDevice);
static adi_eth_Result_e         PHY_ReInitPhy               (adin1200_phy_Device_t *hDevice);
static adi_eth_Result_e         PHY_GetDeviceId             (adin1200_phy_Device_t *hDevice, adinPhy_DeviceId_t *pDevId);

static adi_eth_Result_e         PHY_RegisterCallback        (adin1200_phy_Device_t *hDevice, adi_eth_Callback_t cbFunc, uint32_t cbEvents, void *cbParam);
static adi_eth_Result_e         PHY_ReadIrqStatus           (adin1200_phy_Device_t *hDevice, uint32_t *status);

static adi_eth_Result_e         PHY_SoftwarePowerdown       (adin1200_phy_Device_t *hDevice, adi_phy_swpd_e swpd);
static adi_eth_Result_e         PHY_GetSoftwarePowerdown    (adin1200_phy_Device_t *hDevice, bool *enable);

static adi_eth_Result_e         PHY_GetLinkStatus           (adin1200_phy_Device_t *hDevice, adi_phy_LinkStatus_e *status);
static adi_eth_Result_e         PHY_LinkConfig              (adin1200_phy_Device_t *hDevice, bool enable);

static adi_eth_Result_e         PHY_AnegEnable              (adin1200_phy_Device_t *hDevice, adi_phy_AnegEn_e enable);
static adi_eth_Result_e         PHY_AnegRestart             (adin1200_phy_Device_t *hDevice);
static adi_eth_Result_e         PHY_GetAnStatus             (adin1200_phy_Device_t *hDevice, adi_phy_AnegStatus_e *status);
static adi_eth_Result_e         PHY_AnegAdverConfig         (adin1200_phy_Device_t *hDevice, adi_phy_AnegAdv_e anegAdver);
static adi_eth_Result_e         PHY_AnegAdvertised           (adin1200_phy_Device_t *hDevice, uint16_t *phy_speed, uint16_t *phy_speed1G);

static adi_eth_Result_e         PHY_AutoMDIXConfig          (adin1200_phy_Device_t *hDevice, adi_phy_AutoMDIXConfig_e autoMdixConfig);

static adi_eth_Result_e         PHY_ConfigEnergyDetectPWD   (adin1200_phy_Device_t *hDevice, adi_Energy_Detect_PWD_e EnergyDetectPWD);
static adi_eth_Result_e         PHY_GetEDetectPWDStatus     (adin1200_phy_Device_t *hDevice, adi_Energy_Detect_PWD_Status_e *status);

static adi_eth_Result_e         PHY_ConfigDownspeed         (adin1200_phy_Device_t *hDevice, adi_phy_downspeed_e downspeedConfig);
static adi_eth_Result_e         PHY_SetDownspeedRetries     (adin1200_phy_Device_t *hDevice, uint16_t downspeedRetries);

static adi_eth_Result_e         PHY_ForceSpeed              (adin1200_phy_Device_t *hDevice, adi_force_speed_e forceSpeed);

static adi_eth_Result_e         PHY_EnableEEE               (adin1200_phy_Device_t *hDevice, adi_phy_eee_en_e enable);
static adi_eth_Result_e         PHY_GetEEE                  (adin1200_phy_Device_t *hDevice, adi_phy_eee_en_e *status);

static adi_eth_Result_e         PHY_SelectMACInterface      (adin1200_phy_Device_t *hDevice, adi_MAC_Interface_e SelMACInterface);
static adi_eth_Result_e         PHY_EnableRGMIITxRx         (adin1200_phy_Device_t *hDevice, adi_MAC_RGMII_config_e MACRGMIIConfig);
static adi_eth_Result_e         PHY_ResetRMIIFIFO           (adin1200_phy_Device_t *hDevice);

static adi_eth_Result_e         PHY_GetMasterSlaveStatus    (adin1200_phy_Device_t *hDevice, adi_Master_Slave_Status_e *status);
static adi_eth_Result_e         PHY_MasterSlaveConfig       (adin1200_phy_Device_t *hDevice, adi_phy_AdvMasterSlaveCfg_e masterSlaveConfig);

static adi_eth_Result_e         PHY_GeClk25En               (adin1200_phy_Device_t *hDevice);
static adi_eth_Result_e         PHY_GeClkHrtFreeEn          (adin1200_phy_Device_t *hDevice);
static adi_eth_Result_e         PHY_GeClkRCVREn             (adin1200_phy_Device_t *hDevice);
static adi_eth_Result_e         PHY_GeClkRefEn              (adin1200_phy_Device_t *hDevice);
static adi_eth_Result_e         PHY_GeClkFree125En          (adin1200_phy_Device_t *hDevice);
static adi_eth_Result_e         PHY_GeClkRCVR125En          (adin1200_phy_Device_t *hDevice);
static adi_eth_Result_e         PHY_GeClkDisable            (adin1200_phy_Device_t *hDevice);

static adi_eth_Result_e         PHY_Reset                   (adin1200_phy_Device_t *hDevice, adin1200_phy_ResetType_e resetType);

static adi_eth_Result_e         PHY_Write                   (adin1200_phy_Device_t *hDevice, uint32_t regAddr, uint16_t data);
static adi_eth_Result_e         PHY_Read                    (adin1200_phy_Device_t *hDevice, uint32_t regAddr, uint16_t *data);

static adi_eth_Result_e         PHY_LedEn                   (adin1200_phy_Device_t *hDevice, adi_phy_LedPort_e ledSel, bool enable);
/* Forward declarations. */
static adi_eth_Result_e         checkIdentity               (adin1200_phy_Device_t *hDevice, uint32_t *modelNum, uint32_t *revNum);
static adi_eth_Result_e         waitMdio                    (adin1200_phy_Device_t *hDevice);
static adi_eth_Result_e         phyInit                     (adin1200_phy_Device_t *hDevice);
static void                     irqCb                       (void *pCBParam, uint32_t Event, void *pArg);
static adi_eth_Result_e         setSoftwarePowerdown        (adin1200_phy_Device_t *hDevice, bool enable);
static adi_eth_Result_e         phyDiscovery                (adin1200_phy_Device_t *hDevice);

/*!
 * @brief           PHY device initialization.
 *
 * @param [in]      phDevice     PHY Device handle
 * @param [in]      cfg          PHY driver configuration
 * @param [in]      adinDevice   PHY device instance
 * @param [in]      readFn       PHY read handle.
 * @param [in]      writeFn      PHY write handle.
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *                  - #ADI_ETH_INVALID_PARAM        Configured memory size too small.
 *                  - #ADI_ETH_COMM_ERROR           MDIO communication failure.
 *                  - #ADI_ETH_UNSUPPORTED_DEVICE   Device not supported.
 *
 * @details         This function initialises the PHY device
 *
 * @sa
 */
static adi_eth_Result_e PHY_Init(adin1200_phy_Device_t **phDevice, adi_phy_DriverConfig_t *cfg, void *adinDevice, HAL_ReadFn_t readFn, HAL_WriteFn_t writeFn)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    adin1200_phy_Device_t    *hDevice;

    if(cfg->devMemSize < sizeof(adin1200_phy_Device_t))
    {
        return ADI_ETH_INVALID_PARAM;
    }

    /* Implies state is uninitialized */
    memset(cfg->pDevMem, 0, cfg->devMemSize);

    *phDevice = (adin1200_phy_Device_t *)cfg->pDevMem;
    hDevice = *phDevice;
    hDevice->phyAddr = cfg->addr;
    hDevice->irqPending = false;

    hDevice->readFn = readFn;
    hDevice->writeFn = writeFn;

    /* Reset callback settings */
    hDevice->cbFunc = NULL;
    hDevice->cbEvents = 0;

    hDevice->adinDevice = adinDevice;

    /* Disable IRQ whether the interrupt is enabled or not */
    ADI_HAL_DISABLE_IRQ(hDevice->adinDevice);

    /* Only required if the driver is configured to use the PHY interrupt */
    if(cfg->enableIrq)
    {
        ADI_HAL_REGISTER_CALLBACK(hDevice->adinDevice, (HAL_Callback_t const *)(irqCb), hDevice );
    }

    result = phyInit(hDevice);
    if(result != ADI_ETH_SUCCESS)
    {
        hDevice->state = ADI_PHY_STATE_ERROR;
        goto end;
    }

    hDevice->state = ADI_PHY_STATE_OPERATION;
    if(cfg->enableIrq)
    {
        /* Enable IRQ */
        ADI_HAL_ENABLE_IRQ(hDevice->adinDevice);

        /* We may have a pending IRQ that will be services as soon as the IRQ is enabled, */
        /* set pending IRQ to false and if needed, service it here. */
        hDevice->irqPending = false;
    }

    /* Discover which PHY is connected to the MDIO lines*/
    phyDiscovery(hDevice);

end:
    return result;
}

/*!
 * @brief           PHY device uninitialization.
 *
 * @param [in]      hDevice     PHY Device handle
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS               Call completed successfully.
 *                  - #ADI_PHY_STATE_UNINITIALIZED   Configured memory size too small.
 *                  - #ADI_ETH_INVALID_HANDLE        Invalid device handle.
 *
 * @details         This function Un-initialises the PHY device
 *
 * @sa
 */
static adi_eth_Result_e PHY_UnInit(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if(hDevice == NULL)
    {
        result = ADI_ETH_INVALID_HANDLE;
        goto end;
    }

    ADI_HAL_DISABLE_IRQ(hDevice->adinDevice);

    hDevice->state = ADI_PHY_STATE_UNINITIALIZED;

end:
    return result;
}

/*!
 * @brief           PHY device Re-initialization.
 *
 * @param [in]      hDevice     PHY Device handle
 *
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *                  - #ADI_ETH_INVALID_PARAM        Configured memory size too small.
 *                  - #ADI_ETH_COMM_ERROR           MDIO communication failure.
 *                  - #ADI_ETH_UNSUPPORTED_DEVICE   Device not supported.
 *
 * @details         This function Re-initialises the PHY device
 *
 * @sa
 */
static adi_eth_Result_e PHY_ReInitPhy(adin1200_phy_Device_t *hDevice)
{
    return phyInit(hDevice);
}


/*!
 * @brief           Local function called by PHY initialization APIs.
 *
 * @param [in]      hDevice     Device driver handle.
 * @return          Status
 *                  - #ADI_ETH_SUCCESS              Call completed successfully.
 *                  - #ADI_ETH_INVALID_PARAM        Configured memory size too small.
 *                  - #ADI_ETH_COMM_ERROR           MDIO communication failure.
 *                  - #ADI_ETH_UNSUPPORTED_DEVICE   Device not supported.
 *
 * @details         Local function called to Initialise the PHY
 *
 * @sa
 */
static adi_eth_Result_e phyInit(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;
    uint32_t            irqMask;
    uint32_t            modelNum;
    uint32_t            revNum;

    hDevice->state = ADI_PHY_STATE_UNINITIALIZED;

    /* Wait until MDIO interface is up, in case this function is called during the powerup sequence of the PHY. */
    result = waitMdio(hDevice);
    if(result != ADI_ETH_SUCCESS)
    {
        hDevice->state = ADI_PHY_STATE_ERROR;
        goto end;
    }

    /* Checks the identity of the device based on reading of hardware ID registers */
    /* Ensures the device is supported by the driver, otherwise an error is reported. */
    result = checkIdentity(hDevice, &modelNum, &revNum);
    if(result != ADI_ETH_SUCCESS)
    {
        hDevice->state = ADI_PHY_STATE_ERROR;
        goto end;
    }

    /* Values of event enums are identical to respective interrupt masks */
    /* HW_IRQ_EN and LNK_STAT_CHNG_IRQ_EN  are enabled   */
    irqMask = ADIN1200_BITM_IRQ_MASK_HW_IRQ_EN | ADIN1200_BITM_IRQ_MASK_LNK_STAT_CHNG_IRQ_EN | hDevice->cbEvents;
    result = PHY_Write(hDevice, ADIN1200_ADDR_IRQ_MASK, (irqMask & 0xFFFF));
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    /* Read IRQ status bits to clear them before enabling IRQ. */
    result = PHY_Read(hDevice, ADIN1200_ADDR_IRQ_STATUS, &val16);
    if (result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
    if (val16 & ADIN1200_BITM_IRQ_STATUS_IRQ_PENDING)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    hDevice->irqPending = false;
end:
    return result;
}


/*!
 * @brief           Get Device Id
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      pDevId      Gets PHY device Id
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_COMM_ERROR
 *                  - #ADI_ETH_UNSUPPORTED_DEVICE.
 *
 * @details         Get the PHY device Id (PHY_ID_1, OUI, Model_Num, Rev_Num)
 *
 * @sa
 */
static adi_eth_Result_e PHY_GetDeviceId(adin1200_phy_Device_t *hDevice, adinPhy_DeviceId_t *pDevId)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }
    result = PHY_Read(hDevice, ADIN1200_ADDR_PHY_ID_1, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    pDevId->phyId = (val16 << ADIN1200_BITL_PHY_ID_1_PHY_ID_1);
    result = PHY_Read(hDevice, ADIN1200_ADDR_PHY_ID_2, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    pDevId->phyId |= val16;

    /*Check if the value of PHY_ID_2.OUI matches expected value */
    if((val16 & ADIN1200_BITM_PHY_ID_2_PHY_ID_2_OUI) != (ADI_PHY_DEV_ID2_OUI << ADIN1200_BITP_PHY_ID_2_PHY_ID_2_OUI))
    {
        result = ADI_ETH_UNSUPPORTED_DEVICE;
        goto end;
    }
end:
    return result;
}

/**************************************************/
/***                                            ***/
/***             Interrupt Handling             ***/
/***                                            ***/
/**************************************************/

/*!
 * @brief           PHY register Callback.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      cbFunc      call back functions
 * @param [in]      cbEvents    Events for which interrupt occurs
 * @param [in]      cbParam     Parameters
 *
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         This function registers the callback functions for the interrupts
 *
 * @sa
 */
static adi_eth_Result_e PHY_RegisterCallback(adin1200_phy_Device_t *hDevice, adi_eth_Callback_t cbFunc, uint32_t cbEvents, void *cbParam)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    hDevice->cbFunc = cbFunc;
    hDevice->cbEvents = cbEvents;
    hDevice->cbParam = cbParam;

    /* Values of event enums are identical to respective interrupt masks */
    /* Hardware reset and hardware error interrupts are always enabled   */
    uint32_t irqMask =  ADIN1200_BITM_IRQ_MASK_HW_IRQ_EN | ADIN1200_BITM_IRQ_MASK_LNK_STAT_CHNG_IRQ_EN | hDevice->cbEvents;

    result = PHY_Write(hDevice, ADIN1200_ADDR_IRQ_MASK, irqMask);
    if (result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

end:
    return result;
}

/*!
 * @brief           PHY interrupt callback.
 *
 * @param [in]      pCBParam    Parameters .
 * @param [in]      Event       Events for which interrupt occurs
 * @param [in]      pArg        Events for which interrupt occurs
 *
 *
 * @details        This is a interrupt callback function
 *
 * @sa
 */
static void irqCb(void *pCBParam, uint32_t Event, void *pArg)
{
    adin1200_phy_Device_t    *hDevice = (adin1200_phy_Device_t *)pCBParam;

    (void)Event;
    (void)pArg;

    /* Set this flag to ensure the IRQ is handled before other actions are taken */
    /* The interrupt may be triggered by a hardware reset, in which case the PHY */
    /* will likely need to be reconfigured.                                      */
    /* The flag is cleared when interrupt status registers are read. Taking      */
    /* appropriate action is the responsibility of the caller.                   */
    hDevice->irqPending = true;
    if(hDevice->cbFunc != NULL)
    {
        hDevice->cbFunc(hDevice->cbParam, 0, NULL);
    }
}

/*!
 * @brief           Read interrupt status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      Interrupt register value
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Read the interrupt status register
 *
 * @sa
 */
static adi_eth_Result_e PHY_ReadIrqStatus(adin1200_phy_Device_t *hDevice, uint32_t *status)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;

    result = PHY_Read(hDevice, ADIN1200_ADDR_IRQ_STATUS, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
    else
    {
        *status = val16;
        hDevice->irqPending = false;
    }

end:
    return result;
}

/**************************************************/
/***                                            ***/
/***          Software Powerdown                ***/
/***                                            ***/
/**************************************************/

/*!
 * @brief           Set Software Powerdown
 *
 * @param [in]      hDevice    Device driver handle
 * @param [in]      enable     Enable/Disable Software Power down
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Enable or Disable the Software power down
 *
 * @sa
 */
static adi_eth_Result_e setSoftwarePowerdown(adin1200_phy_Device_t *hDevice, bool enable)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;
    bool                swpd;
    int32_t             iter = ADI_PHY_SOFT_PD_ITER;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_MII_CONTROL, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        return ADI_ETH_COMM_ERROR;
    }

    if(enable)
        val16 |= ADIN1200_BITM_MII_CONTROL_SFT_PD;
    else
        val16 &= ~ADIN1200_BITM_MII_CONTROL_SFT_PD;
    result = PHY_Write(hDevice, ADIN1200_ADDR_MII_CONTROL, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    /* Wait for the PHY device to enter the desired state before returning. */
    do
    {
        result = PHY_GetSoftwarePowerdown(hDevice, &swpd);
        if(swpd == enable) break;
    } while(--iter);

    if(iter <= 0)
    {
        result = ADI_ETH_READ_STATUS_TIMEOUT;
        goto end;
    }

    if(enable)
    {
        hDevice->state = ADI_PHY_STATE_SOFTWARE_POWERDOWN;
    }
    else
    {
        hDevice->state = ADI_PHY_STATE_OPERATION;
    }

end:
    return result;
}

/*!
 * @brief           Enter/exit software powerdown.
 *
 * @param [in]      hDevice    Device driver handle.
 * @param [in]      swpd       Enter/Exit software powerdown
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Enable or Disable the Software power down
 *
 * @sa
 */
static adi_eth_Result_e PHY_SoftwarePowerdown(adin1200_phy_Device_t *hDevice, adi_phy_swpd_e swpd)
{
    if(swpd)
        return setSoftwarePowerdown(hDevice, true);
    else
        return setSoftwarePowerdown(hDevice, false);
}

/*!
 * @brief           Get Software powerdown status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      SWPD Status
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Get SWPD status
 *
 * @sa
 */
static adi_eth_Result_e PHY_GetSoftwarePowerdown(adin1200_phy_Device_t *hDevice, bool *status)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    val16 = 0;
    result = PHY_Read(hDevice, ADIN1200_ADDR_MII_CONTROL, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
    if(val16 & ADIN1200_BITM_MII_CONTROL_SFT_PD)
        *status = true;
    else
        *status = false;
end:
    return result;
}

/**************************************************/
/***                                            ***/
/***                PHY Link                    ***/
/***                                            ***/
/**************************************************/

/*!
 * @brief           Get link status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      Link status
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         This function gets the Link Status
 *
 * @sa
 */
static adi_eth_Result_e PHY_GetLinkStatus(adin1200_phy_Device_t *hDevice, adi_phy_LinkStatus_e *status)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_MII_STATUS, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_MII_STATUS, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    if(val16 & ADIN1200_BITM_MII_STATUS_LINK_STAT_LAT)
        *status = ADI_PHY_LINK_STATUS_UP;
    else *status = ADI_PHY_LINK_STATUS_DOWN;

end:
    return result;
}

/*!
 * @brief           Config Link
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      enable      Enable/Disable Link
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Enables or Disables the Link
 *
 * @sa
 */
static adi_eth_Result_e PHY_LinkConfig(adin1200_phy_Device_t *hDevice, bool enable)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_PHY_CTRL_3, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    if(enable)
        val16 |= ADIN1200_BITM_PHY_CTRL_3_LINK_EN;
    else
        val16 &= ~ADIN1200_BITM_PHY_CTRL_3_LINK_EN;
    result = PHY_Write(hDevice, ADIN1200_ADDR_PHY_CTRL_3, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
    return result;
}

/**************************************************/
/***                                            ***/
/***                  Autoneg                   ***/
/***                                            ***/
/**************************************************/
/*!
 * @brief           Enable/disable auto-negotiation.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      enable      Enable/disable autoneg
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         It is STRONGLY recommended to never disable auto-negotiation!
 *
 * @sa
 */
static adi_eth_Result_e PHY_AnegEnable(adin1200_phy_Device_t *hDevice, adi_phy_AnegEn_e enable)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_MII_CONTROL, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    /* Set the Autonegotiation */
    if(enable)
      val16 |= ADIN1200_BITM_MII_CONTROL_AUTONEG_EN;
    else
      val16 &= ~ADIN1200_BITM_MII_CONTROL_AUTONEG_EN;
    result = PHY_Write(hDevice, ADIN1200_ADDR_MII_CONTROL, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

end:
    return result;
}

/*!
 * @brief           Restart auto-negotiation.
 *
 * @param [in]      hDevice     Device driver handle.
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details        This bit is self clearing. When the autonegotiation
 *                 process is restarted, this bit returns to 1'b0
 *
 * @sa
 */
static adi_eth_Result_e PHY_AnegRestart(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_MII_CONTROL, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
    /* ANEG_ENABLE should be 1 or ANEG_RESTART is ignored. Instead of read-modify-write */
    /* and/or reporting an error, we force ANEG_ENABLE=1.                             */
    val16 |= (1 << ADIN1200_BITP_MII_CONTROL_AUTONEG_EN) | (1 << ADIN1200_BITP_MII_CONTROL_RESTART_ANEG);
    result = PHY_Write(hDevice, ADIN1200_ADDR_MII_CONTROL, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

end:
    return result;
}

/*!
 * @brief           Get auto-negotiation status.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      AutoNeg Status
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         This function gets the Autonegotiation status.
 *
 * @sa
 */
static adi_eth_Result_e PHY_GetAnStatus(adin1200_phy_Device_t *hDevice, adi_phy_AnegStatus_e *status)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_MII_STATUS, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    if(val16 & ADIN1200_BITM_MII_STATUS_AUTONEG_DONE)
        *status = ADI_PHY_ANEG_COMPLETED;
    else
        *status = ADI_PHY_ANEG_NOT_COMPLETED;

end:
    return result;
}

/*!
 * @brief           Config Aneg Advertise Register
 *
 * @param [in]      hDevice          Device driver handle.
 * @param [in]      anegAdvertise    Aneg Advertise Speed
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 *
 * @details         This function sets the bits to advertise
 *                  -  1000BASE-T full duplex (only for ADINPHY 1G)
 *                  -  1000BASE-T half duplex (only for ADINPHY 1G)
 *                  -  100BASE-TX full duplex.
 *                  -  100BASE-TX half duplex.
 *                  -  10BASE-TX full duplex.
 *                  -  10BASE-TX half duplex.
 *
 * @sa
 */
static adi_eth_Result_e PHY_AnegAdverConfig(adin1200_phy_Device_t *hDevice, adi_phy_AnegAdv_e anegAdvertise)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    if(anegAdvertise >= ADI_PHY_ANEG_FD_1000_ADV_EN)
    {
        if(hDevice->mdioPhydevice == ADI_ADIN1200_DEVICE)
        {
            result = ADI_ETH_INVALID_PARAM;
            goto end;
        }
        result = PHY_Read(hDevice, ADIN1200_ADDR_MSTR_SLV_CONTROL, &val16);
        if (result != ADI_ETH_SUCCESS)
        {
            result = ADI_ETH_COMM_ERROR;
            goto end;
        }
    }
    else
    {
        result = PHY_Read(hDevice, ADIN1200_ADDR_AUTONEG_ADV, &val16);
        if(result != ADI_ETH_SUCCESS)
        {
            result = ADI_ETH_COMM_ERROR;
            goto end;
         }
    }

    switch(anegAdvertise)
    {
        case ADI_PHY_ANEG_HD_10_ADV_EN:
            val16 |= ADIN1200_BITM_AUTONEG_ADV_HD_10_ADV;
        break;

        case ADI_PHY_ANEG_HD_10_ADV_DIS:
            val16 &= ~(ADIN1200_BITM_AUTONEG_ADV_HD_10_ADV);
        break;

        case ADI_PHY_ANEG_FD_10_ADV_EN:
            val16 |= ADIN1200_BITM_AUTONEG_ADV_FD_10_ADV;
        break;

        case ADI_PHY_ANEG_FD_10_ADV_DIS:
            val16 &= ~(ADIN1200_BITM_AUTONEG_ADV_FD_10_ADV);
        break;

        case ADI_PHY_ANEG_HD_100_ADV_EN:
            val16 |= ADIN1200_BITM_AUTONEG_ADV_HD_100_ADV;
        break;

        case ADI_PHY_ANEG_HD_100_ADV_DIS:
            val16 &= ~(ADIN1200_BITM_AUTONEG_ADV_HD_100_ADV);
        break;

        case ADI_PHY_ANEG_FD_100_ADV_EN:
            val16 |= ADIN1200_BITM_AUTONEG_ADV_FD_100_ADV;
        break;

        case ADI_PHY_ANEG_FD_100_ADV_DIS:
            val16 &= ~(ADIN1200_BITM_AUTONEG_ADV_FD_100_ADV);
        break;

        case ADI_PHY_ANEG_FD_1000_ADV_EN:
            val16 |= ADIN1200_BITM_MSTR_SLV_CONTROL_FD_1000_ADV;
        break;

        case ADI_PHY_ANEG_FD_1000_ADV_DIS:
            val16 &= ~(ADIN1200_BITM_MSTR_SLV_CONTROL_FD_1000_ADV);
        break;

        case ADI_PHY_ANEG_HD_1000_ADV_EN:
            val16 |= ADIN1200_BITM_MSTR_SLV_CONTROL_HD_1000_ADV;
        break;

        case ADI_PHY_ANEG_HD_1000_ADV_DIS:
            val16 &= ~(ADIN1200_BITM_MSTR_SLV_CONTROL_HD_1000_ADV);
        break;
        default:
            result = ADI_ETH_INVALID_PARAM;
            goto end;
        break;
    }

    if(anegAdvertise >= ADI_PHY_ANEG_FD_1000_ADV_EN)
    {
        result = PHY_Write(hDevice, ADIN1200_ADDR_MSTR_SLV_CONTROL, val16);
        if(result != ADI_ETH_SUCCESS)
        {
            result = ADI_ETH_COMM_ERROR;
            goto end;
        }
    }
    else
    {
        result = PHY_Write(hDevice, ADIN1200_ADDR_AUTONEG_ADV, val16);
        if(result != ADI_ETH_SUCCESS)
        {
            result = ADI_ETH_COMM_ERROR;
            goto end;
        }
    }
end:
    return result;
}

/*!
 * @brief           Aneg Advertise speed
 *
 * @param [in]      hDevice      Device driver handle.
 * @param [out]     phy_speed    Set advertise speed for 10M and 100M
 * @param [out]     phy_speed1G  Set advertise speed for 1G
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         This function return the advertised PHY speeds
 *
 * @sa
 */
static adi_eth_Result_e PHY_AnegAdvertised(adin1200_phy_Device_t *hDevice, uint16_t *phy_speed, uint16_t *phy_speed1G)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_AUTONEG_ADV, phy_speed);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
    if(hDevice->mdioPhydevice == ADI_ADIN1300_DEVICE)
    {
        result = PHY_Read(hDevice, ADIN1200_ADDR_MSTR_SLV_CONTROL, phy_speed1G);
        if(result != ADI_ETH_SUCCESS)
        {
            result = ADI_ETH_COMM_ERROR;
            goto end;
        }
    }
    else
        *phy_speed1G = 0;

end:
    return result;
}

/**************************************************/
/***                                            ***/
/***              Auto MDIX Config              ***/
/***                                            ***/
/**************************************************/
/*!
 * @brief           Auto MDIX Config
 *
 * @param [in]      hDevice    Device driver handle
 * @param [in]      autoMdixConfig MDIX config
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *                  - #ADI_ETH_INVALID_PARAM.
 *
 * @details         Sets the Auto MDIX Config to
 *                  - Mode 1 - Manual MDI
 *                  - Mode 2 - Manual MDIX
 *                  - Mode 3 - Auto MDIX, Prefer MDIX
 *                  - Mode 4 - Auto MDIX, Prefer MDI
 *
 * @sa
 */

static adi_eth_Result_e PHY_AutoMDIXConfig(adin1200_phy_Device_t *hDevice, adi_phy_AutoMDIXConfig_e autoMdixConfig)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_PHY_CTRL_1, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    switch(autoMdixConfig)
    {
        case ADI_PHY_MANUAL_MDI: //Mode 1 - Fixed MDI
            /* MAN_MDIX is clear and AUTO_MDI_EN is clear*/
            val16 &= ~ (ADIN1200_BITM_PHY_CTRL_1_MAN_MDIX | ADIN1200_BITM_PHY_CTRL_1_AUTO_MDI_EN);
            break;

        case ADI_PHY_MANUAL_MDIX: //Mode 2 - Fixed MDIX
            /* When MAN_MDIX bit is set and the AUTO_MDI_EN bit is clear, the PHY operates in the MDIX configuration. */
            val16 &= ~(ADIN1200_BITM_PHY_CTRL_1_AUTO_MDI_EN);
            val16 |= (1 << ADIN1200_BITP_PHY_CTRL_1_MAN_MDIX);
            break;

        case ADI_PHY_AUTO_MDIX_PREFER_MDIX: //Mode 3 - Enable Auto
            /* When AUTO_MDI_EN bit is set and MAN_MDIX bit is set */
            val16 = (ADIN1200_BITM_PHY_CTRL_1_MAN_MDIX | ADIN1200_BITM_PHY_CTRL_1_AUTO_MDI_EN);
            break;

        case ADI_PHY_AUTO_MDIX_PREFER_MDI: //Mode 4 - Enable Auto
            /* When AUTO_MDI_EN bit is set and MAN_MDIX bit is clear */
            val16 &= ~(ADIN1200_BITM_PHY_CTRL_1_MAN_MDIX);
            val16 |= (1 << ADIN1200_BITP_PHY_CTRL_1_AUTO_MDI_EN);
            break;
        default:
            result = ADI_ETH_INVALID_PARAM;
            goto end;
        break;
    }
    result = PHY_Write(hDevice, ADIN1200_ADDR_PHY_CTRL_1, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
    return result;
}

/**************************************************/
/***                                            ***/
/***      Energy Detect Power Down Mode         ***/
/***                                            ***/
/**************************************************/

/*!
 * @brief           Energy Detect Power Down mode Config
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      EnergyDetectPWD Energy power down mode
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *                  - ADI_ETH_INVALID_PARAM
 *
 * @details         Energy Detect Power Down mode as follows
 *                  - NRG_PD_EN
 *                      - 1: enable energy detect power-down mode.
 *                      - 0: disable energy detect power-down mode
 *                  - NRG_PD_TX_EN
 *                      - 1: enable periodic transmission of the pulse while in energy detect power-down mode.
 *                      - 0: disable periodic transmission of the pulse while in energy detect power-down mode.
 *
 * @sa
 */
static adi_eth_Result_e  PHY_ConfigEnergyDetectPWD(adin1200_phy_Device_t *hDevice, adi_Energy_Detect_PWD_e EnergyDetectPWD)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_PHY_CTRL_STATUS_2, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    switch(EnergyDetectPWD)
    {
        case ADI_NRG_PD_EN:
            val16 |= ADIN1200_BITM_PHY_CTRL_STATUS_2_NRG_PD_EN;
        break;

        case ADI_NRG_PD_DIS:
            val16 &= ~ADIN1200_BITM_PHY_CTRL_STATUS_2_NRG_PD_EN;
        break;

        case ADI_NRG_PD_TX_DIS:
            val16 |=  ADIN1200_BITM_PHY_CTRL_STATUS_2_NRG_PD_EN;
            val16 &= ~ADIN1200_BITM_PHY_CTRL_STATUS_2_NRG_PD_TX_EN;
        break;

        case ADI_NRG_PD_TX_EN:
            val16 |= (ADIN1200_BITM_PHY_CTRL_STATUS_2_NRG_PD_EN | ADIN1200_BITM_PHY_CTRL_STATUS_2_NRG_PD_TX_EN);
        break;
        default:
            result = ADI_ETH_INVALID_PARAM;
            goto end;
        break;
    }

    result = PHY_Write(hDevice, ADIN1200_ADDR_PHY_CTRL_STATUS_2, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
    return result;
}

/*!
 * @brief           Get Energy Detect Power Down mode Status
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      Energy Detect Status
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Energy Detect Power Down mode
 *                  - PHY_IN_NRG_PD
 *                     - 1: PHY is in energy detect power-down mode.
 *                     - 0: PHY is not in energy detect power-down mode
 *
 * @sa
 */
static adi_eth_Result_e  PHY_GetEDetectPWDStatus(adin1200_phy_Device_t *hDevice, adi_Energy_Detect_PWD_Status_e *status)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_PHY_CTRL_STATUS_2, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    if(ADIN1200_BITM_PHY_CTRL_STATUS_2_PHY_IN_NRG_PD & val16)
        *status = ADI_PHY_IN_NRG_PD_MODE;
    else
        *status = ADI_PHY_NOT_IN_NRG_PD_MODE;
end:
    return result;
}

/**************************************************/
/***                                            ***/
/***          Downspeed config                  ***/
/***                                            ***/
/**************************************************/

/*!
 * @brief           Downspeed config
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      downspeed      Enable/disable downspeed
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Config Downspeed
 *                  - DN_SPEED_TO_10_EN
 *                      - 1: downspeed downspeed to 10BASE-T -Note that autonegotiation must also be enabled
 *                      - 0: disable downspeed to 10BASE-T.
 *                  - DN_SPEED_TO_100_EN (only for ADIN PHY 1G)
 *                      - 1: downspeed downspeed to 100BASE-TX -Note that autonegotiation must also be enabled
 *                      - 0: disable downspeed to 100BASE-TX.
 *
 * @sa
 */
static adi_eth_Result_e PHY_ConfigDownspeed(adin1200_phy_Device_t *hDevice, adi_phy_downspeed_e downspeed)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    /* The EEE_1000_ADV Advertise is only for ADINPHY 1G devices */
    if( (hDevice->mdioPhydevice == ADI_ADIN1200_DEVICE) & (downspeed >= ADI_PHY_DOWNSPEED_T0_100_DIS) )
    {
        result = ADI_ETH_INVALID_PARAM;
        goto end;
    }

    /* Set the Autonegotiation */
    result = PHY_AnegEnable(hDevice, ADI_PHY_ANEG_EN);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
    result = PHY_Read(hDevice, ADIN1200_ADDR_PHY_CTRL_2, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    switch(downspeed)
    {
        case ADI_PHY_DOWNSPEED_T0_10_DIS:
            val16 &= ~ADIN1200_BITM_PHY_CTRL_2_DN_SPEED_TO_10_EN;
        break;
        case ADI_PHY_DOWNSPEED_TO_10_EN:
            val16 |= ADIN1200_BITM_PHY_CTRL_2_DN_SPEED_TO_10_EN;
        break;
        case ADI_PHY_DOWNSPEED_T0_100_DIS:
            val16 &= ~ADIN1200_BITM_PHY_CTRL_2_DN_SPEED_TO_100_EN;
        break;
        case ADI_PHY_DOWNSPEED_TO_100_EN:
            val16 |= ADIN1200_BITM_PHY_CTRL_2_DN_SPEED_TO_100_EN;
        break;
        default:
            result = ADI_ETH_INVALID_PARAM;
            goto end;
        break;
    }
    result = PHY_Write(hDevice,ADIN1200_ADDR_PHY_CTRL_2, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

end:
    return result;
}

/*!
 * @brief           Downspeed - Number of retries
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      downspeedRetries Number of Retries
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details        If downspeed is enabled, this register bit specifies the
 *                 number of retries the PHY must attempt to bring up a link at
 *                 the advertised speed before advertising a lower speed
 *
 * @sa
 */
static adi_eth_Result_e PHY_SetDownspeedRetries(adin1200_phy_Device_t *hDevice, uint16_t downspeedRetries)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    /* Enable downspeed in the PHY_CTRL_2 register before setting the retries */
    result = PHY_Read(hDevice, ADIN1200_ADDR_PHY_CTRL_3, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    val16 |= (downspeedRetries << ADIN1200_BITP_PHY_CTRL_3_NUM_SPEED_RETRY);
    result = PHY_Write(hDevice, ADIN1200_ADDR_PHY_CTRL_3, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

end:
    return result;
}

/**************************************************/
/***                                            ***/
/***              Force Speed                   ***/
/***                                            ***/
/**************************************************/
/*!
 * @brief           Set Force Speed
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      forceSpeed  Set force speed
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *                  - #ADI_ETH_INVALID_PARAM.
 *
 * @details        Set the speed to one of the following
 *                  - 10BASE_T HD.
 *                  - 10BASE_T FD.
 *                  - 100BASE_TX HD.
 *                  - 100BASE_TX FD.
 * @sa
 */
static adi_eth_Result_e PHY_ForceSpeed(adin1200_phy_Device_t *hDevice, adi_force_speed_e forceSpeed)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_MII_CONTROL, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
          result = ADI_ETH_COMM_ERROR;
          goto end;
    }

    switch(forceSpeed)
    {
        case ADI_SPEED_10BASE_T_HD:
            val16 &= ( ~(ADIN1200_BITM_MII_CONTROL_SPEED_SEL_LSB) & ~(ADIN1200_BITM_MII_CONTROL_SPEED_SEL_MSB) & ~(ADIN1200_BITM_MII_CONTROL_DPLX_MODE) );
        break;

        case ADI_SPEED_10BASE_T_FD:
            val16 &= ( ~(ADIN1200_BITM_MII_CONTROL_SPEED_SEL_LSB) & ~(ADIN1200_BITM_MII_CONTROL_SPEED_SEL_MSB));
            val16 |= ADIN1200_BITM_MII_CONTROL_DPLX_MODE;
        break;

        case ADI_SPEED_100BASE_TX_HD:
            val16 &= ( ~(ADIN1200_BITM_MII_CONTROL_SPEED_SEL_MSB) & ~(ADIN1200_BITM_MII_CONTROL_DPLX_MODE) );
            val16 |= ADIN1200_BITM_MII_CONTROL_SPEED_SEL_LSB;
        break;

        case ADI_SPEED_100BASE_TX_FD:
            val16 &= ~(ADIN1200_BITM_MII_CONTROL_SPEED_SEL_MSB);
            val16 |= (ADIN1200_BITM_MII_CONTROL_DPLX_MODE | ADIN1200_BITM_MII_CONTROL_SPEED_SEL_LSB);
        break;
        default:
            result = ADI_ETH_INVALID_PARAM;
            goto end;
        break;
    }

    result = PHY_Write(hDevice, ADIN1200_ADDR_MII_CONTROL, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
    return result;
}



/**************************************************/
/***                                            ***/
/***          Energy Efficient Ethernet         ***/
/***                                            ***/
/**************************************************/

/*!
 * @brief           Energy Efficient Ethernet
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      enable      Enable/Disable EEE
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *                  - #ADI_ETH_INVALID_PARAM.
 *
 * @details         Energy Efficient Ethernet
 *                  - EEE_100_ADV
 *                      - 1: advertise that the 100BASE-TX has EEE capability
 *                      - 0: do not advertise that the 100BASE-TX has EEE capability.
 *                  - EEE_1000_ADV
 *                      - 1: advertise that the 1000BASE-T has EEE capability
 *                      - 0: do not advertise that the 1000BASE-T has EEE capability.
 *
 * @sa
 */
static adi_eth_Result_e PHY_EnableEEE(adin1200_phy_Device_t *hDevice, adi_phy_eee_en_e adv_eee)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    /* The EEE_1000_ADV Advertise is only for ADINPHY 1G devices */
    if( (hDevice->mdioPhydevice == ADI_ADIN1200_DEVICE) & (adv_eee >= ADI_PHY_EEE_1000_ADV_DIS) )
    {
        result = ADI_ETH_INVALID_PARAM;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_EEE_ADV, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    switch(adv_eee)
    {
        case ADI_PHY_EEE_100_ADV_DIS:
            val16 &= ~(ADIN1200_BITM_EEE_ADV_EEE_100_ADV);
        break;
        case ADI_PHY_EEE_100_ADV_EN:
            val16 |=  ADIN1200_BITM_EEE_ADV_EEE_100_ADV;
        break;
        case ADI_PHY_EEE_1000_ADV_DIS:
            val16 &= ~(ADIN1200_BITM_EEE_ADV_EEE_1000_ADV);
        break;
        case ADI_PHY_EEE_1000_ADV_EN:
            val16 |=  ADIN1200_BITM_EEE_ADV_EEE_1000_ADV;
        break;
        default:
            result = ADI_ETH_INVALID_PARAM;
            goto end;
        break;
    }

    result = PHY_Write(hDevice, ADIN1200_ADDR_EEE_ADV, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

end:
  return result;
}

/*!
 * @brief           Get Energy Efficient Ethernet Status
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     status      EEE status
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Get Energy Efficient Ethernet Status
 *
 * @sa
 */
static adi_eth_Result_e PHY_GetEEE(adin1200_phy_Device_t *hDevice, adi_phy_eee_en_e *status)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_EEE_ADV, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
    *status = (adi_phy_eee_en_e)val16;

end:
    return result;
}

/**************************************************/
/***                                            ***/
/***            MAC Interface Config            ***/
/***                                            ***/
/**************************************************/

/*!
 * @brief           MAC Interface Config
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      SelMACInterface Select MAC Interface
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *                  - ADI_ETH_INVALID_PARAM.
 *
 * @details         Sets the MAC interface
 *                  - MAC Interface Selection        MACIF_SEL1    MACIF_SEL0
 *                   - RGMII RXC/TXC 2 ns Delay        Low           Low
 *                   - RGMII RXC Only, 2 ns Delay      High          Low
 *                   - MII                             Low           High
 *                   - RMII  [no SW config]            High          High
 *
 * @sa
 */

static adi_eth_Result_e PHY_SelectMACInterface(adin1200_phy_Device_t *hDevice, adi_MAC_Interface_e SelMACInterface)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    /* Put the PHY in Software power down mode */
    setSoftwarePowerdown(hDevice, true);
    if(result != ADI_ETH_SUCCESS)
    {
         result = ADI_ETH_COMM_ERROR;
         goto end;
    }

    switch(SelMACInterface)
    {
        case ADI_RGMII_MAC_INTERFACE:
            result = PHY_Read(hDevice, ADIN1200_ADDR_GE_RGMII_CFG, &val16);
            if(result != ADI_ETH_SUCCESS)
            {
                result = ADI_ETH_COMM_ERROR;
                goto end;
            }
            /* Enable RGMII MAC interface */
            val16 |= (ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_EN | ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_TX_ID_EN);
            result = PHY_Write(hDevice, ADIN1200_ADDR_GE_RGMII_CFG, val16);
            if(result != ADI_ETH_SUCCESS)
            {
                result = ADI_ETH_COMM_ERROR;
                goto end;
            }

            result = PHY_Read(hDevice, ADIN1200_ADDR_GE_RMII_CFG, &val16);
            if(result != ADI_ETH_SUCCESS)
            {
                result = ADI_ETH_COMM_ERROR;
                goto end;
            }
            /* Disable RMII MAC Interface*/
            val16 &= (~ADIN1200_BITM_GE_RMII_CFG_GE_RMII_EN);
            result = PHY_Write(hDevice, ADIN1200_ADDR_GE_RMII_CFG, val16);
            if(result != ADI_ETH_SUCCESS)
            {
                result = ADI_ETH_COMM_ERROR;
                goto end;
            }
        break;

        case ADI_RMII_MAC_INTERFACE:
            result = PHY_Read(hDevice, ADIN1200_ADDR_GE_RMII_CFG, &val16);
            if(result != ADI_ETH_SUCCESS)
            {
                result = ADI_ETH_COMM_ERROR;
                goto end;
            }
            /* Enable RMII MAC interface */
            val16 |= ADIN1200_BITM_GE_RMII_CFG_GE_RMII_EN;
            result = PHY_Write(hDevice, ADIN1200_ADDR_GE_RMII_CFG, val16);
            if(result != ADI_ETH_SUCCESS)
            {
                result = ADI_ETH_COMM_ERROR;
                goto end;
            }

            result = PHY_Read(hDevice, ADIN1200_ADDR_GE_RGMII_CFG, &val16);
            if(result != ADI_ETH_SUCCESS)
            {
                result = ADI_ETH_COMM_ERROR;
                goto end;
            }
            /* Disable RGMII MAC interface */
            val16  &= (~(ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_EN|ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_TX_ID_EN));
            result = PHY_Write(hDevice, ADIN1200_ADDR_GE_RGMII_CFG, val16);
            if(result != ADI_ETH_SUCCESS)
            {
                result = ADI_ETH_COMM_ERROR;
                goto end;
            }
         break;

         case ADI_MII:
               result = ADI_ETH_UNSUPPORTED_FEATURE;
               goto end;
         break;
         default:
               result = ADI_ETH_INVALID_PARAM;
               goto end;
         break;
    }
end:
    return result;
}

/*!
 * @brief           RGMII Tx and Rx enable
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      enable      Enable/Disable RGMII
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *                  - #ADI_ETH_INVALID_PARAM.
 *
 * @details         Enable/disable receive clock internal 2 ns delay in RGMII mode
 *
 * @sa
 */
static adi_eth_Result_e PHY_EnableRGMIITxRx(adin1200_phy_Device_t *hDevice, adi_MAC_RGMII_config_e MACRGMIIConfig)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    /* Set SWPD */
    result = setSoftwarePowerdown(hDevice, true);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
    result = PHY_Read(hDevice, ADIN1200_ADDR_GE_RGMII_CFG, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    switch(MACRGMIIConfig)
    {
        case ADI_RGMII_TX_ID_DIS:
            val16 &= ~(ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_TX_ID_EN);
        break;

        case ADI_RGMII_TX_ID_EN:
            val16 |= ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_TX_ID_EN;
        break;

        case ADI_RGMII_RX_ID_DIS:
            val16 &= ~(ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_RX_ID_EN);
        break;

        case ADI_RGMII_RX_ID_EN:
            val16 |= ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_RX_ID_EN;
        break;

        default:
           result = ADI_ETH_INVALID_PARAM;
           goto end;
        break;
    }

    result = PHY_Write(hDevice, ADIN1200_ADDR_GE_RGMII_CFG, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
    return result;
}

/*!
 * @brief           RMII FIFO Reset
 *
 * @param [in]      hDevice     Device driver handle.
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         This bit allows the RMII FIFO to be reset.
 *
 * @sa
 */
static adi_eth_Result_e PHY_ResetRMIIFIFO(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    /* Set SWPD */
    setSoftwarePowerdown(hDevice, true);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_GE_RMII_CFG, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    val16 |= ADIN1200_BITM_GE_RMII_CFG_GE_RMII_FIFO_RST;
    result = PHY_Write(hDevice, ADIN1200_ADDR_GE_RMII_CFG, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
  return result;
}

/**************************************************/
/***                                            ***/
/***       Master Slave Configuration           ***/
/***                                            ***/
/**************************************************/
/*!
 * @brief           Get Master Slave Status
 *
 * @param [in]      hDevice    Device driver handle.
 * @param [out]      status     Master slave staus
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *                  - #ADI_ETH_NOT_IMPLEMENTED.
 *
 * @details         Get Master Slave status.
 *
 * @sa
 */
static adi_eth_Result_e  PHY_GetMasterSlaveStatus(adin1200_phy_Device_t *hDevice, adi_Master_Slave_Status_e *status)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    if(hDevice->mdioPhydevice == ADI_ADIN1200_DEVICE)
    {
        result = ADI_ETH_NOT_IMPLEMENTED;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_MSTR_SLV_STATUS, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    if(ADIN1200_BITM_MSTR_SLV_STATUS_MSTR_SLV_FLT & val16)
        *status = ADI_MAST_SLAVE_FAULT_DETECT;
    else if(( (~ADIN1200_BITM_MSTR_SLV_STATUS_MSTR_SLV_FLT) | val16) == (~ADIN1200_BITM_MSTR_SLV_STATUS_MSTR_SLV_FLT) )
    {
         if(ADIN1200_BITM_MSTR_SLV_STATUS_MSTR_RSLVD & val16)
             *status = ADI_RESOLVED_TO_MASTER;
         else
             *status = ADI_RESOLVED_TO_SLAVE;
    }
end:
    return result;
}

/*!
 * @brief           Master Slave config
 *
 * @param [in]      hDevice    Device driver handle.
 * @param [out]     masterSlaveConfig Config as Master or Slave
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *                  - #ADI_ETH_INVALID_PARAM.
 *                  - #ADI_ETH_NOT_IMPLEMENTED
 *
 * @details         Configure PHY as Master or slave only for ADINPHY 1G
 *
 * @sa
 */
static adi_eth_Result_e PHY_MasterSlaveConfig(adin1200_phy_Device_t *hDevice, adi_phy_AdvMasterSlaveCfg_e masterSlaveConfig)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if (hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    if(hDevice->mdioPhydevice == ADI_ADIN1200_DEVICE)
    {
        result = ADI_ETH_NOT_IMPLEMENTED;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_MSTR_SLV_CONTROL, &val16);
    if (result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    switch(masterSlaveConfig)
    {
        case ADI_PHY_MAN_ADV_MASTER:
            val16 |= (ADIN1200_BITM_MSTR_SLV_CONTROL_MAN_MSTR_SLV_EN_ADV | ADIN1200_BITM_MSTR_SLV_CONTROL_MAN_MSTR_ADV);
        break;
        case ADI_PHY_MAN_ADV_SLAVE:
            val16 |= ADIN1200_BITM_MSTR_SLV_CONTROL_MAN_MSTR_SLV_EN_ADV ;
            val16 &= ~(ADIN1200_BITM_MSTR_SLV_CONTROL_MAN_MSTR_ADV);
        break;
        case ADI_PHY_MAN_MSTR_SLV_DIS:
            val16 &= ~(ADIN1200_BITM_MSTR_SLV_CONTROL_MAN_MSTR_SLV_EN_ADV);
            val16 &= ~(ADIN1200_BITM_MSTR_SLV_CONTROL_MAN_MSTR_ADV);
        break;
        default:
           result = ADI_ETH_INVALID_PARAM;
           goto end;
        break;
    }

    result = PHY_Write(hDevice, ADIN1200_ADDR_MSTR_SLV_CONTROL, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
    return result;
}

/**************************************************/
/***                                            ***/
/***       Subsystem Clock Configuration        ***/
/***                                            ***/
/**************************************************/
/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle.
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Enable GECLK25
 *
 * @sa
 */
static adi_eth_Result_e PHY_GeClk25En(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_GE_CLK_CFG, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    /* Enabling only GE CLK 25 and disabling other clocks*/
    result = PHY_Write(hDevice, ADIN1200_ADDR_GE_CLK_CFG, ADIN1200_BITM_GE_CLK_CFG_GE_CLK_25_EN);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
  return result;
}

/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle.
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Enable GECLK HrtFree
 *
 * @sa
 */
static adi_eth_Result_e PHY_GeClkHrtFreeEn(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_GE_CLK_CFG, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    /* Enabling only GE CLK HRT FREE and disabling other clocks*/
    result = PHY_Write(hDevice, ADIN1200_ADDR_GE_CLK_CFG, ADIN1200_BITM_GE_CLK_CFG_GE_CLK_HRT_FREE_EN);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
  return result;
}

/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle.
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Enable GECLKRcvr
 *
 * @sa
 */
static adi_eth_Result_e PHY_GeClkRCVREn(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_GE_CLK_CFG, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    /* Enabling only GE CLK RCVR and disabling other clocks*/
    result = PHY_Write(hDevice, ADIN1200_ADDR_GE_CLK_CFG, ADIN1200_BITM_GE_CLK_CFG_GE_CLK_HRT_RCVR_EN);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
  return result;
}

/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle.
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *                  - #ADI_ETH_NOT_IMPLEMENTED.
 *
 * @details         Enable GE Reference clock, Only for ADIN PHY 1G
 *
 * @sa
 */
static adi_eth_Result_e PHY_GeClkRefEn(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    if(hDevice->mdioPhydevice == ADI_ADIN1200_DEVICE)
    {
        result = ADI_ETH_NOT_IMPLEMENTED;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_GE_CLK_CFG, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    /* Enabling only GE CLK Reference and disabling other clocks*/
    result = PHY_Write(hDevice, ADIN1200_ADDR_GE_CLK_CFG, ADIN1200_BITM_GE_CLK_CFG_GE_REF_CLK_EN);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
  return result;
}

/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle.
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Enable GEClkFree125
 *
 * @sa
 */
static adi_eth_Result_e PHY_GeClkFree125En(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    /* Disabling other clocks */
    result = PHY_Write(hDevice, ADIN1200_ADDR_GE_CLK_CFG, 0x00);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    /* Enabling only GE CLK Free 125 and disabling other clocks*/
    result = PHY_Write(hDevice, ADIN1200_ADDR_GE_CLK_CFG, ADIN1200_BITM_GE_CLK_CFG_GE_CLK_FREE_125_EN);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

end:
  return result;
}

/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Enable GECLKRcvr125En
 *
 * @sa
 */
static adi_eth_Result_e PHY_GeClkRCVR125En(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    /* Disabling other clocks */
    result = PHY_Write(hDevice, ADIN1200_ADDR_GE_CLK_CFG, 0x00);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

     /* Enabling only GE CLK RCVR 125 and disabling other clocks*/
    result = PHY_Write(hDevice, ADIN1200_ADDR_GE_CLK_CFG, ADIN1200_BITM_GE_CLK_CFG_GE_CLK_RCVR_125_EN);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
  return result;
}


/*!
 * @brief           Subsystem Clock Configuration
 *
 * @param [in]      hDevice     Device driver handle
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         Disable GECLK
 *
 *
 * @sa
 */
static adi_eth_Result_e PHY_GeClkDisable(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    /* Enabling only GE CLK RCVR 125 and disabling other clocks*/
    result = PHY_Write(hDevice, ADIN1200_ADDR_GE_CLK_CFG, ADIN1200_RSTVAL_GE_CLK_CFG);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
  return result;
}

/**************************************************/
/***                                            ***/
/***                PHY Reset                   ***/
/***                                            ***/
/**************************************************/
/*!
 * @brief           Reset the PHY device.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      resetType   Reset type of ADINPHY
 * @return          - #ADI_ETH_SUCCESS
 *                  - #ADI_ETH_COMM_ERROR
 *                  - #ADI_ETH_INVALID_PARAM
 *
 * @details        3 Types of Reset
 *                 - Subsystem SW Reset with pin configuration
 *                 - Subsystem SW Reset
 *                 - PHY Core SW Reset
 *
 * @sa
 */
static adi_eth_Result_e PHY_Reset(adin1200_phy_Device_t *hDevice, adin1200_phy_ResetType_e resetType)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 = 0;

    if(hDevice->irqPending)
    {
        result = ADI_ETH_IRQ_PENDING;
        goto end;
    }

    switch (resetType)
    {
        case ADIN1200_PHY_RESET_TYPE_SW_WITH_PIN_CONFIG:
            /* A subsystem reset with a pin configuration can be initiated by setting GE_SFT_RST_CFG_EN (Address 0xFF0D) to 1 before setting GE_SFT_RST bit (Address 0xFF0C) to 1.*/
            val16 = (1 << ADIN1200_BITP_GE_SFT_RST_CFG_EN_GE_SFT_RST_CFG_EN);
            result = PHY_Write(hDevice, ADIN1200_ADDR_GE_SFT_RST_CFG_EN, val16);
            val16 = (1 << ADIN1200_BITP_GE_SFT_RST_GE_SFT_RST);
            result = PHY_Write(hDevice, ADIN1200_ADDR_GE_SFT_RST, val16);
            if(result != ADI_ETH_SUCCESS)
            {
                result = ADI_ETH_COMM_ERROR;
                goto end;
            }
            else
            {
                result = phyInit(hDevice);
            }
            break;

        case ADIN1200_PHY_RESET_TYPE_SW:
            /* The subsystem can be reset by setting GE_SFT_RST (Subsystem Register 0xFF0C, Bit 0) to 1. */
            val16 = (1 << ADIN1200_BITP_GE_SFT_RST_GE_SFT_RST);
            result = PHY_Write(hDevice, ADIN1200_ADDR_GE_SFT_RST, val16);
            if(result != ADI_ETH_SUCCESS)
            {
                result = ADI_ETH_COMM_ERROR;
                goto end;
            }
            else
            {
                result = phyInit(hDevice);
            }
            break;

        case ADIN1200_PHY_CORE_SW_RESET:
            /* The PHY core registers can be reset by setting the SFT_RST bit in the MII_CONTROL register, Address 0x0000 to 1*/
            result = PHY_Read(hDevice, ADIN1200_ADDR_MII_CONTROL, &val16);
            if(result != ADI_ETH_SUCCESS)
            {
                result = ADI_ETH_COMM_ERROR;
                goto end;
            }

            val16 |= ADIN1200_BITM_MII_CONTROL_SFT_RST;
            result = PHY_Write(hDevice, ADIN1200_ADDR_MII_CONTROL, val16);
            if(result != ADI_ETH_SUCCESS)
            {
                result = ADI_ETH_COMM_ERROR;
            }
            else
            {
                result = phyInit(hDevice);
            }
            break;

        default:
        {
            result = ADI_ETH_INVALID_PARAM;
            goto end;
        }
    }

end:
  return result;
}

/**************************************************/
/***                                            ***/
/***                MDIO Communication          ***/
/***                                            ***/
/**************************************************/

/*!
 * @brief            MDIO Write with Clause22 or Clause45
 *
 * @param [in]       hDevice    Device driver handle.
 * @param [in]       regAddr    PHY MDIO Register address to write.
 * @param [out]      data       Data to write.
 *
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         This function writes the PHY registers in both Clause22 and Clause45
 *
 * @sa              PHY_Read()
*/
static adi_eth_Result_e PHY_Write(adin1200_phy_Device_t *hDevice, uint32_t regAddr, uint16_t data)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint32_t            backup;

    backup = ADI_HAL_GET_ENABLE_IRQ(hDevice->adinDevice);
    ADI_HAL_DISABLE_IRQ(hDevice->adinDevice);

    if(hDevice->writeFn(hDevice->phyAddr, regAddr, data) != ADI_HAL_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    if(backup)
    {
        ADI_HAL_ENABLE_IRQ(hDevice->adinDevice);
    }
end:
    return result;
}

/*!
 * @brief            MDIO Read with Clause22 or Clause45
 *
 * @param [in]       hDevice    Device driver handle.
 * @param [in]       regAddr    PHY MDIO Register address to read.
 * @param [out]      data       Pointer to the data buffer.
 *
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         This function reads the PHY registers in both Clause22 and Clause45
 *
 * @sa              PHY_Write()
*/
static adi_eth_Result_e PHY_Read(adin1200_phy_Device_t *hDevice, uint32_t regAddr, uint16_t *data)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint32_t            backup;

    backup = ADI_HAL_GET_ENABLE_IRQ(hDevice->adinDevice);
    ADI_HAL_DISABLE_IRQ(hDevice->adinDevice);

    /* The only error returned by the HAL function is caused by 2nd TA bit */
    /* not being pulled low, which indicates the MDIO interface on the PHY */
    /* device is not operational.                                          */
    if(hDevice->readFn(hDevice->phyAddr, regAddr, data) != ADI_HAL_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    if(backup)
    {
        ADI_HAL_ENABLE_IRQ(hDevice->adinDevice);
    }
end:
    return result;
}

/*!
 * @brief           Enable or disable the LED.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [in]      ledSel      Selection of the LED port.
 * @param [in]      enable      Enable/disable the LED.
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_IRQ_PENDING.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         This functions enables the PHY LED
 *
 * @sa
 */
static adi_eth_Result_e PHY_LedEn(adin1200_phy_Device_t *hDevice, adi_phy_LedPort_e ledSel, bool enable)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16 =0 ;

    result = PHY_Read(hDevice, ADIN1200_ADDR_LED_CTRL_1, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }

    if(ledSel == ADI_PHY_LED_0)
    {
        if(enable) // Enable PHY_LED output
            val16 &= ~ADIN1200_BITM_LED_CTRL_1_LED_OE_N;
        else  // Disable PHY_LED outputs
            val16 |= ADIN1200_BITM_LED_CTRL_1_LED_OE_N;
    }
    result = PHY_Write(hDevice, ADIN1200_ADDR_LED_CTRL_1, val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_COMM_ERROR;
        goto end;
    }
end:
    return result;
}

/*!
 * @brief           Discover mdio device
 *
 * @param [in]      hDevice   Device driver handle.
 * @return
 *
 * @details         This function discovers the PHY device connected over the MDIO lines
 *
 * @sa
 */
static adi_eth_Result_e phyDiscovery(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint32_t            modelNum;
    uint32_t            revNum;

    /* Checks the identity of the device based on reading of hardware ID registers */
    result = checkIdentity(hDevice, &modelNum, &revNum);
    if(result != ADI_ETH_SUCCESS)
    {
        hDevice->state = ADI_PHY_STATE_ERROR;
        goto end;
    }

    if(modelNum == ADI_PHY_DEV_ID2_MODEL_NUM_ADIN1200)
    {
        hDevice->mdioPhydevice = ADI_ADIN1200_DEVICE;
    }
    else if(modelNum == ADI_PHY_DEV_ID2_MODEL_NUM_ADIN1300)
    {
        hDevice->mdioPhydevice = ADI_ADIN1300_DEVICE;
    }
end:
    return result;
}

/*!
 * @brief           Check the device identity.
 *
 * @param [in]      hDevice     Device driver handle.
 * @param [out]     modelNum    PHY Device model number
 * @param [out]     revNum      PHY Device rev number
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_COMM_ERROR.
 *                  - #ADI_ETH_UNSUPPORTED_DEVICE.
 *
 * @details         This function is called after a reset event and before the
 *                  initialization of the device is performed.
 *                  It reads the PHY_ID_1 /PHY_ID_2 registers and compares
 *                  them with expected values. If comparison is not successful,
 *                  return ADI_PHY_UNSUPPORTED_DEVID
 *
 * @sa
 */
static adi_eth_Result_e checkIdentity(adin1200_phy_Device_t *hDevice, uint32_t *modelNum, uint32_t *revNum)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    uint16_t            val16;

    result = PHY_Read(hDevice, ADIN1200_ADDR_PHY_ID_1, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        goto end;
    }
    if (val16 != ADI_PHY_DEV_ID1)
    {
        result = ADI_ETH_UNSUPPORTED_DEVICE;
        goto end;
    }

    result = PHY_Read(hDevice, ADIN1200_ADDR_PHY_ID_2, &val16);
    if(result != ADI_ETH_SUCCESS)
    {
        result = ADI_ETH_UNSUPPORTED_DEVICE;
        goto end;
    }

    /*Check if the value of PHY_ID_2.OUI matches expected value */
    if((val16 & ADIN1200_BITM_PHY_ID_2_PHY_ID_2_OUI) != (ADI_PHY_DEV_ID2_OUI << ADIN1200_BITP_PHY_ID_2_PHY_ID_2_OUI))
    {
        result = ADI_ETH_UNSUPPORTED_DEVICE;
        goto end;
    }

    *modelNum = (uint32_t)((val16 & ADIN1200_BITM_PHY_ID_2_MODEL_NUM) >> ADIN1200_BITP_PHY_ID_2_MODEL_NUM);
    *revNum = (uint32_t)((val16 & ADIN1200_BITM_PHY_ID_2_REV_NUM) >> ADIN1200_BITP_PHY_ID_2_REV_NUM);
end:
    return result;

}

/*!
 * @brief            Delay function
 *
 * @param [in]       hDevice     Device driver handle.
 *
 * @return
 *                  - #ADI_ETH_SUCCESS.
 *                  - #ADI_ETH_COMM_ERROR.
 *
 * @details         This function polls the PHY registers and exists when the MDIO interface is up !
 *
*/
static adi_eth_Result_e waitMdio(adin1200_phy_Device_t *hDevice)
{
    adi_eth_Result_e    result = ADI_ETH_SUCCESS;
    int32_t             iter = ADI_PHY_MDIO_POWERUP_ITER;
    uint16_t            val16;

    /* Wait until MDIO interface is up */
    do
    {
        result = PHY_Read(hDevice, ADIN1200_ADDR_PHY_ID_1, &val16);
    } while ((result == ADI_ETH_COMM_ERROR) && (--iter));

    if(result == ADI_ETH_COMM_ERROR)
    {
        goto end;
    }
end:
    return result;
}

/*!
 * @brief      PHY Driver Entry.
 *
 */
adin1200_phy_DriverEntry_t adin1200PhyDriverEntry =
{
    PHY_Init,
    PHY_UnInit,
    PHY_ReInitPhy,
    PHY_GetDeviceId,
    PHY_RegisterCallback,
    PHY_ReadIrqStatus,
    PHY_SoftwarePowerdown,
    PHY_GetSoftwarePowerdown,
    PHY_GetLinkStatus,
    PHY_LinkConfig,
    PHY_AnegEnable,
    PHY_AnegRestart,
    PHY_GetAnStatus,
    PHY_AnegAdverConfig,
    PHY_AnegAdvertised,
    PHY_AutoMDIXConfig,
    PHY_ConfigEnergyDetectPWD,
    PHY_GetEDetectPWDStatus,
    PHY_ConfigDownspeed,
    PHY_SetDownspeedRetries,
    PHY_ForceSpeed,
    PHY_EnableEEE,
    PHY_GetEEE,
    PHY_ResetRMIIFIFO,
    PHY_SelectMACInterface,
    PHY_EnableRGMIITxRx,
    PHY_GetMasterSlaveStatus,
    PHY_GeClkDisable,
    PHY_GeClk25En,
    PHY_GeClkHrtFreeEn,
    PHY_GeClkRCVREn,
    PHY_GeClkRefEn,
    PHY_GeClkFree125En,
    PHY_GeClkRCVR125En,
    PHY_Reset,
    PHY_Read,
    PHY_Write,
    PHY_LedEn,
    PHY_MasterSlaveConfig,
};

/** @}*/
