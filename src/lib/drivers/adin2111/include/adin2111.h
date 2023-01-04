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

/** @addtogroup adin2111 ADIN2111 LES Software Driver
 *  @{
 */

#ifndef __ADIN2111_H__
#define __ADIN2111_H__

#include "adi_phy.h"
#include "adi_mac.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*! ADIN2111 driver major version. */
#define ADIN2111_VERSION_MAJOR      (1)
/*! ADIN2111 driver minor version. */
#define ADIN2111_VERSION_MINOR      (0)
/*! ADIN2111 driver patch version. */
#define ADIN2111_VERSION_PATCH      (0)
/*! ADIN2111 driver extra version. */
#define ADIN2111_VERSION_EXTRA      (0)

/*! ADIN2111 driver version. */
#define ADIN2111_VERSION            ((ADIN2111_VERSION_MAJOR << 24) | \
                                     (ADIN2111_VERSION_MINOR << 16) | \
                                     (ADIN2111_VERSION_PATCH << 8) | \
                                     (ADIN2111_VERSION_EXTRA))

/*!
 * @brief       The size of the dynamic forwarding table.
 *
 * @details     Should be the number of hosts you expect to communicate with
 *              plus a few (e.g. 8) to cover ARP requests to other devices, etc.
 */
#define ADIN2111_DYNAMIC_TABLE_SIZE         (16)

/*!
 * @brief       Determines how long entries in the dynamic forwarding table persist.
 *
 * @details     This is the number of calls to adin2111_AgeTick() that it takes
 *              to age out an entry in the table.
 */
#define ADIN2111_DYNAMIC_TABLE_MAX_AGE      (16)

/*! Size of memory required for ADIN2111 device structure. */
#define ADIN2111_DEVICE_SIZE        (ADIN2111_PORT_NUM * (ADI_PHY_DEVICE_SIZE + sizeof(adi_phy_Device_t*)) + \
                                     ADI_MAC_DEVICE_SIZE + sizeof(adi_mac_Device_t*) + \
                                     sizeof(adin2111_DynamicTableEntry_t) * ADIN2111_DYNAMIC_TABLE_SIZE + 8)

/*! Maximum number of attempts to establish that both PHYs have come out of reset. */
/* Based on polling register at full speed. Can be shortcut by introducing a delay */
/* between reset and calling adin2111_Init().                                      */
#define ADIN2111_PHY_POWERUP_MAX_RETRIES    (200)

/*!
 * @brief Switch port.
 */
typedef enum
{
    ADIN2111_PORT_1         = 0,    /*!< Port 1.                    */
    ADIN2111_PORT_2         = 1,    /*!< Port 2.                    */
    ADIN2111_PORT_NUM       = 2,    /*!< Number of ports available. */
} adin2111_Port_e;

/*!
 * @brief Switch port used when transmitting a frame.
 */
typedef enum
{
    ADIN2111_TX_PORT_1         = 0,    /*!< Transmit on port 1.                         */
    ADIN2111_TX_PORT_2         = 1,    /*!< Transmit on port 2.                         */
    ADIN2111_TX_PORT_AUTO      = 2,    /*!< Transmit based on dynamic forwarding table. */
    ADIN2111_TX_PORT_FLOOD     = 3,    /*!< Transmit on both ports.                     */
} adin2111_TxPort_e;


/*!
 * @brief Dynamic forwarding table entry.
 */
typedef struct {
    uint8_t     age;                /*!< Age of the entry, if 0 the entry is not valid.                   */
    uint8_t     fromPort;           /*!< Frames with the MAC address source were received from this port. */
    uint8_t     macAddress[6];      /*!< MAC address.                                                     */
    uint8_t     refresh;            /*!< The entry has been refreshed by another frame arriving from the  */
                                    /*   same source address.                                             */
} adin2111_DynamicTableEntry_t;

/*!
* @brief ADIN2111 device identification.
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
} adin2111_DeviceId_t;

/*!
 * @brief TS_TIMER pin mux options.
 */
typedef enum
{
    ADIN2111_TS_TIMER_MUX_LED_0 =   (ENUM_DIGIO_PINMUX_DIGIO_TSTIMER_PINMUX_LED_0),         /*!< TS_TIMER assigned to LED_0.        */
    ADIN2111_TS_TIMER_MUX_INT_N =   (ENUM_DIGIO_PINMUX_DIGIO_TSTIMER_PINMUX_INT_N),         /*!< TS_TIMER assigned to INT_N.        */
    ADIN2111_TS_TIMER_MUX_NA    =   (ENUM_DIGIO_PINMUX_DIGIO_TSTIMER_PINMUX_TS_TIMER_NA),   /*!< TS_TIMER not assigned to any pin.  */
} adin2111_TsTimerPin_e;

/*!
 * @brief TS_CAPT pin mux options.
 */
typedef enum
{
    ADIN2111_TS_CAPT_MUX_LED_1  =  (ENUM_DIGIO_PINMUX_DIGIO_TSCAPT_PINMUX_LED_1),       /*!< TS_CAPT assigned to LED_1.         */
    ADIN2111_TS_CAPT_MUX_TEST_1 =  (ENUM_DIGIO_PINMUX_DIGIO_TSCAPT_PINMUX_TEST_1),      /*!< TS_CAPT assigned to TEST_1.        */
    ADIN2111_TS_CAPT_MUX_NA     =  (ENUM_DIGIO_PINMUX_DIGIO_TSCAPT_PINMUX_TS_CAPT_NA),  /*!< TS_CAPT not assigned to any pin.   */
} adin2111_TsCaptPin_e;

/*!
 * @brief ADIN2111 device driver structure.
 */
typedef struct
{
    adi_phy_Device_t                *pPhyDevice[ADIN2111_PORT_NUM];             /*!< Pointer to the PHY driver instance.            */
    adi_mac_Device_t                *pMacDevice;                                /*!< Pointer to the MAC driver instance.            */
    adin2111_DynamicTableEntry_t    dynamicTable[ADIN2111_DYNAMIC_TABLE_SIZE];  /*!< Dynamic forwarding table.                      */
    uint32_t                        dynamicTableReplaceLoc;                     /*!< If table full, replace entry at this index.    */
    uint8_t                         portEnabled[ADIN2111_PORT_NUM];             /*!< Indicates if a port is enabled.                */
    adin2111_TsTimerPin_e           tsTimerPin;                                 /*!< Pin assignment for TS_TIMER functionality.     */
    adin2111_TsCaptPin_e            tsCaptPin;                                  /*!< Pin assignment for TS_CAPT functionality.      */
} adin2111_DeviceStruct_t;

/*!
 * @brief ADIN2111 driver configuration.
 */
typedef struct
{
    void                    *pDevMem;       /*!< Pointer to memory area used by the driver.                                                             */
    uint32_t                devMemSize;     /*!< Size of the memory used by the driver. Needs to be greater than or equal to ADIN2111_DEVICE_SIZE.      */
    bool                    fcsCheckEn;     /*!< Configure the driver to check FCS on frame receive to the host. Note this is a check for SPI
                                             *   transaction integrity, the FCS is previously checked by the MAC hardware on frame receive to the MAC.  */
    adin2111_TsTimerPin_e   tsTimerPin;     /*!< Pin assignment for TS_TIMER functionality; unassigned by default.                                      */
    adin2111_TsCaptPin_e    tsCaptPin;      /*!< Pin assignment for TS_CAPT functionality; unassigned by default.                                       */
} adin2111_DriverConfig_t;

/*!
 * @brief ADIN2111 device handle.
 */
typedef adin2111_DeviceStruct_t*    adin2111_DeviceHandle_t;

adi_eth_Result_e    adin2111_Init                    (adin2111_DeviceHandle_t hDevice, adin2111_DriverConfig_t *pCfg);
adi_eth_Result_e    adin2111_UnInit                  (adin2111_DeviceHandle_t hDevice);
adi_eth_Result_e    adin2111_GetDeviceId             (adin2111_DeviceHandle_t hDevice, adin2111_DeviceId_t *pDevId);
adi_eth_Result_e    adin2111_Enable                  (adin2111_DeviceHandle_t hDevice);
adi_eth_Result_e    adin2111_Disable                 (adin2111_DeviceHandle_t hDevice);
adi_eth_Result_e    adin2111_EnablePort              (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port);
adi_eth_Result_e    adin2111_DisablePort             (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port);
adi_eth_Result_e    adin2111_Reset                   (adin2111_DeviceHandle_t hDevice, adi_eth_ResetType_e resetType);
adi_eth_Result_e    adin2111_SyncConfig              (adin2111_DeviceHandle_t hDevice);
adi_eth_Result_e    adin2111_GetLinkStatus           (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_eth_LinkStatus_e *linkStatus);
adi_eth_Result_e    adin2111_GetStatCounters         (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_eth_MacStatCounters_t *stat);
adi_eth_Result_e    adin2111_LedEn                   (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool enable);
adi_eth_Result_e    adin2111_SetLoopbackMode         (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_LoopbackMode_e loopbackMode);
adi_eth_Result_e    adin2111_SetTestMode             (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_TestMode_e testMode);

adi_eth_Result_e    adin2111_AddAddressFilter        (adin2111_DeviceHandle_t hDevice, uint8_t *macAddr, uint8_t *macAddrMask, adi_mac_AddressRule_t addrRule);
adi_eth_Result_e    adin2111_ClearAddressFilter      (adin2111_DeviceHandle_t hDevice, uint32_t addrIndex);
adi_eth_Result_e    adin2111_SubmitTxBuffer          (adin2111_DeviceHandle_t hDevice, adin2111_TxPort_e port, adi_eth_BufDesc_t *pBufDesc);
adi_eth_Result_e    adin2111_SubmitRxBuffer          (adin2111_DeviceHandle_t hDevice, adi_eth_BufDesc_t *pBufDesc);
#if defined(ADI_MAC_ENABLE_RX_QUEUE_HI_PRIO)
adi_eth_Result_e    adin2111_SubmitRxBufferHp        (adin2111_DeviceHandle_t hDevice, adi_eth_BufDesc_t *pBufDesc);
#endif
adi_eth_Result_e    adin2111_SetPromiscuousMode      (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool bFlag);
adi_eth_Result_e    adin2111_GetPromiscuousMode      (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool *pFlag);
adi_eth_Result_e    adin2111_SetPortForwardMode      (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool bFlag);
adi_eth_Result_e    adin2111_GetPortForwardMode      (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool *pFlag);
#if defined(CONFIG_SPI_OA_EN)
adi_eth_Result_e    adin2111_SetChunkSize            (adin2111_DeviceHandle_t hDevice, adi_mac_OaCps_e cps);
adi_eth_Result_e    adin2111_GetChunkSize            (adin2111_DeviceHandle_t hDevice, adi_mac_OaCps_e *pCps);
#endif
adi_eth_Result_e    adin2111_SetCutThroughMode       (adin2111_DeviceHandle_t hDevice, bool txcte, bool rxcte, bool p2pcte);
adi_eth_Result_e    adin2111_GetCutThroughMode       (adin2111_DeviceHandle_t hDevice, bool *pTxcte, bool *pRxcte, bool *pP2pcte);
adi_eth_Result_e    adin2111_SetFifoSizes            (adin2111_DeviceHandle_t hDevice, adi_mac_FifoSizes_t fifoSizes);
adi_eth_Result_e    adin2111_GetFifoSizes            (adin2111_DeviceHandle_t hDevice, adi_mac_FifoSizes_t *pFifoSizes);
adi_eth_Result_e    adin2111_ClearFifos              (adin2111_DeviceHandle_t hDevice, adi_mac_FifoClrMode_e clearMode);

adi_eth_Result_e    adin2111_TsEnable                (adin2111_DeviceHandle_t hDevice, adi_mac_TsFormat_e format);
adi_eth_Result_e    adin2111_TsClear                 (adin2111_DeviceHandle_t hDevice);
adi_eth_Result_e    adin2111_TsTimerStart            (adin2111_DeviceHandle_t hDevice, adi_mac_TsTimerConfig_t *pTimerConfig);
adi_eth_Result_e    adin2111_TsTimerStop             (adin2111_DeviceHandle_t hDevice);
adi_eth_Result_e    adin2111_TsSetTimerAbsolute      (adin2111_DeviceHandle_t hDevice, uint32_t seconds, uint32_t nanoseconds);
adi_eth_Result_e    adin2111_TsSyncClock             (adin2111_DeviceHandle_t hDevice, int64_t tError, uint64_t referenceTimeNsDiff, uint64_t localTimeNsDiff);
adi_eth_Result_e    adin2111_TsGetEgressTimestamp    (adin2111_DeviceHandle_t hDevice, adi_mac_EgressCapture_e egressReg, adi_mac_TsTimespec_t *pCapturedTimespec);
adi_eth_Result_e    adin2111_TsConvert               (uint32_t timestampLowWord, uint32_t timestampHighWord, adi_mac_TsFormat_e format, adi_mac_TsTimespec_t *pTimespec);
int64_t             adin2111_TsSubtract              (adi_mac_TsTimespec_t *pTsA, adi_mac_TsTimespec_t *pTsB);

adi_eth_Result_e    adin2111_RegisterCallback       (adin2111_DeviceHandle_t hDevice, adi_eth_Callback_t cbFunc, adi_mac_InterruptEvt_e cbEvent);

adi_eth_Result_e    adin2111_WriteRegister           (adin2111_DeviceHandle_t hDevice, uint16_t regAddr, uint32_t regData);
adi_eth_Result_e    adin2111_ReadRegister            (adin2111_DeviceHandle_t hDevice, uint16_t regAddr, uint32_t *regData);
adi_eth_Result_e    adin2111_PhyWrite                (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint32_t regAddr, uint16_t regData);
adi_eth_Result_e    adin2111_PhyRead                 (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint32_t regAddr, uint16_t *regData);

adi_eth_Result_e    adin2111_GetMseLinkQuality       (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_MseLinkQuality_t *mseLinkQuality);

adi_eth_Result_e    adin2111_FrameGenEn              (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool enable);
adi_eth_Result_e    adin2111_FrameGenSetMode         (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_FrameGenMode_e mode);
adi_eth_Result_e    adin2111_FrameGenSetFrameCnt     (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint32_t frameCnt);
adi_eth_Result_e    adin2111_FrameGenSetFramePayload (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_FrameGenPayload_e payload);
adi_eth_Result_e    adin2111_FrameGenSetFrameLen     (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint16_t frameLen);
adi_eth_Result_e    adin2111_FrameGenSetIfgLen       (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint16_t ifgLen);
adi_eth_Result_e    adin2111_FrameGenRestart         (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port);
adi_eth_Result_e    adin2111_FrameGenDone            (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool *fgDone);

adi_eth_Result_e    adin2111_FrameChkEn              (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, bool enable);
adi_eth_Result_e    adin2111_FrameChkSourceSelect    (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_FrameChkSource_e source);
adi_eth_Result_e    adin2111_FrameChkReadFrameCnt    (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint32_t *cnt);
adi_eth_Result_e    adin2111_FrameChkReadRxErrCnt    (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, uint16_t *cnt);
adi_eth_Result_e    adin2111_FrameChkReadErrorCnt    (adin2111_DeviceHandle_t hDevice, adin2111_Port_e port, adi_phy_FrameChkErrorCounters_t *cnt);

adi_eth_Result_e    adin2111_AgeTick                 (adin2111_DeviceHandle_t hDevice);
adi_eth_Result_e    adin2111_ClearDynamicTable       (adin2111_DeviceHandle_t hDevice);

/*! @cond PRIVATE */

/*! @endcond */

#ifdef __cplusplus
}
#endif

#endif /* __ADIN2111_H__ */

/** @}*/

