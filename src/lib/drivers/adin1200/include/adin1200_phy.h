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

/** @addtogroup phy PHY Definitions
 *  @{
 */

#ifndef ADI_PHY_H
#define ADI_PHY_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "adin_eth_common.h"
#include "adin1200_phy_addr_rdef_22.h"
#include "adin1200_phy_addr_rdef_45.h"

/* The writeFn and readFn functions are available in the HAL layer.
 * These HAL API functions will not be provided as part of the ADIN1200/ADIN1300 NON-OS Device driver release */
#include "adin1200_hal.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*! Size of the PHY device structure, in bytes. Needs to be a multiple of 4. */
#define ADI_PHY_DEVICE_SIZE           (48)

/*! Hardware reset value of ADDR_PHY_ID_1 register, used for device identification. */
#define ADI_PHY_DEV_ID1               (0x0283)
/*! Hardware reset value of ADDR_PHY_ID_2 register (OUI field), used for device identification. */
#define ADI_PHY_DEV_ID2_OUI           (0x2F) // The value read from PHY_ID_2 should be (0xBC20)

/*! Hardware reset value of ADDR_PHY_ID_2 register (MODEL_NUM field), used for device identification. */
#define ADI_PHY_DEV_ID2_MODEL_NUM_ADIN1200  (0x2)

/*! Hardware reset value of ADDR_PHY_ID_2 register (MODEL_NUM field), used for device identification. */
#define ADI_PHY_DEV_ID2_MODEL_NUM_ADIN1300  (0x3)

/*! Hardware reset value of ADDR_PHY_ID_2 register (REV_NUM field), used for device identification. */
#define ADI_PHY_DEV_ID2_REV_NUM           (0x0)


/*!
 * @name PHY Capabilities
 * List of PHY capabilities.
 */
/** @{ */
#define ADI_PHY_NO_PHY                      (0)         /*!< No PHY support (base value).        */
#define ADI_PHY_LINK_IS_UP                  (1 << 1)    /*!< PHY Link is UP                      */
#define ADI_PHY_IN_SWPD                     (1 << 2)    /*!< PHY is in Software Power Down mode  */
#define ADI_AUTONEG_ENABLED                 (1 << 3)    /*!< PHY Auto negotiation is Enabled  */

/** @} */

/*!
* @brief ADINPHY device identification
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
} adinPhy_DeviceId_t;


/*!
 * @brief PHY interrupt events.
 */
typedef enum
{
    ADIN1200_CBL_DIAG_IRQ_EN                = (1 << ADIN1200_BITP_IRQ_MASK_CBL_DIAG_IRQ_EN),           /*!< Cable Diagnostics Interrupt Enable Bit */
    ADIN1200_MDIO_SYNC_IRQ_EN               = (1 << ADIN1200_BITP_IRQ_MASK_MDIO_SYNC_IRQ_EN),          /*!< MDIO Synchronization Lost Interrupt Enable Bit */
    ADIN1200_AN_STAT_CHNG_IRQ_EN            = (1 << ADIN1200_BITP_IRQ_MASK_AN_STAT_CHNG_IRQ_EN),       /*!< Autonegotiation Status Changed Interrupt Enable Bit */
    ADIN1200_FC_FG_IRQ_EN                   = (1 << ADIN1200_BITP_IRQ_MASK_FC_FG_IRQ_EN),              /*!< Frame checker/generator interrupt enable bit */
    ADIN1200_PAGE_RX_IRQ_EN                 = (1 << ADIN1200_BITP_IRQ_MASK_PAGE_RX_IRQ_EN),            /*!< Autonegotiation Page Received Interrupt Enable Bit */
    ADIN1200_IDLE_ERR_CNT_IRQ_EN            = (1 << ADIN1200_BITP_IRQ_MASK_IDLE_ERR_CNT_IRQ_EN),       /*!< Idle Error Counter Saturated Interrupt Enable Bit */
    ADIN1200_FIFO_OU_IRQ_EN                 = (1 << ADIN1200_BITP_IRQ_MASK_FIFO_OU_IRQ_EN),            /*!< MAC Interface FIFO Overflow/Underflow Interrupt Enable Bit */
    ADIN1200_RX_STAT_CHNG_IRQ_EN            = (1 << ADIN1200_BITP_IRQ_MASK_RX_STAT_CHNG_IRQ_EN),       /*!< Receive Status Changed Interrupt Enable Bit */
    ADIN1200_LNK_STAT_CHNG_IRQ_EN           = (1 << ADIN1200_BITP_IRQ_MASK_LNK_STAT_CHNG_IRQ_EN),      /*!< Link Status Changed Interrupt Enable Bit */
    ADIN1200_SPEED_CHNG_IRQ_EN              = (1 << ADIN1200_BITP_IRQ_MASK_SPEED_CHNG_IRQ_EN),         /*!< Speed Changed Interrupt Enable Bit  */
    ADIN1200_HW_IRQ_EN                      = (1 << ADIN1200_BITP_IRQ_MASK_HW_IRQ_EN),	         /*!< This bit enables the HW interrupt pin, INT_N */
} adin1200_phy_InterruptEvt_e;

/*!
* @brief PHY Device connected to MDIO
*/
typedef enum
{
    ADI_ADIN1200_DEVICE               = 0,   /*!< ADIN1200 device is connected over the MDIO Lines */
    ADI_ADIN1300_DEVICE               = 1,   /*!< ADIN1300 device is connected over the MDIO Lines */
} adi_mdio_phyDevice_e;

/*!
 * @brief Autoneg Advertisement Register types.
 */
typedef enum
{
    ADI_PHY_ANEG_HD_10_ADV_EN = 0,            /*!< This bit is to advertise 10BASE-T half duplex  */
    ADI_PHY_ANEG_HD_10_ADV_DIS,               /*!< This bit is Not to advertise 10BASE-T half duplex  */
    ADI_PHY_ANEG_FD_10_ADV_EN,                /*!< This bit is to advertise 10BASE-T full duplex  */
    ADI_PHY_ANEG_FD_10_ADV_DIS,               /*!< This bit is Not to advertise 10BASE-T full duplex  */
    ADI_PHY_ANEG_HD_100_ADV_EN,               /*!< This bit is to advertise 100BASE-TX half duplex.  */
    ADI_PHY_ANEG_HD_100_ADV_DIS,              /*!< This bit is NOT to advertise 100BASE-T half duplex  */
    ADI_PHY_ANEG_FD_100_ADV_EN,               /*!< This bit is to advertise 100BASE-TX full duplex  */
    ADI_PHY_ANEG_FD_100_ADV_DIS,              /*!< This bit is Not to advertise 100BASE-T full duplex  */
    ADI_PHY_ANEG_FD_1000_ADV_EN,              /*!< This bit is to advertise 1000BASE-T full duplex - Only for ADINPHY 1G */
    ADI_PHY_ANEG_FD_1000_ADV_DIS,             /*!< This bit is Not to advertise 1000BASE-T full duplex - Only for v  */
    ADI_PHY_ANEG_HD_1000_ADV_EN,              /*!< This bit is to advertise 1000BASE-T half duplex - Only for ADINPHY 1G  */
    ADI_PHY_ANEG_HD_1000_ADV_DIS,             /*!< This bit is Not to advertise 1000BASE-T half duplex - Only for ADINPHY 1G  */
} adi_phy_AnegAdv_e;

/*!
 * @brief Auto-negotiation master-slave advertisement.
 */
typedef enum
{
    ADI_PHY_MAN_ADV_MASTER = 0,              /*!< Force master.          */
    ADI_PHY_MAN_ADV_SLAVE,                   /*!< Force slave.           */
    ADI_PHY_MAN_MSTR_SLV_DIS,                /*!< Disable Manual Master-Slave configuration */
} adi_phy_AdvMasterSlaveCfg_e;

/*!
* @brief Autonegotiation Done status.
*/
typedef enum
{
    ADI_PHY_ANEG_NOT_COMPLETED        = (0),   /*!< Autonegotiation is Not completed */
    ADI_PHY_ANEG_COMPLETED            = (1),   /*!< Autonegotiation is completed */
} adi_phy_AnegStatus_e;


/*!
* @brief Autonegotiation Enable/Disable.
*/
typedef enum
{
    ADI_PHY_ANEG_DIS               = (0),     /*!< Disable Autonegotiation */
    ADI_PHY_ANEG_EN                = (1),     /*!< Enable Autonegotiation */
} adi_phy_AnegEn_e;


/*!
* @brief software powerdown
*/
typedef enum
{
    ADI_PHY_SWPD_DIS             = (0),        /*!< Disable Software Powerdown */
    ADI_PHY_SWPD_EN              = (1),        /*!< Enable Software Powerdown */
} adi_phy_swpd_e;



/*!
* @brief Master slave status.
*/
typedef enum
{
    ADI_RESOLVED_TO_SLAVE             = (0),      /*!< MSTR_RSLVD = 0 - local PHY configuration resolved to slave  */
    ADI_RESOLVED_TO_MASTER,                       /*!< MSTR_RSLVD = 1 - local PHY configuration resolved to master */
    ADI_MAST_SLAVE_FAULT_DETECT,                  /*!< MSTR_SLV_FLT  is 1 - master slave configuration fault detected */
} adi_Master_Slave_Status_e;


/*!
 * @brief Auto-MDIX configuration.
 */
typedef enum
{
    ADI_PHY_MANUAL_MDI = 1,           /*!< Manual MDI               Mode 1   */
    ADI_PHY_MANUAL_MDIX,              /*!< Manual MDIX              Mode 2   */
    ADI_PHY_AUTO_MDIX_PREFER_MDIX,    /*!< Auto MDIX, Prefer MDIX   Mode 3   */
    ADI_PHY_AUTO_MDIX_PREFER_MDI,     /*!< Auto MDIX, Prefer MDI    Mode 4   */
} adi_phy_AutoMDIXConfig_e;


/*!
 * @brief MAC RGMII configuration.
 */
typedef enum
{
    ADI_RGMII_TX_ID_DIS = 0,          /*!< disable transmit clock internal 2 ns delay in RGMII mode */
    ADI_RGMII_TX_ID_EN,               /*!< enable transmit clock internal 2 ns delay in RGMII mode  */
    ADI_RGMII_RX_ID_DIS,              /*!< disable receive clock internal 2 ns delay in RGMII mode  */
    ADI_RGMII_RX_ID_EN,               /*!< enable receive clock internal 2 ns delay in RGMII mode   */
}adi_MAC_RGMII_config_e;


/*!
 * @brief MAC interface.
 */
typedef enum
{
    ADI_RGMII_MAC_INTERFACE = 0,        /*!< RGMII MAC Interface is enabled */
    ADI_RMII_MAC_INTERFACE,             /*!< RMII MAC Interface is enabled */
    ADI_MII,                            /*!< MII MAC Interface is enabled */
}adi_MAC_Interface_e;

/*!
 * @brief Energy Detect Power Down mode
 */
typedef enum
{
    ADI_NRG_PD_DIS = 0,       /*!< enable energy detect power-down mode.  */
    ADI_NRG_PD_EN,           /*!< disable energy detect power-down mode.  */
    ADI_NRG_PD_TX_EN,        /*!< enable periodic transmission of the pulse while in energy detect power-down mode. */
    ADI_NRG_PD_TX_DIS,       /*!< disable periodic transmission of the pulse while in energy detect power-down mode */
}adi_Energy_Detect_PWD_e;

/*!
 * @brief Energy Detect Power Down mode
 */
typedef enum
{
    ADI_PHY_NOT_IN_NRG_PD_MODE=0,   /*!< PHY is not in energy detect power-down mode */
    ADI_PHY_IN_NRG_PD_MODE,         /*!< PHY is in energy detect power-down mode */
}adi_Energy_Detect_PWD_Status_e;

/*!
 * @brief PHY reset types.
 */
typedef enum
{
    ADIN1200_PHY_RESET_TYPE_SW_WITH_PIN_CONFIG = 0,      /*!< Software reset with pin configuration  */
    ADIN1200_PHY_RESET_TYPE_SW,                          /*!< Software reset */
    ADIN1200_PHY_CORE_SW_RESET,                          /*!< PHY Core Software reset */
} adin1200_phy_ResetType_e;

/*!
 * @brief Force Speed
 */
typedef enum
{
    ADI_SPEED_10BASE_T_HD = 0,            /*!< Speed is forced to 10 HD */
    ADI_SPEED_10BASE_T_FD,                /*!< Speed is forced to 10 FD */
    ADI_SPEED_100BASE_TX_HD,              /*!< Speed is forced to 100 HD */
    ADI_SPEED_100BASE_TX_FD,              /*!< Speed is forced to 100 FD */
}adi_force_speed_e;

/*!
 * @brief GE CLK Config
 */
typedef enum
{
    ADI_GE_CLK_DISABLE = 0,         /*!< All GE clocks are disabled */
    ADI_GE_CLK_25M_EN,              /*!< When Enabled, the 25 MHz reference clock from the crystal oscillator is driven at the GP_CLK pin  */
    ADI_GE_CLK_HRT_FREE_EN,         /*!< When Enabled, the free running heartbeat clock is driven at the GP_CLK pin */
    ADI_GE_CLK_HRT_RCVR_EN,         /*!< When Enabled, the recovered heartbeat clock is driven at the GP_CLK pin.  */
    ADI_GE_CLK_REF_EN,              /*!< Only for ADINPHY 1G - When Enabled, 25 MHz reference clock from the crystal oscillator is driven at the CLK25_REF pin  */
    ADI_GE_CLK_FREE_125_EN,         /*!< When Enabled, the 125 MHz PHY free running clock is driven at the GP_CLK pin */
    ADI_GE_CLK_RCVR_125_EN,         /*!< When Enabled, the 125 MHz PHY recovered clock (or PLL clock) is driven at the GP_CLK pin */
}adi_ge_clk_config_e;

/*!
* @brief Downspeed Config
*/
typedef enum
{
    ADI_PHY_DOWNSPEED_T0_10_DIS = (0),  /*!< PHY Downspeed to 10Base-T is Disabled */
    ADI_PHY_DOWNSPEED_TO_10_EN,         /*!< PHY Downspeed to 10Base-T is Enabled */
    ADI_PHY_DOWNSPEED_T0_100_DIS,       /*!< PHY Downspeed to 100Base-TX is Disabled - Only for ADINPHY 1G */
    ADI_PHY_DOWNSPEED_TO_100_EN,        /*!< PHY Downspeed to 100Base-TX is Enabled - Only for ADINPHY 1G */
} adi_phy_downspeed_e;

/*!
* @brief EEE - Energy Efficient Ethernet
*/
typedef enum
{
    ADI_PHY_EEE_100_ADV_DIS  = 0,   /*!< Do not advertise that the 100BASE-TX has EEE capability */
    ADI_PHY_EEE_100_ADV_EN,         /*!< Advertise that the 100BASE-TX has EEE capability */
    ADI_PHY_EEE_1000_ADV_DIS,       /*!< Do not advertise that the 1000BASE-TX has EEE capability */
    ADI_PHY_EEE_1000_ADV_EN,        /*!< Advertise that the 1000BASE-TX has EEE capability */
}adi_phy_eee_en_e;

/*!
 * @brief Pointer to function to use to read PHY registers.
 */
typedef uint32_t (*HAL_ReadFn_t)    (uint8_t hwAddr, uint32_t regAddr, uint16_t *data);
/*!
 * @brief Pointer to function to use to write PHY registers.
 */
typedef uint32_t (*HAL_WriteFn_t)   (uint8_t hwAddr, uint32_t regAddr, uint16_t data);


/** @}*/

/*! @cond PRIVATE */

/*! Timeout for MDIO interface bringup after a powerup event or equivalent, in miliseconds. */
#define ADI_PHY_MDIO_POWERUP_TIMEOUT        (100)
/*! Timeout for system ready after a powerup event or equivalent, in miliseconds. */
#define ADI_PHY_SYS_RDY_TIMEOUT             (700)

/*! Timeout for MDIO interface bringup, in number of iterations. */
#define ADI_PHY_MDIO_POWERUP_ITER           (uint32_t)(ADI_PHY_MDIO_POWERUP_TIMEOUT * 1000 / ADI_HAL_MDIO_READ_DURATION)
/*! Timeout for system ready, in number of iterations. */
#define ADI_PHY_SYS_RDY_ITER                (uint32_t)(ADI_PHY_SYS_RDY_TIMEOUT * 1000 / ADI_HAL_MDIO_READ_DURATION)

/*! Number of retries allowed for software powerdown entry, in MDIO read count. */
#define ADI_PHY_SOFT_PD_ITER                (10)





/*!
* @brief Callback function definition for the Ethernet devices.
*/
typedef void (* adi_eth_Callback_t) (
    void      *pCBParam,                /*!< Client-supplied callback parameter. */
    uint32_t   Event,                   /*!< Event ID specific to the Driver/Service. */
    void      *pArg                     /*!< Pointer to the event-specific argument. */

    );

/*!
 * @brief Phy device structure
 */
typedef struct
{
    uint32_t                phyAddr;
    adi_phy_State_e         state;
    HAL_ReadFn_t            readFn;
    HAL_WriteFn_t           writeFn;
    adi_phy_LinkStatus_e    linkStatus;
    adi_eth_Callback_t      cbFunc;
    uint32_t                cbEvents;
    void                    *cbParam;
    void                    *adinDevice;
    uint32_t                irqMask;
    bool                    irqPending;
    adi_mdio_phyDevice_e    mdioPhydevice;
    adi_phy_Stats_t         stats;
} adin1200_phy_Device_t;

/*!
 * @brief Phy driver entry functions.
 */
typedef struct
{
    adi_eth_Result_e (*Init)                    (adin1200_phy_Device_t **hPhyDevice, adi_phy_DriverConfig_t *cfg, void *adinDevice, HAL_ReadFn_t readFn, HAL_WriteFn_t writeFn);
    adi_eth_Result_e (*UnInit)                  (adin1200_phy_Device_t *hDevice);
    adi_eth_Result_e (*ReInitPhy)               (adin1200_phy_Device_t *hDevice);
    adi_eth_Result_e (*GetDeviceId)             (adin1200_phy_Device_t *hDevice, adinPhy_DeviceId_t *pDevId);
    adi_eth_Result_e (*RegisterCallback)        (adin1200_phy_Device_t *hDevice, adi_eth_Callback_t cbFunc, uint32_t cbEvents, void *cbParam);
    adi_eth_Result_e (*ReadIrqStatus)           (adin1200_phy_Device_t *hDevice, uint32_t *status);
    adi_eth_Result_e (*SoftwarePowerdown)       (adin1200_phy_Device_t *hDevice, adi_phy_swpd_e swpd);
    adi_eth_Result_e (*GetSoftwarePowerdown)    (adin1200_phy_Device_t *hDevice, bool *enable);
    adi_eth_Result_e (*GetLinkStatus)           (adin1200_phy_Device_t *hDevice, adi_phy_LinkStatus_e *status);
    adi_eth_Result_e (*LinkConfig)              (adin1200_phy_Device_t *hDevice, bool enable);
    adi_eth_Result_e (*AnegEnable)              (adin1200_phy_Device_t *hDevice, adi_phy_AnegEn_e enable);
    adi_eth_Result_e (*AnegRestart)             (adin1200_phy_Device_t *hDevice);
    adi_eth_Result_e (*GetAnStatus)             (adin1200_phy_Device_t *hDevice, adi_phy_AnegStatus_e *status);
    adi_eth_Result_e (*AnegAdverConfig)         (adin1200_phy_Device_t *hDevice, adi_phy_AnegAdv_e anegAdver);
    adi_eth_Result_e (*AnegAdvertised)           (adin1200_phy_Device_t *hDevice, uint16_t *phy_speed, uint16_t *phy_speed1G);
    adi_eth_Result_e (*AutoMDIXConfig)          (adin1200_phy_Device_t *hDevice, adi_phy_AutoMDIXConfig_e autoMdixConfig);
    adi_eth_Result_e (*ConfigEnergyDetectPWD)   (adin1200_phy_Device_t *hDevice, adi_Energy_Detect_PWD_e EnergyDetectPWD);
    adi_eth_Result_e (*GetEDetectPWDStatus)     (adin1200_phy_Device_t *hDevice, adi_Energy_Detect_PWD_Status_e *status);
    adi_eth_Result_e (*ConfigDownspeed)         (adin1200_phy_Device_t *hDevice, adi_phy_downspeed_e downspeedConfig);
    adi_eth_Result_e (*SetDownspeedRetries)     (adin1200_phy_Device_t *hDevice, uint16_t downspeedRetiries);
    adi_eth_Result_e (*ForceSpeed)              (adin1200_phy_Device_t *hDevice, adi_force_speed_e forceSpeed);
    adi_eth_Result_e (*EnableEEE)               (adin1200_phy_Device_t *hDevice, adi_phy_eee_en_e enableEEE);
    adi_eth_Result_e (*GetEEE)                  (adin1200_phy_Device_t *hDevice, adi_phy_eee_en_e *status);
    adi_eth_Result_e (*ResetRMIIFIFO)           (adin1200_phy_Device_t *hDevice);
    adi_eth_Result_e (*SelMACInterface)         (adin1200_phy_Device_t *hDevice, adi_MAC_Interface_e MACInterface);
    adi_eth_Result_e (*EnableRGMIITxRx)         (adin1200_phy_Device_t *hDevice, adi_MAC_RGMII_config_e MACRGMIIConfig);
    adi_eth_Result_e (*GetMasterSlaveStatus)    (adin1200_phy_Device_t *hDevice, adi_Master_Slave_Status_e *status);
    adi_eth_Result_e (*GeClkDisable)            (adin1200_phy_Device_t *hDevice);
    adi_eth_Result_e (*GeClk25En)               (adin1200_phy_Device_t *hDevice);
    adi_eth_Result_e (*GeClkHrtFreeEn)          (adin1200_phy_Device_t *hDevice);
    adi_eth_Result_e (*GeClkRCVREn)             (adin1200_phy_Device_t *hDevice);
    adi_eth_Result_e (*GeClkRefEn)              (adin1200_phy_Device_t *hDevice);
    adi_eth_Result_e (*GeClkFree125En)          (adin1200_phy_Device_t *hDevice);
    adi_eth_Result_e (*GeClkRCVR125En)          (adin1200_phy_Device_t *hDevice);
    adi_eth_Result_e (*Reset)                   (adin1200_phy_Device_t *hDevice, adin1200_phy_ResetType_e resetType);
    adi_eth_Result_e (*PHY_Read)                (adin1200_phy_Device_t *hDevice, uint32_t regAddr, uint16_t *data);
    adi_eth_Result_e (*PHY_Write)               (adin1200_phy_Device_t *hDevice, uint32_t regAddr, uint16_t data);
    adi_eth_Result_e (*LedEn)                   (adin1200_phy_Device_t *hDevice, adi_phy_LedPort_e ledSel, bool enable);
    adi_eth_Result_e (*MasterSlaveConfig)       (adin1200_phy_Device_t *hDevice, adi_phy_AdvMasterSlaveCfg_e masterSlaveConfig);
} adin1200_phy_DriverEntry_t;

extern adin1200_phy_DriverEntry_t adin1200PhyDriverEntry;

/*! @endcond */

#ifdef __cplusplus
}
#endif

#endif /* ADI_PHY_H */
