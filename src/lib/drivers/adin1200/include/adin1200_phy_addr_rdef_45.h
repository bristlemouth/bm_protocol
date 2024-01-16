/*
 *---------------------------------------------------------------------------
 *
 * Copyright (c) 2022 Analog Devices, Inc. All Rights Reserved.
 * This software is proprietary to Analog Devices, Inc.
 * and its licensors. By using this software you agree to the terms of the
 * associated Analog Devices Software License Agreement.
 *
 *---------------------------------------------------------------------------
 */
/** @addtogroup phy PHY Definitions
 *  @{
 */

#ifndef ADIN_PHY_ADDR_RDEF_45_H
#define ADIN_PHY_ADDR_RDEF_45_H


#if defined(_LANGUAGE_C) || (defined(__GNUC__) && !defined(__ASSEMBLER__))
#include <stdint.h>
#endif /* _LANGUAGE_C */

/* ====================================================================================================
        ADINPHY  Subsystem Register Address Offset Definitions
   ==================================================================================================== */
#define ADIN1200_ADDR_GE_SFT_RST                              (0XFF0C)  /* Subsystem Software Reset Register */
#define ADIN1200_ADDR_GE_SFT_RST_CFG_EN                       (0XFF0D)  /* Subsystem Software Reset Configuration Enable Register */
#define ADIN1200_ADDR_GE_CLK_CFG                              (0XFF1F)  /* Subsystem Clock Configuration Register */
#define ADIN1200_ADDR_GE_RGMII_CFG                            (0XFF23)  /* Subsystem RGMII Configuration Register */
#define ADIN1200_ADDR_GE_RMII_CFG                             (0XFF24)  /* Subsystem RMII Configuration Register */
#define ADIN1200_ADDR_GE_PHY_BASE_CFG                         (0XFF26)  /* Subsystem PHY Base Configuration Register */
#define ADIN1200_ADDR_GE_LNK_STAT_INV_EN                      (0XFF3C)  /* Subsystem Link Status Invert Enable Register*/
#define ADIN1200_ADDR_GE_IO_GP_CLK_OR_CNTRL                   (0XFF3D)  /* Subsystem GP_CLK Pin Override Control Register */
#define ADIN1200_ADDR_GE_IO_GP_OUT_OR_CNTRL                   (0XFF3E)  /* Subsystem LINK_ST Pin Override Control Register */
#define ADIN1200_ADDR_GE_IO_INT_N_OR_CNTRL                    (0XFF3F)  /* Subsystem INT_N Pin Override Control Register */
#define ADIN1200_ADDR_GE_IO_LED_A_OR_CNTRL                    (0XFF41)  /* Subsystem LED_0 Pin Override Control Register */

/* ====================================================================================================
        ADINPHY Module Register Address Offset Definitions
   ==================================================================================================== */
#define ADIN1200_ADDR_EEE_CAPABILITY                          (0x8000) /* Energy Efficient Ethernet Capability Register */
#define ADIN1200_ADDR_EEE_ADV                                 (0x8001) /* Energy Efficient Ethernet Advertisement Register */
#define ADIN1200_ADDR_EEE_LP_ABILITY                          (0x8002) /* Energy Efficient Ethernet Link Partner Ability Register */
#define ADIN1200_ADDR_EEE_RSLVD                               (0x8008) /* Energy Efficient Ethernet Resolved Register*/
#define ADIN1200_ADDR_MSE_A                                   (0x8402) /* Mean Square Error A Register */
#define ADIN1200_ADDR_FLD_EN                                  (0x8E27) /* Enhanced Link Detection Enable Register */
#define ADIN1200_ADDR_FLD_STAT_LAT                            (0x8E38) /* Enhanced Link Detection Latched Status Register */
#define ADIN1200_ADDR_RX_MII_CLK_STOP_EN                      (0x9400) /* Receive MII Clock Stop Enable Register */
#define ADIN1200_ADDR_PCS_STATUS_1                            (0x9401) /* Physical Coding Sublayer (PCS) Status 1 Register */
#define ADIN1200_ADDR_FC_EN                                   (0x9403) /* Frame Checker Enable Register */
#define ADIN1200_ADDR_FC_IRQ_EN                               (0x9406) /* Frame Checker Interrupt Enable Register */
#define ADIN1200_ADDR_FC_TX_SEL                               (0x9407) /* Frame Checker Transmit Select Register */
#define ADIN1200_ADDR_FC_MAX_FRM_SIZE                         (0x9408) /* Frame Checker Maximum Frame Size Register */
#define ADIN1200_ADDR_FC_FRM_CNT_H                            (0x940A) /* Frame Checker Count High Register */
#define ADIN1200_ADDR_FC_FRM_CNT_L                            (0x940B) /* Frame Checker Count Low Register */
#define ADIN1200_ADDR_FC_LEN_ERR_CNT                          (0x940C) /* Frame Checker Length Error Count Register */
#define ADIN1200_ADDR_FC_ALGN_ERR_CNT                         (0x940D) /* Frame Checker Alignment Error Count Register */
#define ADIN1200_ADDR_FC_SYMB_ERR_CNT                         (0x940E) /* Frame Checker Symbol Error Counter Register */
#define ADIN1200_ADDR_FC_OSZ_CNT                              (0x940F) /* Frame Checker Oversized Frame Count Register */
#define ADIN1200_ADDR_FC_USZ_CNT                              (0x9410) /* Frame Checker Undersized Frame Count Register */
#define ADIN1200_ADDR_FC_ODD_CNT                              (0x9411) /* Frame Checker Odd Nibble Frame Count Register */
#define ADIN1200_ADDR_FC_ODD_PRE_CNT                          (0x9412) /* Frame Checker Odd Preamble Packet Count Register */
#define ADIN1200_ADDR_FC_DRIBBLE_BITS_CNT                     (0x9413) /* Frame Checker Dribble Bits Frame Count Register */
#define ADIN1200_ADDR_FC_FALSE_CARRIER_CNT                    (0x9414) /* Frame Checker False Carrier Count Register */
#define ADIN1200_ADDR_FG_EN                                   (0x9415) /* Frame Generator Enable Register */
#define ADIN1200_ADDR_FG_CNTRL_RSTRT                          (0x9416) /* Frame Generator Control and Restart Register */
#define ADIN1200_ADDR_FG_CONT_MODE_EN                         (0x9417) /* Frame Generator Continuous Mode Enable Register */
#define ADIN1200_ADDR_FG_IRQ_EN                               (0x9418) /* Frame Generator Interrupt Enable Register */
#define ADIN1200_ADDR_FG_FRM_LEN                              (0x941A) /* Frame Generator Frame Length Register */
#define ADIN1200_ADDR_FG_IFG_LEN                              (0x941B) /* Frame Generator Interframe Gap Length Register */
#define ADIN1200_ADDR_FG_NFRM_H                               (0x941C) /* Frame Generator Number of Frames High Register */
#define ADIN1200_ADDR_FG_NFRM_L                               (0x941D) /* Frame Generator Number of Frames Low Register */
#define ADIN1200_ADDR_FG_DONE                                 (0x941E) /* Frame Generator Done Register */
#define ADIN1200_ADDR_FIFO_SYNC                               (0x9427) /* FIFO Sync Register */
#define ADIN1200_ADDR_SOP_CTRL                                (0x9428) /* Start of Packet Control Register */
#define ADIN1200_ADDR_SOP_RX_DEL                              (0x9429) /* Start of Packet Receive Detection Delay Register */
#define ADIN1200_ADDR_SOP_TX_DEL                              (0x942A) /* Start of Packet Transmit Detection Delay Register */
#define ADIN1200_ADDR_DPTH_MII_BYTE                           (0x9602) /* Control of FIFO Depth for MII Modes Register */
#define ADIN1200_ADDR_LPI_WAKE_ERR_CNT                        (0xA000) /* Wake Error Count Register */
#define ADIN1200_ADDR_B_10_E_EN                               (0xB403) /* Base 10e Enable Register */
#define ADIN1200_ADDR_B_10_TX_TST_MODE                        (0xB412) /* 10BASE-T Transmit Test Mode Register*/
#define ADIN1200_ADDR_B_100_TX_TST_MODE                       (0xB413) /* 100BASE-TX Transmit Test Mode Register*/
#define ADIN1200_ADDR_CDIAG_RUN                               (0xBA1B) /* Run Automated Cable Diagnostics Register */
#define ADIN1200_ADDR_CDIAG_XPAIR_DIS                         (0xBA1C) /* Cable Diagnostics Cross Pair Fault Checking Disable Register */
#define ADIN1200_ADDR_CDIAG_DTLD_RSLTS_0                      (0xBA1D) /* Cable Diagnostics Results 0 Register */
#define ADIN1200_ADDR_CDIAG_DTLD_RSLTS_1                      (0xBA1E) /* Cable Diagnostics Results 1 Register */
#define ADIN1200_ADDR_CDIAG_FLT_DIST_0                        (0xBA21) /* Cable Diagnostics Fault Distance Pair 0 Register */
#define ADIN1200_ADDR_CDIAG_FLT_DIST_1                        (0xBA22) /* Cable Diagnostics Fault Distance Pair 1 Register */
#define ADIN1200_ADDR_CDIAG_CBL_LEN_EST                       (0xBA25) /* Cable Diagnostics Cable Length Estimate Register */
#define ADIN1200_ADDR_LED_PUL_STR_DUR                         (0xBC00) /* LED Pulse Stretching Duration Register */

/* ====================================================================================================
        ADINPHY 1G Subsystem Register Address Offset Definitions
   ==================================================================================================== */
#define ADIN1200_ADDR_GE_IRQ_EN                               (0xFF1D)  /* Subsystem Interrupt Enable Register */
#define ADIN1200_ADDR_GE_IRQ_LAT                              (0xFF27)  /* Subsystem Interrupt Status Register */
#define ADIN1200_ADDR_GE_PHY_IF_CFG                           (0xFF1E)  /* Subsystem Gigabit Ethernet PHY Interface Control Register */
#define ADIN1200_ADDR_GE_B10_REGEN_PRE                        (0xFF38)  /* Subsystem 10BASE-T Preamble Generation Register */
#define ADIN1200_ADDR_GE_RGMII_IO_CNTRL                       (0xFF3A)  /* Subsystem RGMII I/O Control Register */
#define ADIN1200_ADDR_GE_CLK_IO_CNTRL                         (0xFF3B)  /* Subsystem Clock I/O Control Register */

/* ====================================================================================================
        ADINPHY 1G Module Register Address Offset Definitions
   ==================================================================================================== */
#define ADIN1200_ADDR_MSE_B                                   (0x8403)  /* Mean Square Error B Register */
#define ADIN1200_ADDR_MSE_C                                   (0x8404)  /* Mean Square Error C Register */
#define ADIN1200_ADDR_B_1000_RTRN_EN                          (0xA001)  /* Base 1000 Retrain Enable Register */
#define ADIN1200_ADDR_CDIAG_DTLD_RSLTS_2                      (0xBA1F)  /* Cable Diagnostics Results 2 Register */
#define ADIN1200_ADDR_CDIAG_DTLD_RSLTS_3                      (0xBA20)  /* Cable Diagnostics Results 3 Register.*/
#define ADIN1200_ADDR_CDIAG_FLT_DIST_2                        (0xBA23)  /* Cable Diagnostics Fault Distance Pair 2 Register */
#define ADIN1200_ADDR_CDIAG_FLT_DIST_3                        (0xBA24)  /* Cable Diagnostics Fault Distance Pair 3 Register */
#define ADIN1200_ADDR_LED_A_INV_EN LED                        (0xBC01)  /* Active Low Status Register */

/* ====================================================================================================
        ADINPHY Subsystem Register ResetValue Definitions
   ==================================================================================================== */
#define ADIN1200_RSTVAL_GE_SFT_RST                            (0X0000)
#define ADIN1200_RSTVAL_GE_SFT_RST_CFG_EN                     (0X0000)
#define ADIN1200_RSTVAL_GE_CLK_CFG                            (0X0000)
#define ADIN1200_RSTVAL_GE_RGMII_CFG                          (0x0E07)
#define ADIN1200_RSTVAL_GE_RMII_CFG                           (0x0116)
#define ADIN1200_RSTVAL_GE_PHY_BASE_CFG                       (0x0C86)
#define ADIN1200_RSTVAL_GE_LNK_STAT_INV_EN                    (0X0000)
#define ADIN1200_RSTVAL_GE_IO_GP_CLK_OR_CNTRL                 (0X0000)
#define ADIN1200_RSTVAL_GE_IO_GP_OUT_OR_CNTRL                 (0X0000)
#define ADIN1200_RSTVAL_GE_IO_INT_N_OR_CNTRL                  (0X0000)
#define ADIN1200_RSTVAL_GE_IO_LED_A_OR_CNTRL                  (0X0000)

#define ADIN1200_RSTVAL_EEE_CAPABILITY                        (0x0006)
#define ADIN1200_RSTVAL_EEE_ADV                               (0x0000)
#define ADIN1200_RSTVAL_EEE_LP_ABILITY                        (0x0000)
#define ADIN1200_RSTVAL_EEE_RSLVD                             (0x0000)
#define ADIN1200_RSTVAL_MSE_A                                 (0x0000)
#define ADIN1200_RSTVAL_FLD_EN                                (0x003D)
#define ADIN1200_RSTVAL_FLD_STAT_LAT                          (0x0000)
#define ADIN1200_RSTVAL_RX_MII_CLK_STOP_EN                    (0x0400)
#define ADIN1200_RSTVAL_PCS_STATUS_1                          (0x0040)
#define ADIN1200_RSTVAL_FC_EN                                 (0x0001)
#define ADIN1200_RSTVAL_FC_IRQ_EN                             (0x0001)
#define ADIN1200_RSTVAL_FC_TX_SEL                             (0x0000)
#define ADIN1200_RSTVAL_FC_MAX_FRM_SIZE                       (0x05F2)
#define ADIN1200_RSTVAL_FC_FRM_CNT_H                          (0x0000)
#define ADIN1200_RSTVAL_FC_FRM_CNT_L                          (0x0000)
#define ADIN1200_RSTVAL_FC_LEN_ERR_CNT                        (0x0000)
#define ADIN1200_RSTVAL_FC_ALGN_ERR_CNT                       (0x0000)
#define ADIN1200_RSTVAL_FC_SYMB_ERR_CNT                       (0x0000)
#define ADIN1200_RSTVAL_FC_OSZ_CNT                            (0x0000)
#define ADIN1200_RSTVAL_FC_USZ_CNT                            (0x0000)
#define ADIN1200_RSTVAL_FC_ODD_CNT                            (0x0000)
#define ADIN1200_RSTVAL_FC_ODD_PRE_CNT                        (0x0000)
#define ADIN1200_RSTVAL_FC_DRIBBLE_BITS_CNT                   (0x0000)
#define ADIN1200_RSTVAL_FC_FALSE_CARRIER_CNT                  (0x0000)
#define ADIN1200_RSTVAL_FG_EN                                 (0x0000)
#define ADIN1200_RSTVAL_FG_CNTRL_RSTRT                        (0x0001)
#define ADIN1200_RSTVAL_FG_CONT_MODE_EN                       (0x0000)
#define ADIN1200_RSTVAL_FG_IRQ_EN                             (0x0000)
#define ADIN1200_RSTVAL_FG_FRM_LEN                            (0x006B)
#define ADIN1200_RSTVAL_FG_IFG_LEN                            (0x000C)
#define ADIN1200_RSTVAL_FG_NFRM_H                             (0x0000)
#define ADIN1200_RSTVAL_FG_NFRM_L                             (0x0100)
#define ADIN1200_RSTVAL_FG_DONE                               (0x0000)
#define ADIN1200_RSTVAL_FIFO_SYNC                             (0x0000)
#define ADIN1200_RSTVAL_SOP_CTRL                              (0x0034)
#define ADIN1200_RSTVAL_SOP_RX_DEL                            (0x0000)
#define ADIN1200_RSTVAL_SOP_TX_DEL                            (0x0000)
#define ADIN1200_RSTVAL_DPTH_MII_BYTE                         (0x0001)
#define ADIN1200_RSTVAL_LPI_WAKE_ERR_CNT LPI                  (0x0000)
#define ADIN1200_RSTVAL_B_10_E_EN                             (0x0001)
#define ADIN1200_RSTVAL_B_10_TX_TST_MODE                      (0x0000)
#define ADIN1200_RSTVAL_B_100_TX_TST_MODE                     (0x0000)
#define ADIN1200_RSTVAL_CDIAG_RUN                             (0x0000)
#define ADIN1200_RSTVAL_CDIAG_XPAIR_DIS                       (0x0000)
#define ADIN1200_RSTVAL_CDIAG_DTLD_RSLTS_0                    (0x0000)
#define ADIN1200_RSTVAL_CDIAG_DTLD_RSLTS_1                    (0x0000)
#define ADIN1200_RSTVAL_CDIAG_FLT_DIST_0                      (0x00FF)
#define ADIN1200_RSTVAL_CDIAG_FLT_DIST_1                      (0x00FF)
#define ADIN1200_RSTVAL_CDIAG_CBL_LEN_EST                     (0x00FF)
#define ADIN1200_RSTVAL_LED_PUL_STR_DUR                       (0x0011)


/* ====================================================================================================
        ADINPHY 1G Subsystem Register ResetValue Definitions
   ==================================================================================================== */
#define ADIN1200_RSTVAL_GE_IRQ_EN                             (0x000E)
#define ADIN1200_RSTVAL_GE_IRQ_LAT                            (0x0000)
#define ADIN1200_RSTVAL_GE_PHY_IF_CFG                         (0x0002)
#define ADIN1200_RSTVAL_GE_B10_REGEN_PRE                      (0x0000)
#define ADIN1200_RSTVAL_GE_RGMII_IO_CNTRL                     (0x0005)
#define ADIN1200_RSTVAL_GE_CLK_IO_CNTRL                       (0x0002)

/* ====================================================================================================
        ADINPHY 1G Module Register Address ResetValue Definitions
   ==================================================================================================== */
#define ADIN1200_RSTVAL_MSE_B                                 (0X0000)
#define ADIN1200_RSTVAL_MSE_B                                 (0X0000)
#define ADIN1200_RSTVAL_B_1000_RTRN_EN                        (0X0000)
#define ADIN1200_RSTVAL_CDIAG_DTLD_RSLTS_2                    (0X0000)
#define ADIN1200_RSTVAL_CDIAG_DTLD_RSLTS_3                    (0X0000)
#define ADIN1200_RSTVAL_CDIAG_FLT_DIST_2                      (0x00FF)
#define ADIN1200_RSTVAL_CDIAG_FLT_DIST_3                      (0x00FF)
#define ADIN1200_RSTVAL_LED_A_INV_EN LED                      (0X0000)

/* ====================================================================================================
        ADINPHY Subsystem Register BitPositions, Lengths, Masks and Enumerations Definitions
   ==================================================================================================== */

/* ----------------------------------------------------------------------------------------------------
          GE_SFT_RST                                  Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_SFT_RST_GE_SFT_RST                    (0U)                /* GE_SFT_RST */
#define ADIN1200_BITL_GE_SFT_RST_GE_SFT_RST                    (1U)
#define ADIN1200_BITM_GE_SFT_RST_GE_SFT_RST                    (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          GE_SFT_RST_CFG_EN                            Value              Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_SFT_RST_CFG_EN_GE_SFT_RST_CFG_EN      (0U)                /* GE_SFT_RST_CFG_EN */
#define ADIN1200_BITL_GE_SFT_RST_CFG_EN_GE_SFT_RST_CFG_EN      (1U)
#define ADIN1200_BITM_GE_SFT_RST_CFG_EN_GE_SFT_RST_CFG_EN      (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          GE_CLK_CFG                                  Value               Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_CLK_CFG_GE_CLK_RCVR_125_EN            (5U)                /* GE_CLK_RCVR_125_EN */
#define ADIN1200_BITL_GE_CLK_CFG_GE_CLK_RCVR_125_EN            (1U)
#define ADIN1200_BITM_GE_CLK_CFG_GE_CLK_RCVR_125_EN            (0X0020U)

#define ADIN1200_BITP_GE_CLK_CFG_GE_CLK_FREE_125_EN            (4U)                /* GE_CLK_FREE_125_EN */
#define ADIN1200_BITL_GE_CLK_CFG_GE_CLK_FREE_125_EN            (1U)
#define ADIN1200_BITM_GE_CLK_CFG_GE_CLK_FREE_125_EN            (0X0010U)

#define ADIN1200_BITP_GE_CLK_CFG_GE_REF_CLK_EN                 (3U)                /* GE_REF_CLK_EN */
#define ADIN1200_BITL_GE_CLK_CFG_GE_REF_CLK_EN                 (1U)
#define ADIN1200_BITM_GE_CLK_CFG_GE_REF_CLK_EN                 (0X0008U)

#define ADIN1200_BITP_GE_CLK_CFG_GE_CLK_HRT_RCVR_EN            (2U)                /* GE_CLK_HRT_RCVR_EN */
#define ADIN1200_BITL_GE_CLK_CFG_GE_CLK_HRT_RCVR_EN            (1U)
#define ADIN1200_BITM_GE_CLK_CFG_GE_CLK_HRT_RCVR_EN            (0X0004U)

#define ADIN1200_BITP_GE_CLK_CFG_GE_CLK_HRT_FREE_EN            (1U)                /* GE_CLK_HRT_FREE_EN */
#define ADIN1200_BITL_GE_CLK_CFG_GE_CLK_HRT_FREE_EN            (1U)
#define ADIN1200_BITM_GE_CLK_CFG_GE_CLK_HRT_FREE_EN            (0X0002U)

#define ADIN1200_BITP_GE_CLK_CFG_GE_CLK_25_EN                  (0U)                /* GE_CLK_25_EN */
#define ADIN1200_BITL_GE_CLK_CFG_GE_CLK_25_EN                  (1U)
#define ADIN1200_BITM_GE_CLK_CFG_GE_CLK_25_EN                  (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          GE_RGMII_CFG                                 Value              Description
   ---------------------------------------------------------------------------------------------------- */

#define ADIN1200_BITP_GE_RGMII_CFG_GE_RGMII_100_LOW_LTNCY_EN   (10U)               /* GE_RGMII_100_LOW_LTNCY_EN */
#define ADIN1200_BITL_GE_RGMII_CFG_GE_RGMII_100_LOW_LTNCY_EN   (1U)
#define ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_100_LOW_LTNCY_EN   (0X0400U)

#define ADIN1200_BITP_GE_RGMII_CFG_GE_RGMII_10_LOW_LTNCY_EN    (9U)                /* GE_RGMII_10_LOW_LTNCY_EN */
#define ADIN1200_BITL_GE_RGMII_CFG_GE_RGMII_10_LOW_LTNCY_EN    (1U)
#define ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_10_LOW_LTNCY_EN    (0X0200U)

#define ADIN1200_BITP_GE_RGMII_CFG_GE_RGMII_RX_SEL             (6U)                /* GE_RGMII_RX_SEL */
#define ADIN1200_BITL_GE_RGMII_CFG_GE_RGMII_RX_SEL             (3U)
#define ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_RX_SEL             (0X01C0U)

#define ADIN1200_BITP_GE_RGMII_CFG_GE_RGMII_GTX_SEL            (3U)                /* GE_RGMII_GTX_SEL */
#define ADIN1200_BITL_GE_RGMII_CFG_GE_RGMII_GTX_SEL            (3U)
#define ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_GTX_SEL            (0X0038U)

#define ADIN1200_BITP_GE_RGMII_CFG_GE_RGMII_RX_ID_EN           (2U)                /* GE_RGMII_RX_ID_EN */
#define ADIN1200_BITL_GE_RGMII_CFG_GE_RGMII_RX_ID_EN           (1U)
#define ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_RX_ID_EN           (0X0004U)

#define ADIN1200_BITP_GE_RGMII_CFG_GE_RGMII_TX_ID_EN           (1U)                /* GE_RGMII_TX_ID_EN */
#define ADIN1200_BITL_GE_RGMII_CFG_GE_RGMII_TX_ID_EN           (1U)
#define ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_TX_ID_EN           (0X0002U)

#define ADIN1200_BITP_GE_RGMII_CFG_GE_RGMII_EN                 (0U)                /* GE_RGMII_EN */
#define ADIN1200_BITL_GE_RGMII_CFG_GE_RGMII_EN                 (1U)
#define ADIN1200_BITM_GE_RGMII_CFG_GE_RGMII_EN                 (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          GE_RMII_CFG                                 Value               Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_RMII_CFG_GE_RMII_FIFO_RST             (7U)                /* GE_RMII_FIFO_RST */
#define ADIN1200_BITL_GE_RMII_CFG_GE_RMII_FIFO_RST             (1U)
#define ADIN1200_BITM_GE_RMII_CFG_GE_RMII_FIFO_RST             (0X0080U)

#define ADIN1200_BITP_GE_RMII_CFG_GE_RMII_FIFO_DPTH            (4U)                /* GE_RMII_FIFO_DPTH */
#define ADIN1200_BITL_GE_RMII_CFG_GE_RMII_FIFO_DPTH            (3U)
#define ADIN1200_BITM_GE_RMII_CFG_GE_RMII_FIFO_DPTH            (0X0070U)

#define ADIN1200_BITP_GE_RMII_CFG_GE_RMII_TXD_CHK_EN           (3U)                /* GE_RMII_TXD_CHK_EN */
#define ADIN1200_BITL_GE_RMII_CFG_GE_RMII_TXD_CHK_EN           (1U)
#define ADIN1200_BITM_GE_RMII_CFG_GE_RMII_TXD_CHK_EN           (0X0008U)

#define ADIN1200_BITP_GE_RMII_CFG_GE_RMII_CRS_EN               (2U)                /* GE_RMII_CRS_EN */
#define ADIN1200_BITL_GE_RMII_CFG_GE_RMII_CRS_EN               (1U)
#define ADIN1200_BITM_GE_RMII_CFG_GE_RMII_CRS_EN               (0X0004U)

#define ADIN1200_BITP_GE_RMII_CFG_GE_RMII_BAD_SSD_RX_ER_EN     (1U)                /* GE_RMII_BAD_SSD_RX_ER_EN */
#define ADIN1200_BITL_GE_RMII_CFG_GE_RMII_BAD_SSD_RX_ER_EN     (1U)
#define ADIN1200_BITM_GE_RMII_CFG_GE_RMII_BAD_SSD_RX_ER_EN     (0X0002U)

#define ADIN1200_BITP_GE_RMII_CFG_GE_RMII_EN                   (0U)                /* GE_RMII_EN */
#define ADIN1200_BITL_GE_RMII_CFG_GE_RMII_EN                   (1U)
#define ADIN1200_BITM_GE_RMII_CFG_GE_RMII_EN                   (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          GE_PHY_BASE_CFG                              Value               Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_PHY_BASE_CFG_GE_RTRN_EN_CFG           (12U)               /* GE_RTRN_EN_CFG - only in ADINPHY 1G */
#define ADIN1200_BITL_GE_PHY_BASE_CFG_GE_RTRN_EN_CFG           (1U)
#define ADIN1200_BITM_GE_PHY_BASE_CFG_GE_RTRN_EN_CFG           (0X1000U)

#define ADIN1200_BITP_GE_PHY_BASE_CFG_GE_FLD_1000_EN_CFG       (11U)               /* GE_FLD_1000_EN_CFG - only in ADINPHY 1G*/
#define ADIN1200_BITL_GE_PHY_BASE_CFG_GE_FLD_1000_EN_CFG       (1U)
#define ADIN1200_BITM_GE_PHY_BASE_CFG_GE_FLD_1000_EN_CFG       (0X0800U)

#define ADIN1200_BITP_GE_PHY_BASE_CFG_GE_FLD_100_EN_CFG        (10U)               /* GE_FLD_100_EN_CFG */
#define ADIN1200_BITL_GE_PHY_BASE_CFG_GE_FLD_100_EN_CFG        (1U)
#define ADIN1200_BITM_GE_PHY_BASE_CFG_GE_FLD_100_EN_CFG        (0X0400U)

#define ADIN1200_BITP_GE_PHY_BASE_CFG_GE_MAN_MDI_FLIP_CFG      (4U)                /* GE_MAN_MDI_FLIP_CFG - only in ADINPHY 1G*/
#define ADIN1200_BITL_GE_PHY_BASE_CFG_GE_MAN_MDI_FLIP_CFG      (1U)
#define ADIN1200_BITM_GE_PHY_BASE_CFG_GE_MAN_MDI_FLIP_CFG      (0X0010U)

#define ADIN1200_BITP_GE_PHY_BASE_CFG_GE_PHY_SFT_PD_CFG        (3U)                /* GE_PHY_SFT_PD_CFG */
#define ADIN1200_BITL_GE_PHY_BASE_CFG_GE_PHY_SFT_PD_CFG        (1U)
#define ADIN1200_BITM_GE_PHY_BASE_CFG_GE_PHY_SFT_PD_CFG        (0X0008U)

/* ----------------------------------------------------------------------------------------------------
          GE_LNK_STAT_INV_EN                               Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_LNK_STAT_INV_EN_GE_LNK_STAT_INV_EN         (0U)           /* GE_LNK_STAT_INV_EN */
#define ADIN1200_BITL_GE_LNK_STAT_INV_EN_GE_LNK_STAT_INV_EN         (1U)
#define ADIN1200_BITM_GE_LNK_STAT_INV_EN_GE_LNK_STAT_INV_EN         (0X0001U)
/* ----------------------------------------------------------------------------------------------------
          GE_IO_GP_CLK_OR_CNTRL                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_IO_GP_CLK_OR_CNTRL_GE_IO_GP_CLK_OR_CNTRL   (0U)           /* GE_IO_GP_CLK_OR_CNTRL */
#define ADIN1200_BITL_GE_IO_GP_CLK_OR_CNTRL_GE_IO_GP_CLK_OR_CNTRL   (3U)
#define ADIN1200_BITM_GE_IO_GP_CLK_OR_CNTRL_GE_IO_GP_CLK_OR_CNTRL   (0X0007U)

/* ----------------------------------------------------------------------------------------------------
          GE_IO_GP_OUT_OR_CNTRL                            Value              Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_IO_GP_OUT_OR_CNTRL_GE_IO_GP_OUT_OR_CNTRL   (0U)           /* GE_IO_GP_OUT_OR_CNTRL */
#define ADIN1200_BITL_GE_IO_GP_OUT_OR_CNTRL_GE_IO_GP_OUT_OR_CNTRL   (3U)
#define ADIN1200_BITM_GE_IO_GP_OUT_OR_CNTRL_GE_IO_GP_OUT_OR_CNTRL   (0X0007U)

/* ----------------------------------------------------------------------------------------------------
          GE_IO_INT_N_OR_CNTRL                             Value              Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_IO_INT_N_OR_CNTRL_GE_IO_INT_N_OR_CNTRL     (0U)           /* GE_IO_INT_N_OR_CNTRL */
#define ADIN1200_BITL_GE_IO_INT_N_OR_CNTRL_GE_IO_INT_N_OR_CNTRL     (3U)
#define ADIN1200_BITM_GE_IO_INT_N_OR_CNTRL_GE_IO_INT_N_OR_CNTRL     (0X0007U)

/* ----------------------------------------------------------------------------------------------------
          GE_IO_LED_A_OR_CNTRL                             Value               Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_IO_LED_A_OR_CNTRL_GE_IO_LED_A_OR_CNTRL     (0U)           /* GE_IO_LED_A_OR_CNTRL */
#define ADIN1200_BITL_GE_IO_LED_A_OR_CNTRL_GE_IO_LED_A_OR_CNTRL     (4U)
#define ADIN1200_BITM_GE_IO_LED_A_OR_CNTRL_GE_IO_LED_A_OR_CNTRL     (0X000FU)


/* ====================================================================================================
   ADINPHY 1G Subsystem Register BitPositions, Lengths, Masks and Enumerations Definitions
   ==================================================================================================== */

/* ----------------------------------------------------------------------------------------------------
          GE_IRQ_EN                                        Value                Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_IRQ_EN_GE_HRD_RST_IRQ_EN                   (4U)                /* GE_IRQ_EN */
#define ADIN1200_BITL_GE_IRQ_EN_GE_HRD_RST_IRQ_EN                   (1U)
#define ADIN1200_BITM_GE_IRQ_EN_GE_HRD_RST_IRQ_EN                   (0X0010U)

/* ----------------------------------------------------------------------------------------------------
          GE_IRQ_LAT                                       Value                Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_IRQ_LAT_GE_HRD_RST_IRQ_LH                   (4U)                /* GE_IRQ_LAT */
#define ADIN1200_BITL_GE_IRQ_LAT_GE_HRD_RST_IRQ_LH                   (1U)
#define ADIN1200_BITM_GE_IRQ_LAT_GE_HRD_RST_IRQ_LH                   (0X0010U)

/* ----------------------------------------------------------------------------------------------------
          GE_PHY_IF_CFG                                    Value               Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_PHY_IF_CFG_GE_FIFO_DPTH                    (2U)              /* GE_PHY_IF_CFG */
#define ADIN1200_BITL_GE_PHY_IF_CFG_GE_FIFO_DPTH                    (3U)
#define ADIN1200_BITM_GE_PHY_IF_CFG_GE_FIFO_DPTH                    (0X001CU)

/* ----------------------------------------------------------------------------------------------------
          GE_B10_REGEN_PRE                                 Value           Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_B10_REGEN_PRE_GE_B10_REGEN_PRE             (0U)           /* GE_B10_REGEN_PRE */
#define ADIN1200_BITL_GE_B10_REGEN_PRE_GE_B10_REGEN_PRE             (1U)
#define ADIN1200_BITM_GE_B10_REGEN_PRE_GE_B10_REGEN_PRE             (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          GE_RGMII_IO_CNTRL                                Value            Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_RGMII_IO_CNTRL_GE_RGMII_RX_DRV             (2U)          /* GE_RGMII_IO_CNTRL */
#define ADIN1200_BITL_GE_RGMII_IO_CNTRL_GE_RGMII_RX_DRV             (2U)
#define ADIN1200_BITM_GE_RGMII_IO_CNTRL_GE_RGMII_RX_DRV             (0X000CU)
/* ----------------------------------------------------------------------------------------------------
          GE_CLK_IO_CNTRL                                  Value           Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_GE_CLK_IO_CNTRL_GE_CLK_DRV                    (1U)          /* GE_CLK_IO_CNTRL */
#define ADIN1200_BITL_GE_CLK_IO_CNTRL_GE_CLK_DRV                    (2U)
#define ADIN1200_BITM_GE_CLK_IO_CNTRL_GE_CLK_DRV                    (0X0006U)

/* ====================================================================================================
   ADINPHY 1G Module Register BitPositions, Lengths, Masks and Enumerations Definitions
   ==================================================================================================== */

/* ----------------------------------------------------------------------------------------------------
          EEE_CAPABILITY                              Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_EEE_CAPABILITY_EEE_10_G_KR_SPRT          (6U)                /* EEE_10_G_KR_SPRT */
#define ADIN1200_BITL_EEE_CAPABILITY_EEE_10_G_KR_SPRT          (1U)
#define ADIN1200_BITM_EEE_CAPABILITY_EEE_10_G_KR_SPRT          (0X0040U)

#define ADIN1200_BITP_EEE_CAPABILITY_EEE_10_G_KX_4_SPRT        (5U)                /* EEE_10_G_KX_4_SPRT */
#define ADIN1200_BITL_EEE_CAPABILITY_EEE_10_G_KX_4_SPRT        (1U)
#define ADIN1200_BITM_EEE_CAPABILITY_EEE_10_G_KX_4_SPRT        (0X0020U)

#define ADIN1200_BITP_EEE_CAPABILITY_EEE_1000_KX_SPRT          (4U)                /* EEE_1000_KX_SPRT */
#define ADIN1200_BITL_EEE_CAPABILITY_EEE_1000_KX_SPRT          (1U)
#define ADIN1200_BITM_EEE_CAPABILITY_EEE_1000_KX_SPRT          (0X0010U)

#define ADIN1200_BITP_EEE_CAPABILITY_EEE_10_G_SPRT             (3U)                /* EEE_10_G_SPRT */
#define ADIN1200_BITL_EEE_CAPABILITY_EEE_10_G_SPRT             (1U)
#define ADIN1200_BITM_EEE_CAPABILITY_EEE_10_G_SPRT             (0X0008U)

#define ADIN1200_BITP_EEE_CAPABILITY_EEE_1000_SPRT             (2U)                /* EEE_1000_SPRT */
#define ADIN1200_BITL_EEE_CAPABILITY_EEE_1000_SPRT             (1U)
#define ADIN1200_BITM_EEE_CAPABILITY_EEE_1000_SPRT             (0X0004U)

#define ADIN1200_BITP_EEE_CAPABILITY_EEE_100_SPRT              (1U)                /* GE_EEE_100_SPRTSFT_RST */
#define ADIN1200_BITL_EEE_CAPABILITY_EEE_100_SPRT              (1U)
#define ADIN1200_BITM_EEE_CAPABILITY_EEE_100_SPRT              (0X0002U)

/* ----------------------------------------------------------------------------------------------------
          EEE_ADV                                     Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_EEE_ADV_EEE_10_G_KR_ADV                  (6U)                /* EEE_10_G_KR_ADV */
#define ADIN1200_BITL_EEE_ADV_EEE_10_G_KR_ADV                  (1U)
#define ADIN1200_BITM_EEE_ADV_EEE_10_G_KR_ADV                  (0X0040U)

#define ADIN1200_BITP_EEE_ADV_EEE_10_G_KX_4_ADV                (5U)                /* EEE_10_G_KX_4_ADV */
#define ADIN1200_BITL_EEE_ADV_EEE_10_G_KX_4_ADV                (1U)
#define ADIN1200_BITM_EEE_ADV_EEE_10_G_KX_4_ADV                (0X0020U)

#define ADIN1200_BITP_EEE_ADV_EEE_1000_KX_ADV                  (4U)                /* EEE_1000_KX_ADV */
#define ADIN1200_BITL_EEE_ADV_EEE_1000_KX_ADV                  (1U)
#define ADIN1200_BITM_EEE_ADV_EEE_1000_KX_ADV                  (0X0010U)

#define ADIN1200_BITP_EEE_ADV_EEE_10_G_ADV                     (3U)                /* EEE_10_G_ADV */
#define ADIN1200_BITL_EEE_ADV_EEE_10_G_ADV                     (1U)
#define ADIN1200_BITM_EEE_ADV_EEE_10_G_ADV                     (0X0008U)

#define ADIN1200_BITP_EEE_ADV_EEE_1000_ADV                     (2U)                /* EEE_1000_ADV */
#define ADIN1200_BITL_EEE_ADV_EEE_1000_ADV                     (1U)
#define ADIN1200_BITM_EEE_ADV_EEE_1000_ADV                     (0X0004U)

#define ADIN1200_BITP_EEE_ADV_EEE_100_ADV                      (1U)                /* EEE_100_ADV */
#define ADIN1200_BITL_EEE_ADV_EEE_100_ADV                      (1U)
#define ADIN1200_BITM_EEE_ADV_EEE_100_ADV                      (0X0002U)

/* ----------------------------------------------------------------------------------------------------
          EEE_LP_ABILITY                              Value                   Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_EEE_LP_ABILITY_LP_EEE_10_G_KR_ABLE       (6U)                /* LP_EEE_10_G_KR_ABLE */
#define ADIN1200_BITL_EEE_LP_ABILITY_LP_EEE_10_G_KR_ABLE       (1U)
#define ADIN1200_BITM_EEE_LP_ABILITY_LP_EEE_10_G_KR_ABLE       (0X0040U)

#define ADIN1200_BITP_EEE_LP_ABILITY_LP_EEE_10_G_KX_4_ABLE     (5U)                /* LP_EEE_10_G_KX_4_ABLE */
#define ADIN1200_BITL_EEE_LP_ABILITY_LP_EEE_10_G_KX_4_ABLE     (1U)
#define ADIN1200_BITM_EEE_LP_ABILITY_LP_EEE_10_G_KX_4_ABLE     (0X0020U)

#define ADIN1200_BITP_EEE_LP_ABILITY_LP_EEE_1000_KX_ABLE       (4U)                /* LP_EEE_1000_KX_ABLE */
#define ADIN1200_BITL_EEE_LP_ABILITY_LP_EEE_1000_KX_ABLE       (1U)
#define ADIN1200_BITM_EEE_LP_ABILITY_LP_EEE_1000_KX_ABLE       (0X0010U)

#define ADIN1200_BITP_EEE_LP_ABILITY_LP_EEE_10_G_ABLE          (3U)                /* LP_EEE_10_G_ABLE */
#define ADIN1200_BITL_EEE_LP_ABILITY_LP_EEE_10_G_ABLE          (1U)
#define ADIN1200_BITM_EEE_LP_ABILITY_LP_EEE_10_G_ABLE          (0X0008U)

#define ADIN1200_BITP_EEE_LP_ABILITY_LP_EEE_1000_ABLE          (2U)                /* GE_SFTLP_EEE_1000_ABLE _RST */
#define ADIN1200_BITL_EEE_LP_ABILITY_LP_EEE_1000_ABLE          (1U)
#define ADIN1200_BITM_EEE_LP_ABILITY_LP_EEE_1000_ABLE          (0X0004U)

#define ADIN1200_BITP_EEE_LP_ABILITY_LP_EEE_100_ABLE           (1U)                /* LP_EEE_100_ABLE */
#define ADIN1200_BITL_EEE_LP_ABILITY_LP_EEE_100_ABLE           (1U)
#define ADIN1200_BITM_EEE_LP_ABILITY_LP_EEE_100_ABLE           (0X0002U)

/* ----------------------------------------------------------------------------------------------------
          EEE_RSLVD                                    Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_EEE_RSLVD_EEE_RSLVD                      (0U)                /* EEE_RSLVD */
#define ADIN1200_BITL_EEE_RSLVD_EEE_RSLVD                      (1U)
#define ADIN1200_BITM_EEE_RSLVD_EEE_RSLVD                      (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          MSE_A                                        Value                Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_MSE_A_MSE_A                              (0U)                /* MSE_A */
#define ADIN1200_BITL_MSE_A_MSE_A                              (8U)
#define ADIN1200_BITM_MSE_A_MSE_A                              (0X00FFU)

/* ----------------------------------------------------------------------------------------------------
          FLD_EN                                      Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FLD_EN_FLD_PCS_ERR_B_100_EN              (7U)                /* FLD_PCS_ERR_B_100_EN */
#define ADIN1200_BITL_FLD_EN_FLD_PCS_ERR_B_100_EN              (1U)
#define ADIN1200_BITM_FLD_EN_FLD_PCS_ERR_B_100_EN              (0X0080U)

#define ADIN1200_BITP_FLD_EN_FLD_PCS_ERR_B_1000_EN             (6U)                /* FLD_PCS_ERR_B_1000_EN - Only in 1G PHY */
#define ADIN1200_BITL_FLD_EN_FLD_PCS_ERR_B_1000_EN             (1U)
#define ADIN1200_BITM_FLD_EN_FLD_PCS_ERR_B_1000_EN             (0X0040U)

#define ADIN1200_BITP_FLD_EN_FLD_SLCR_OUT_STUCK_B_100_EN       (5U)                /* FLD_SLCR_OUT_STUCK_B_100_EN */
#define ADIN1200_BITL_FLD_EN_FLD_SLCR_OUT_STUCK_B_100_EN       (1U)
#define ADIN1200_BITM_FLD_EN_FLD_SLCR_OUT_STUCK_B_100_EN       (0X0020U)

#define ADIN1200_BITP_FLD_EN_FLD_SLCR_OUT_STUCK_B_1000_EN      (4U)                /* FLD_SLCR_OUT_STUCK_B_1000_EN - Only in 1G PHY */
#define ADIN1200_BITL_FLD_EN_FLD_SLCR_OUT_STUCK_B_1000_EN      (1U)
#define ADIN1200_BITM_FLD_EN_FLD_SLCR_OUT_STUCK_B_1000_EN      (0X0010U)

#define ADIN1200_BITP_FLD_EN_FLD_SLCR_IN_ZDET_B_100_EN         (3U)                /* FLD_SLCR_IN_ZDET_B_100_EN */
#define ADIN1200_BITL_FLD_EN_FLD_SLCR_IN_ZDET_B_100_EN         (1U)
#define ADIN1200_BITM_FLD_EN_FLD_SLCR_IN_ZDET_B_100_EN         (0X0008U)

#define ADIN1200_BITP_FLD_EN_FLD_SLCR_IN_ZDET_B_1000_EN        (2U)                /* FLD_SLCR_IN_ZDET_B_1000_EN - Only in 1G PHY */
#define ADIN1200_BITL_FLD_EN_FLD_SLCR_IN_ZDET_B_1000_EN        (1U)
#define ADIN1200_BITM_FLD_EN_FLD_SLCR_IN_ZDET_B_1000_EN        (0X0004U)

#define ADIN1200_BITP_FLD_EN_FLD_SLCR_IN_INVLD_B_100_EN        (1U)                /* FLD_SLCR_IN_INVLD_B_100_EN */
#define ADIN1200_BITL_FLD_EN_FLD_SLCR_IN_INVLD_B_100_EN        (1U)
#define ADIN1200_BITM_FLD_EN_FLD_SLCR_IN_INVLD_B_100_EN        (0X0002U)

#define ADIN1200_BITP_FLD_EN_FLD_SLCR_IN_INVLD_B_1000_EN       (0U)                /* FLD_SLCR_IN_INVLD_B_1000_EN - Only in 1G PHY */
#define ADIN1200_BITL_FLD_EN_FLD_SLCR_IN_INVLD_B_1000_EN       (1U)
#define ADIN1200_BITM_FLD_EN_FLD_SLCR_IN_INVLD_B_1000_EN       (0X0001U)


/* ----------------------------------------------------------------------------------------------------
          FLD_STAT_LAT                                Value                Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FLD_STAT_LAT_FAST_LINK_DOWN_LAT          (13U)               /* FAST_LINK_DOWN_LAT  */
#define ADIN1200_BITL_FLD_STAT_LAT_FAST_LINK_DOWN_LAT          (1U)
#define ADIN1200_BITM_FLD_STAT_LAT_FAST_LINK_DOWN_LAT          (0X2000U)

/* ----------------------------------------------------------------------------------------------------
          RX_MII_CLK_STOP_EN                          Value                Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_RX_MII_CLK_STOP_EN_RX_MII_CLK_STOP_EN    (10U)               /* RX_MII_CLK_STOP_EN */
#define ADIN1200_BITL_RX_MII_CLK_STOP_EN_RX_MII_CLK_STOP_EN    (1U)
#define ADIN1200_BITM_RX_MII_CLK_STOP_EN_RX_MII_CLK_STOP_EN    (0X0400U)

/* ----------------------------------------------------------------------------------------------------
          PCS_STATUS_1                                Value                Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_PCS_STATUS_1_TX_LPI_RCVD                 (11U)               /* TX_LPI_RCVD  */
#define ADIN1200_BITL_PCS_STATUS_1_TX_LPI_RCVD                 (1U)
#define ADIN1200_BITM_PCS_STATUS_1_TX_LPI_RCVD                 (0X0800U)

#define ADIN1200_BITP_PCS_STATUS_1_RX_LPI_RCVD                 (10U)               /* RX_LPI_RCVD */
#define ADIN1200_BITL_PCS_STATUS_1_RX_LPI_RCVD                 (1U)
#define ADIN1200_BITM_PCS_STATUS_1_RX_LPI_RCVD                 (0X0400U)

#define ADIN1200_BITP_PCS_STATUS_1_TX_LPI                      (9U)                /* TX_LPI */
#define ADIN1200_BITL_PCS_STATUS_1_TX_LPI                      (1U)
#define ADIN1200_BITM_PCS_STATUS_1_TX_LPI                      (0X0200U)

#define ADIN1200_BITP_PCS_STATUS_1_RX_LPI                      (8U)                /* RX_LPI */
#define ADIN1200_BITL_PCS_STATUS_1_RX_LPI                      (1U)
#define ADIN1200_BITM_PCS_STATUS_1_RX_LPI                      (0X0100U)

#define ADIN1200_BITP_PCS_STATUS_1_TX_MII_CLK_STOP_CPBL        (6U)                /* TX_MII_CLK_STOP_CPBL  */
#define ADIN1200_BITL_PCS_STATUS_1_TX_MII_CLK_STOP_CPBL        (1U)
#define ADIN1200_BITM_PCS_STATUS_1_TX_MII_CLK_STOP_CPBL        (0X0040U)

/* ----------------------------------------------------------------------------------------------------
          FC_EN                                       Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_EN_FC_EN                              (0U)                /* FC_EN */
#define ADIN1200_BITL_FC_EN_FC_EN                              (1U)
#define ADIN1200_BITM_FC_EN_FC_EN                              (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          FC_IRQ_EN                                   Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_IRQ_EN_FC_IRQ_EN                      (0U)                /* FC_IRQ_EN */
#define ADIN1200_BITL_FC_IRQ_EN_FC_IRQ_EN                      (1U)
#define ADIN1200_BITM_FC_IRQ_EN_FC_IRQ_EN                      (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          FC_TX_SEL                                   Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_TX_SEL_FC_TX_SEL                      (0U)                /* FC_TX_SEL */
#define ADIN1200_BITL_FC_TX_SEL_FC_TX_SEL                      (1U)
#define ADIN1200_BITM_FC_TX_SEL_FC_TX_SEL                      (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          FC_MAX_FRM_SIZE                             Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_MAX_FRM_SIZE_FC_MAX_FRM_SIZE          (0U)                /* FC_MAX_FRM_SIZE */
#define ADIN1200_BITL_FC_MAX_FRM_SIZE_FC_MAX_FRM_SIZE          (15U)
#define ADIN1200_BITM_FC_MAX_FRM_SIZE_FC_MAX_FRM_SIZE          (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          FC_FRM_CNT_H                                Value                Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_FRM_CNT_H_FC_FRM_CNT_H                (0U)                /* FC_FRM_CNT_H */
#define ADIN1200_BITL_FC_FRM_CNT_H_FC_FRM_CNT_H                (15U)
#define ADIN1200_BITM_FC_FRM_CNT_H_FC_FRM_CNT_H                (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          FC_FRM_CNT_L                                Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_FRM_CNT_L_FC_FRM_CNT_L                (0U)                /* FC_FRM_CNT_L */
#define ADIN1200_BITL_FC_FRM_CNT_L_FC_FRM_CNT_L                (15U)
#define ADIN1200_BITM_FC_FRM_CNT_L_FC_FRM_CNT_L                (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          FC_LEN_ERR_CNT                              Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_LEN_ERR_CNT_FC_LEN_ERR_CNT            (0U)                /* FC_LEN_ERR_CNT */
#define ADIN1200_BITL_FC_LEN_ERR_CNT_FC_LEN_ERR_CNT            (15U)
#define ADIN1200_BITM_FC_LEN_ERR_CNT_FC_LEN_ERR_CNT            (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          FC_ALGN_ERR_CNT                             Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_ALGN_ERR_CNT_FC_ALGN_ERR_CNT          (0U)                /* FC_ALGN_ERR_CNT */
#define ADIN1200_BITL_FC_ALGN_ERR_CNT_FC_ALGN_ERR_CNT          (15U)
#define ADIN1200_BITM_FC_ALGN_ERR_CNT_FC_ALGN_ERR_CNT          (0XFFFFU)

/* ---------------------------------------------------------------------------------------------------
          FC_SYMB_ERR_CNT                             Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_SYMB_ERR_CNT_FC_SYMB_ERR_CNT          (0U)                /* FC_SYMB_ERR_CNT */
#define ADIN1200_BITL_FC_SYMB_ERR_CNT_FC_SYMB_ERR_CNT          (15U)
#define ADIN1200_BITM_FC_SYMB_ERR_CNT_FC_SYMB_ERR_CNT          (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          FC_OSZ_CNT                                  Value                   Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_OSZ_CNT_FC_OSZ_CNT                    (0U)                /* FC_OSZ_CNT */
#define ADIN1200_BITL_FC_OSZ_CNT_FC_OSZ_CNT                    (15U)
#define ADIN1200_BITM_FC_OSZ_CNT_FC_OSZ_CNT                    (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          FC_USZ_CNT                                  Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_USZ_CNT_FC_USZ_CNT                    (0U)                /* FC_USZ_CNT */
#define ADIN1200_BITL_FC_USZ_CNT_FC_USZ_CNT                    (15U)
#define ADIN1200_BITM_FC_USZ_CNT_FC_USZ_CNT                    (0XFFFFU)


/* ----------------------------------------------------------------------------------------------------
          FC_ODD_CNT                                  Value                   Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_ODD_CNT_FC_ODD_CNT                    (0U)                  /* FC_ODD_CNT */
#define ADIN1200_BITL_FC_ODD_CNT_FC_ODD_CNT                    (15U)
#define ADIN1200_BITM_FC_ODD_CNT_FC_ODD_CNT                    (0XFFFFU)


/* ----------------------------------------------------------------------------------------------------
          FC_ODD_PRE_CNT                              Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_ODD_PRE_CNT_FC_ODD_PRE_CNT            (0U)                /* FC_ODD_PRE_CNT */
#define ADIN1200_BITL_FC_ODD_PRE_CNT_FC_ODD_PRE_CNT            (15U)
#define ADIN1200_BITM_FC_ODD_PRE_CNT_FC_ODD_PRE_CNT            (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          FC_DRIBBLE_BITS_CNT                              Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_DRIBBLE_BITS_CNT_FC_DRIBBLE_BITS_CNT       (0U)           /* FC_DRIBBLE_BITS_CNT */
#define ADIN1200_BITL_FC_DRIBBLE_BITS_CNT_FC_DRIBBLE_BITS_CNT       (15U)
#define ADIN1200_BITM_FC_DRIBBLE_BITS_CNT_FC_DRIBBLE_BITS_CNT       (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          FC_FALSE_CARRIER_CNT                             Value            Description
   --------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_FALSE_CARRIER_CNT_FC_FALSE_CARRIER_CNT     (0U)           /* FC_FALSE_CARRIER_CNT */
#define ADIN1200_BITL_FC_FALSE_CARRIER_CNT_FC_FALSE_CARRIER_CNT     (15U)
#define ADIN1200_BITM_FC_FALSE_CARRIER_CNT_FC_FALSE_CARRIER_CNT     (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
           FG_EN                                      Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FG_EN_FG_EN                              (0U)                /* FG_EN */
#define ADIN1200_BITL_FG_EN_FG_EN                              (1U)
#define ADIN1200_BITM_FG_EN_FG_EN                              (0X0001U)


/* ----------------------------------------------------------------------------------------------------
          FG_CNTRL_RSTRT                              Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FG_CNTRL_RSTRT_FG_RSTRT                  (3U)                /* FG_RSTRT  */
#define ADIN1200_BITL_FG_CNTRL_RSTRT_FG_RSTRT                  (1U)
#define ADIN1200_BITM_FG_CNTRL_RSTRT_FG_RSTRT                  (0X0008U)

#define ADIN1200_BITP_FG_CNTRL_RSTRT_FG_CNTRL                  (0U)                /* FG_CNTRL  */
#define ADIN1200_BITL_FG_CNTRL_RSTRT_FG_CNTRL                  (3U)
#define ADIN1200_BITM_FG_CNTRL_RSTRT_FG_CNTRL                  (0X0007U)


/* ----------------------------------------------------------------------------------------------------
          FG_CONT_MODE_EN                             Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FG_CONT_MODE_EN_FG_CONT_MODE_EN          (0U)                /* FG_CONT_MODE_EN */
#define ADIN1200_BITL_FG_CONT_MODE_EN_FG_CONT_MODE_EN          (1U)
#define ADIN1200_BITM_FG_CONT_MODE_EN_FG_CONT_MODE_EN          (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          FG_IRQ_EN                                   Value                Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FG_IRQ_EN_FG_IRQ_EN                      (0U)                /* FG_IRQ_EN */
#define ADIN1200_BITL_FG_IRQ_EN_FG_IRQ_EN                      (1U)
#define ADIN1200_BITM_FG_IRQ_EN_FG_IRQ_EN                      (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          FG_FRM_LEN                                  Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FG_FRM_LEN_FG_FRM_LEN                    (0U)                /* FG_FRM_LEN */
#define ADIN1200_BITL_FG_FRM_LEN_FG_FRM_LEN                    (15U)
#define ADIN1200_BITM_FG_FRM_LEN_FG_FRM_LEN                    (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          FG_IFG_LEN                                  Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FG_IFG_LEN_FG_IFG_LEN                    (0U)                /* FG_IFG_LEN */
#define ADIN1200_BITL_FG_IFG_LEN_FG_IFG_LEN                    (8U)
#define ADIN1200_BITM_FG_IFG_LEN_FG_IFG_LEN                    (0X00FFU)

/* ----------------------------------------------------------------------------------------------------
          FG_NFRM_H                                   Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FG_NFRM_H_FG_NFRM_H                      (0U)                /* FG_NFRM_H */
#define ADIN1200_BITL_FG_NFRM_H_FG_NFRM_H                      (15U)
#define ADIN1200_BITM_FG_NFRM_H_FG_NFRM_H                      (0XFFFFU)


/* ----------------------------------------------------------------------------------------------------
          FG_NFRM_L                                   Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FG_NFRM_L_FG_NFRM_L                      (0U)                /* FG_NFRM_L */
#define ADIN1200_BITL_FG_NFRM_L_FG_NFRM_L                      (15U)
#define ADIN1200_BITM_FG_NFRM_L_FG_NFRM_L                      (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          FG_DONE                                     Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FG_DONE_FG_DONE                          (0U)                /* FG_DONE */
#define ADIN1200_BITL_FG_DONE_FG_DONE                          (1U)
#define ADIN1200_BITM_FG_DONE_FG_DONE                          (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          FIFO_SYNC                                   Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FIFO_SYNC_FIFO_SYNC                      (0U)                /* FIFO_SYNC */
#define ADIN1200_BITL_FIFO_SYNC_FIFO_SYNC                      (1U)
#define ADIN1200_BITM_FIFO_SYNC_FIFO_SYNC                      (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          SOP_CTRL                                    Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_SOP_CTRL_SOP_N_8_CYCM_1                  (4U)                /* SOP_CTRL */
#define ADIN1200_BITL_SOP_CTRL_SOP_N_8_CYCM_1                  (3U)
#define ADIN1200_BITM_SOP_CTRL_SOP_N_8_CYCM_1                  (0X0070U)

#define ADIN1200_BITP_SOP_CTRL_SOP_NCYC_EN                     (30U)               /* SOP_NCYC_EN */
#define ADIN1200_BITL_SOP_CTRL_SOP_NCYC_EN                     (1U)
#define ADIN1200_BITM_SOP_CTRL_SOP_NCYC_EN                     (0X0008U)

#define ADIN1200_BITP_SOP_CTRL_SOP_SFD_EN                      (2U)                /* SOP_SFD_EN */
#define ADIN1200_BITL_SOP_CTRL_SOP_SFD_EN                      (1U)
#define ADIN1200_BITM_SOP_CTRL_SOP_SFD_EN                      (0X0004U)

#define ADIN1200_BITP_SOP_CTRL_SOP_RX_EN                       (1U)                /* SOP_RX_EN */
#define ADIN1200_BITL_SOP_CTRL_SOP_RX_EN                       (1U)
#define ADIN1200_BITM_SOP_CTRL_SOP_RX_EN                       (0X0002U)

#define ADIN1200_BITP_SOP_CTRL_SOP_TX_EN                       (0U)                /* SOP_TX_EN */
#define ADIN1200_BITL_SOP_CTRL_SOP_TX_EN                       (1U)
#define ADIN1200_BITM_SOP_CTRL_SOP_TX_EN                       (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          SOP_RX_DEL                                  Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_SOP_RX_DEL_SOP_RX_10_DEL_NCYC            (11U)               /* SOP_RX_10_DEL_NCYC */
#define ADIN1200_BITL_SOP_RX_DEL_SOP_RX_10_DEL_NCYC            (5U)
#define ADIN1200_BITM_SOP_RX_DEL_SOP_RX_10_DEL_NCYC            (0XF800U)

#define ADIN1200_BITP_SOP_RX_DEL_SOP_RX_100_DEL_NCYC           (6U)                /* SOP_RX_100_DEL_NCYC */
#define ADIN1200_BITL_SOP_RX_DEL_SOP_RX_100_DEL_NCYC           (5U)
#define ADIN1200_BITM_SOP_RX_DEL_SOP_RX_100_DEL_NCYC           (0X07C0U)

#define ADIN1200_BITP_SOP_RX_DEL_SOP_RX_1000_DEL_NCYC          (0U)                /* SOP_RX_1000_DEL_NCYC - only for 1G PHY */
#define ADIN1200_BITL_SOP_RX_DEL_SOP_RX_1000_DEL_NCYC          (5U)
#define ADIN1200_BITM_SOP_RX_DEL_SOP_RX_1000_DEL_NCYC          (0X003FU)


/* ----------------------------------------------------------------------------------------------------
          SOP_TX_DEL                                  Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_SOP_TX_DEL_SOP_TX_10_DEL_N_8_NS          (8U)                /* SOP_TX_10_DEL_N_8_NS */
#define ADIN1200_BITL_SOP_TX_DEL_SOP_TX_10_DEL_N_8_NS          (5U)
#define ADIN1200_BITM_SOP_TX_DEL_SOP_TX_10_DEL_N_8_NS          (0X1F00U)

#define ADIN1200_BITP_SOP_TX_DEL_SOP_TX_100_DEL_N_8_NS         (4U)                /* SOP_TX_100_DEL_N_8_NS */
#define ADIN1200_BITL_SOP_TX_DEL_SOP_TX_100_DEL_N_8_NS         (4U)
#define ADIN1200_BITM_SOP_TX_DEL_SOP_TX_100_DEL_N_8_NS         (0X00F0U)

#define ADIN1200_BITP_SOP_TX_DEL_SOP_TX_1000_DEL_N_8_NS        (0U)                /* SOP_TX_1000_DEL_N_8_NS - only for 1G PHY */
#define ADIN1200_BITL_SOP_TX_DEL_SOP_TX_1000_DEL_N_8_NS        (4U)
#define ADIN1200_BITM_SOP_TX_DEL_SOP_TX_1000_DEL_N_8_NS        (0X000FU)

/* ----------------------------------------------------------------------------------------------------
          DPTH_MII_BYTE                               Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_DPTH_MII_BYTE_DPTH_MII_BYTE              (0U)                /* DPTH_MII_BYTE */
#define ADIN1200_BITL_DPTH_MII_BYTE_DPTH_MII_BYTE              (1U)
#define ADIN1200_BITM_DPTH_MII_BYTE_DPTH_MII_BYTE              (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          LPI_WAKE_ERR_CNT                            Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_LPI_WAKE_ERR_CNT_LPI_WAKE_ERR_CNT        (0U)                /* LPI_WAKE_ERR_CNT */
#define ADIN1200_BITL_LPI_WAKE_ERR_CNT_LPI_WAKE_ERR_CNT        (15U)
#define ADIN1200_BITM_LPI_WAKE_ERR_CNT_LPI_WAKE_ERR_CNT        (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          B_10_E_EN                                   Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_B_10_E_EN_B_10_E_EN                      (0U)                /* B_10_E_EN */
#define ADIN1200_BITL_B_10_E_EN_B_10_E_EN                      (1U)
#define ADIN1200_BITM_B_10_E_EN_B_10_E_EN                      (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          B_10_TX_TST_MODE                            Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_B_10_TX_TST_MODE_B_10_TX_TST_MODE        (0U)                /* B_10_TX_TST_MODE */
#define ADIN1200_BITL_B_10_TX_TST_MODE_B_10_TX_TST_MODE        (3U)
#define ADIN1200_BITM_B_10_TX_TST_MODE_B_10_TX_TST_MODE        (0X0007U)

/* ----------------------------------------------------------------------------------------------------
          B_100_TX_TST_MODE                           Value                   Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_B_100_TX_TST_MODE_B_100_TX_TST_MODE      (0U)                /* B_100_TX_TST_MODE */
#define ADIN1200_BITL_B_100_TX_TST_MODE_B_100_TX_TST_MODE      (3U)
#define ADIN1200_BITM_B_100_TX_TST_MODE_B_100_TX_TST_MODE      (0X0007U)

/* ----------------------------------------------------------------------------------------------------
          CDIAG_RUN                                   Value                   Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_CDIAG_RUN_CDIAG_RUN                      (0U)               /* CDIAG_RUN */
#define ADIN1200_BITL_CDIAG_RUN_CDIAG_RUN                      (1U)
#define ADIN1200_BITM_CDIAG_RUN_CDIAG_RUN                      (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          CDIAG_XPAIR_DIS                             Value                   Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_CDIAG_XPAIR_DIS_CDIAG_XPAIR_DIS          (0U)                /* CDIAG_XPAIR_DIS */
#define ADIN1200_BITL_CDIAG_XPAIR_DIS_CDIAG_XPAIR_DIS          (1U)
#define ADIN1200_BITM_CDIAG_XPAIR_DIS_CDIAG_XPAIR_DIS          (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          CDIAG_DTLD_RSLTS_0                         Value                   Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_BSY      (10U)               /* CDIAG_RSLT_0_BSY */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_BSY      (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_BSY      (0X0400U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSIM_3   (9U)                /* CDIAG_RSLT_0_XSIM_3- only in 1G PHY */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSIM_3   (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSIM_3   (0X0200U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSIM_2   (8U)                /* CDIAG_RSLT_0_XSIM_2- only in 1G PHY */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSIM_2   (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSIM_2   (0X0100U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSIM_1   (7U)                /* RSLT_0_XSIM_1 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSIM_1   (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSIM_1   (0X0080U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_SIM      (6U)                /* RSLT_0_SIM */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_SIM      (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_SIM      (0X0040U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSHRT_3  (5U)                /* CDIAG_RSLT_0_XSHRT_3 - only in 1G PHY */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSHRT_3  (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSHRT_3  (0X0020U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSHRT_2  (4U)                /* CDIAG_RSLT_0_XSHRT_2 - only in 1G PHY*/
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSHRT_2  (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSHRT_2  (0X0010U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSHRT_1  (3U)                /* RSLT_0_XSHRT_1 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSHRT_1  (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_XSHRT_1  (0X0008U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_SHRT     (2U)                /* RSLT_0_SHRT */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_SHRT     (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_SHRT     (0X0004U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_OPN      (1U)                /* RSLT_0_OPN */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_OPN      (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_OPN      (0X0002U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_GD       (0U)                /* RSLT_0_GD */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_GD       (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_0_GD       (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          CDIAG_DTLD_RSLTS_1                          Value                Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_BSY      (10U)               /* CDIAG_RSLT_1_BSY */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_BSY      (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_BSY      (0X0400U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_1_XSIM_3   (9U)                /* CDIAG_RSLT_1_XSIM_3- only in 1G PHY */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_1_XSIM_3   (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_1_XSIM_3   (0X0200U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_1_XSIM_2   (8U)                /* CDIAG_RSLT_1_XSIM_2- only in 1G PHY */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_1_XSIM_2   (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_1_XSIM_2   (0X0100U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_XSIM_0   (7U)                /* CDIAG_RSLT_1_XSIM_0 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_XSIM_0   (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_XSIM_0   (0X0080U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_SIM      (6U)                /* CDIAG_RSLT_1_SIM */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_SIM      (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_SIM      (0X0040U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_1_XSHRT_3  (5U)                /* CDIAG_RSLT_1_XSHRT_3 - only in 1G PHY */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_1_XSHRT_3  (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_1_XSHRT_3  (0X0020U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_1_XSHRT_2  (4U)                /* CDIAG_RSLT_1_XSHRT_2 - only in 1G PHY*/
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_1_XSHRT_2  (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_0_CDIAG_RSLT_1_XSHRT_2  (0X0010U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_XSHRT_0  (3U)                /* CDIAG_RSLT_1_XSHRT_0 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_XSHRT_0  (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_XSHRT_0  (0X0008U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_SHRT     (2U)                /* CDIAG_RSLT_1_SHRT */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_SHRT     (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_SHRT     (0X0004U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_OPN      (1U)                /* CDIAG_RSLT_1_OPN */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_OPN      (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_OPN      (0X0002U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_GD       (0U)                /* CDIAG_RSLT_1_GD */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_GD       (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_1_CDIAG_RSLT_1_GD       (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          CDIAG_FLT_DIST_0                           Value                   Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_CDIAG_FLT_DIST_0_CDIAG_FLT_DIST_0       (0U)                 /* CDIAG_FLT_DIST_0 */
#define ADIN1200_BITL_CDIAG_FLT_DIST_0_CDIAG_FLT_DIST_0       (8U)
#define ADIN1200_BITM_CDIAG_FLT_DIST_0_CDIAG_FLT_DIST_0       (0X00FFU)

/* ----------------------------------------------------------------------------------------------------
          CDIAG_FLT_DIST_1                           Value                    Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_CDIAG_FLT_DIST_1_CDIAG_FLT_DIST_1       (0U)                 /* CDIAG_FLT_DIST_1 */
#define ADIN1200_BITL_CDIAG_FLT_DIST_1_CDIAG_FLT_DIST_1       (8U)
#define ADIN1200_BITM_CDIAG_FLT_DIST_1_CDIAG_FLT_DIST_1       (0X00FFU)

/* ----------------------------------------------------------------------------------------------------
          CDIAG_CBL_LEN_EST                          Value                   Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_CDIAG_CBL_LEN_EST_CDIAG_CBL_LEN_EST     (0U)                 /* CDIAG_CBL_LEN_EST */
#define ADIN1200_BITL_CDIAG_CBL_LEN_EST_CDIAG_CBL_LEN_EST     (8U)
#define ADIN1200_BITM_CDIAG_CBL_LEN_EST_CDIAG_CBL_LEN_EST     (0X00FFU)

/* ----------------------------------------------------------------------------------------------------
          LED_PUL_STR_DUR                            Value                   Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_LED_PUL_STR_DUR_LED_PUL_STR_DUR         (0U)                 /* LED_PUL_STR_DUR */
#define ADIN1200_BITL_LED_PUL_STR_DUR_LED_PUL_STR_DUR         (6U)
#define ADIN1200_BITM_LED_PUL_STR_DUR_LED_PUL_STR_DUR         (0X001FU)


/* ==========================================================================================================
   ADINPHY 1G Module Register BitPositions, Lengths, Masks and Enumerations Definitions
   ========================================================================================================== */

/* ----------------------------------------------------------------------------------------------------
          MSE_B                                      Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_MSE_B_MSE_B                             (0U)                 /* MSE_B */
#define ADIN1200_BITL_MSE_B_MSE_B                             (8U)
#define ADIN1200_BITM_MSE_B_MSE_B                             (0X00FFU)

/* ----------------------------------------------------------------------------------------------------
          MSE_C                                      Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_MSE_C_MSE_C                             (0U)                 /* MSE_C */
#define ADIN1200_BITL_MSE_C_MSE_C                             (8U)
#define ADIN1200_BITM_MSE_C_MSE_C                             (0X00FFU)

/* ----------------------------------------------------------------------------------------------------
          B_1000_RTRN_EN                              Value                  Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_B_1000_RTRN_EN_B_1000_RTRN_EN            (0U)                /* B_1000_RTRN_EN */
#define ADIN1200_BITL_B_1000_RTRN_EN_B_1000_RTRN_EN            (1U)
#define ADIN1200_BITM_B_1000_RTRN_EN_B_1000_RTRN_EN            (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          FC_FRM_CNT_H                                Value                Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_FC_FRM_CNT_H                             (0U)                /* FC_FRM_CNT_H */
#define ADIN1200_BITL_FC_FRM_CNT_H                             (16U)
#define ADIN1200_BITM_FC_FRM_CNT_H                             (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          B_100_ZPTM_EN_DIMRX                         Value                   Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_B_100_ZPTM_EN_DIMRX_B_100_ZPTM_EN_DIMRX  (0U)               /* B_100_ZPTM_EN_DIMRX */
#define ADIN1200_BITL_B_100_ZPTM_EN_DIMRX_B_100_ZPTM_EN_DIMRX  (1U)
#define ADIN1200_BITM_B_100_ZPTM_EN_DIMRX_B_100_ZPTM_EN_DIMRX  (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          CDIAG_DTLD_RSLTS_2                          Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_BSY      (10U)               /* CDIAG_RSLT_2_BSY */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_BSY      (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_BSY      (0X0400U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSIM_3   (9U)                /* CDIAG_RSLT_2_XSIM_3 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSIM_3   (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSIM_3   (0X0200U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSIM_1   (8U)                /* CDIAG_RSLT_2_XSIM_1 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSIM_1   (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSIM_1   (0X0100U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSIM_0   (7U)                /* CDIAG_RSLT_2_XSIM_0 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSIM_0   (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSIM_0   (0X0080U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_SIM      (6U)                /* CDIAG_RSLT_2_SIM */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_SIM      (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_SIM      (0X0040U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSHRT_3  (5U)                /* CDIAG_RSLT_2_XSHRT_3 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSHRT_3  (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSHRT_3  (0X0020U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSHRT_1  (4U)                /* CDIAG_RSLT_2_XSHRT_1 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSHRT_1  (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSHRT_1  (0X0010U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSHRT_0  (3U)                /* CDIAG_RSLT_2_XSHRT_0 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSHRT_0  (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_XSHRT_0  (0X0008U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_SHRT     (2U)                /* CDIAG_RSLT_2_SHRT */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_SHRT     (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_SHRT     (0X0004U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_OPN      (1U)                /* CDIAG_RSLT_2_OPN */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_OPN      (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_OPN      (0X0002U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_GD       (0U)                /* CDIAG_RSLT_2_GD */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_GD       (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_2_CDIAG_RSLT_2_GD       (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          CDIAG_DTLD_RSLTS_3                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_BSY      (10U)               /* CDIAG_RSLT_3_BSY */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_BSY      (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_BSY      (0X0400U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSIM_2   (9U)                /* CDIAG_RSLT_3_XSIM_2 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSIM_2   (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSIM_2   (0X0200U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSIM_1   (8U)                /* CDIAG_RSLT_3_XSIM_1 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSIM_1   (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSIM_1   (0X0100U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSIM_0   (7U)                /* CDIAG_RSLT_3_XSIM_0 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSIM_0   (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSIM_0   (0X0080U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_SIM      (6U)                /* CDIAG_RSLT_3_SIM */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_SIM      (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_SIM      (0X0040U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSHRT_2  (5U)                /* CDIAG_RSLT_3_XSHRT_2 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSHRT_2  (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSHRT_2  (0X0020U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSHRT_1  (4U)                /* CDIAG_RSLT_3_XSHRT_1 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSHRT_1  (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSHRT_1  (0X0010U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSHRT_0  (3U)                /* CDIAG_RSLT_3_XSHRT_0 */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSHRT_0  (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_XSHRT_0  (0X0008U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_SHRT     (2U)                /* CDIAG_RSLT_3_SHRT */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_SHRT     (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_SHRT     (0X0004U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_OPN      (1U)                /* CDIAG_RSLT_3_OPN */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_OPN      (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_OPN      (0X0002U)

#define ADIN1200_BITP_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_GD       (0U)                /* CDIAG_RSLT_3_GD */
#define ADIN1200_BITL_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_GD       (1U)
#define ADIN1200_BITM_CDIAG_DTLD_RSLTS_3_CDIAG_RSLT_3_GD       (0X0001U)


/* ----------------------------------------------------------------------------------------------------
          CDIAG_FLT_DIST_2                            Value                    Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_CDIAG_FLT_DIST_2_CDIAG_FLT_DIST_2        (0U)                /* CDIAG_FLT_DIST_2 */
#define ADIN1200_BITL_CDIAG_FLT_DIST_2_CDIAG_FLT_DIST_2        (8U)
#define ADIN1200_BITM_CDIAG_FLT_DIST_2_CDIAG_FLT_DIST_2        (0X00FFU)

/* ----------------------------------------------------------------------------------------------------
          CDIAG_FLT_DIST_3                            Value                    Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_CDIAG_FLT_DIST_3_CDIAG_FLT_DIST_3        (0U)                /* CDIAG_FLT_DIST_3 */
#define ADIN1200_BITL_CDIAG_FLT_DIST_3_CDIAG_FLT_DIST_3        (8U)
#define ADIN1200_BITM_CDIAG_FLT_DIST_3_CDIAG_FLT_DIST_3        (0X00FFU)

/* ----------------------------------------------------------------------------------------------------
          LED_A_INV_EN                                Value                 Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_LED_A_INV_EN_LED_A_INV_EN                (0U)                /* LED_A_INV_EN */
#define ADIN1200_BITL_LED_A_INV_EN_LED_A_INV_EN                (1U)
#define ADIN1200_BITM_LED_A_INV_EN_LED_A_INV_EN                (0X0001U)

#endif /* ADIN_PHY_ADDR_RDEF_45_H */

/*! @endcond */
