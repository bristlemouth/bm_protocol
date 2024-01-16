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

#ifndef ADIN_PHY_ADDR_RDEF_22_H
#define ADIN_PHY_ADDR_RDEF_22_H


#if defined(_LANGUAGE_C) || (defined(__GNUC__) && !defined(__ASSEMBLER__))
#include <stdint.h>
#endif /* _LANGUAGE_C */

/* ====================================================================================================
        ADINPHY Module Register Address Offset Definitions
   ==================================================================================================== */
#define ADIN1200_ADDR_MII_CONTROL                             (0x0000)       /* MII Control Register*/
#define ADIN1200_ADDR_MII_STATUS                              (0x0001)       /* MII Status Register */
#define ADIN1200_ADDR_PHY_ID_1                                (0x0002)       /* PHY Identifier 1 Register */
#define ADIN1200_ADDR_PHY_ID_2                                (0x0003)       /* PHY Identifier 2 Register */
#define ADIN1200_ADDR_AUTONEG_ADV                             (0x0004)       /* Autonegotiation Advertisement Register */
#define ADIN1200_ADDR_LP_ABILITY                              (0x0005)       /* Autonegotiation Link Partner Base Page Ability Register */
#define ADIN1200_ADDR_AUTONEG_EXP                             (0x0006)       /* Autonegotiation Expansion Register */
#define ADIN1200_ADDR_TX_NEXT_PAGE                            (0x0007)       /* Autonegotiation Next Page Transmit Register */
#define ADIN1200_ADDR_LP_RX_NEXT_PAGE                         (0x0008)       /* Autonegotiation Link Partner Received Next Page Register. */
#define ADIN1200_ADDR_MSTR_SLV_STATUS                         (0x000A)       /* Master Slave Status Register */
#define ADIN1200_ADDR_EXT_STATUS                              (0x000F)       /* Extended Status Register */
#define ADIN1200_ADDR_EXT_REG_PTR                             (0x0010)       /* Extended Register Pointer Register */
#define ADIN1200_ADDR_EXT_REG_DATA                            (0x0011)       /* Extended Register Data Register */
#define ADIN1200_ADDR_PHY_CTRL_1                              (0x0012)       /* PHY Control 1 Register */
#define ADIN1200_ADDR_PHY_CTRL_STATUS_1                       (0x0013)       /* PHY Control Status 1 Register */
#define ADIN1200_ADDR_RX_ERR_CNT                              (0x0014)       /* Receive Error Count Register */
#define ADIN1200_ADDR_PHY_CTRL_STATUS_2                       (0x0015)       /* PHY Control Status 2 Register */
#define ADIN1200_ADDR_PHY_CTRL_2                              (0x0016)       /* PHY Control 2 Register */
#define ADIN1200_ADDR_PHY_CTRL_3                              (0x0017)       /* PHY Control 2 Register */
#define ADIN1200_ADDR_IRQ_MASK                                (0x0018)       /* PHY Control 2 Register. */
#define ADIN1200_ADDR_IRQ_STATUS                              (0x0019)       /* PHY Control 2 Register. */
#define ADIN1200_ADDR_PHY_STATUS_1                            (0x001A)       /* PHY Control 2 Register. */
#define ADIN1200_ADDR_LED_CTRL_1                              (0x001B)       /* LED Control 1 Register */
#define ADIN1200_ADDR_LED_CTRL_2                              (0x001C)       /* LED Control 2 Register */
#define ADIN1200_ADDR_LED_CTRL_3                              (0x001D)       /* LED Control 3 Register */
#define ADIN1200_ADDR_PHY_STATUS_2                            (0x001F)       /* PHY Status 2 Register */


/* ====================================================================================================
        ADINPHY 1G Module Register Address Offset Definitions
   ==================================================================================================== */
#define ADIN1200_ADDR_MSTR_SLV_CONTROL                         (0x0009) /* Master Slave Control Register */

/* ====================================================================================================
        ADINPHY Module Register ResetValue Definitions
   ==================================================================================================== */
#define ADIN1200_RSTVAL_MII_CONTROL                           (0x1000)
#define ADIN1200_RSTVAL_MII_STATUS                            (0x7949)
#define ADIN1200_RSTVAL_PHY_ID_1                              (0x0283)
#define ADIN1200_RSTVAL_PHY_ID_2                              (0xBC20)
#define ADIN1200_RSTVAL_AUTONEG_ADV                           (0x01E1)
#define ADIN1200_RSTVAL_LP_ABILITY                            (0x0000)
#define ADIN1200_RSTVAL_AUTONEG_EXP                           (0x0064)
#define ADIN1200_RSTVAL_TX_NEXT_PAGE                          (0x2001)
#define ADIN1200_RSTVAL_LP_RX_NEXT_PAGE                       (0x0000)
#define ADIN1200_RSTVAL_MSTR_SLV_STATUS                       (0x0000)
#define ADIN1200_RSTVAL_EXT_STATUS                            (0x0000)
#define ADIN1200_RSTVAL_EXT_REG_PTR                           (0x0000)
#define ADIN1200_RSTVAL_EXT_REG_DATA                          (0x0000)
#define ADIN1200_RSTVAL_PHY_CTRL_1                            (0x0002)
#define ADIN1200_RSTVAL_PHY_CTRL_STATUS_1                     (0x1041)
#define ADIN1200_RSTVAL_RX_ERR_CNT                            (0x0000)
#define ADIN1200_RSTVAL_PHY_CTRL_STATUS_2                     (0x0000)
#define ADIN1200_RSTVAL_PHY_CTRL_2                            (0x0308)
#define ADIN1200_RSTVAL_PHY_CTRL_3                            (0x3048)
#define ADIN1200_RSTVAL_IRQ_MASK Interrupt                    (0x0000)
#define ADIN1200_RSTVAL_IRQ_STATUS                            (0x0000)
#define ADIN1200_RSTVAL_PHY_STATUS_1                          (0x0300)
#define ADIN1200_RSTVAL_LED_CTRL_1                            (0x0001)
#define ADIN1200_RSTVAL_LED_CTRL_2                            (0x210A)
#define ADIN1200_RSTVAL_LED_CTRL_3                            (0x1855)
#define ADIN1200_RSTVAL_PHY_STATUS_2                          (0x03FC)

/* ====================================================================================================
        ADINPHY 1G Module Register ResetValue Definitions
   ==================================================================================================== */
#define ADIN1200_RSTVAL_MSTR_SLV_CONTROL                      (0x0200)       /* Master Slave Control Register */


/* ====================================================================================================
        ADINPHY Module Register BitPositions, Lengths, Masks and Enumerations Definitions
   ==================================================================================================== */

/* ----------------------------------------------------------------------------------------------------
          MII_CONTROL                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_MII_CONTROL_SFT_RST                     (15U)          /* Software Reset Bit */
#define ADIN1200_BITL_MII_CONTROL_SFT_RST                     (1U)
#define ADIN1200_BITM_MII_CONTROL_SFT_RST                     (0X8000U)

#define ADIN1200_BITP_MII_CONTROL_LOOPBACK                    (14U)          /* LoopBack */
#define ADIN1200_BITL_MII_CONTROL_LOOPBACK                    (1U)
#define ADIN1200_BITM_MII_CONTROL_LOOPBACK                    (0X4000U)

#define ADIN1200_BITP_MII_CONTROL_SPEED_SEL_LSB               (13U)          /* The speed selection LSB register bits */
#define ADIN1200_BITL_MII_CONTROL_SPEED_SEL_LSB               (1U)
#define ADIN1200_BITM_MII_CONTROL_SPEED_SEL_LSB               (0X2000U)

#define ADIN1200_BITP_MII_CONTROL_AUTONEG_EN                  (12U)          /* Autonegotiation enable bit */
#define ADIN1200_BITL_MII_CONTROL_AUTONEG_EN                  (1U)
#define ADIN1200_BITM_MII_CONTROL_AUTONEG_EN                  (0X1000U)

#define ADIN1200_BITP_MII_CONTROL_SFT_PD                      (11U)          /* Software Power-down */
#define ADIN1200_BITL_MII_CONTROL_SFT_PD                      (1U)
#define ADIN1200_BITM_MII_CONTROL_SFT_PD                      (0X0800U)

#define ADIN1200_BITP_MII_CONTROL_ISOLATE                     (10U)          /* Software Power-down */
#define ADIN1200_BITL_MII_CONTROL_ISOLATE                     (1U)
#define ADIN1200_BITM_MII_CONTROL_ISOLATE                     (0X0400U)

#define ADIN1200_BITP_MII_CONTROL_RESTART_ANEG                (9U)           /* Restart Autonegotiation Bit */
#define ADIN1200_BITL_MII_CONTROL_RESTART_ANEG                (1U)
#define ADIN1200_BITM_MII_CONTROL_RESTART_ANEG                (0X0200U)

#define ADIN1200_BITP_MII_CONTROL_DPLX_MODE                   (8U)           /* Duplex Mode Bit. */
#define ADIN1200_BITL_MII_CONTROL_DPLX_MODE                   (1U)
#define ADIN1200_BITM_MII_CONTROL_DPLX_MODE                   (0X0100U)

#define ADIN1200_BITP_MII_CONTROL_COLTEST                     (7U)           /* Col test Bit. */
#define ADIN1200_BITL_MII_CONTROL_COLTEST                     (1U)
#define ADIN1200_BITM_MII_CONTROL_COLTEST                     (0X0080U)

#define ADIN1200_BITP_MII_CONTROL_SPEED_SEL_MSB               (6U)           /* The speed selection MSB */
#define ADIN1200_BITL_MII_CONTROL_SPEED_SEL_MSB               (1U)
#define ADIN1200_BITM_MII_CONTROL_SPEED_SEL_MSB               (0X0040U)

#define ADIN1200_BITP_MII_CONTROL_UNIDIR_EN                   (5U)           /* Uni Dir Bit */
#define ADIN1200_BITL_MII_CONTROL_UNIDIR_EN                   (1U)
#define ADIN1200_BITM_MII_CONTROL_UNIDIR_EN                   (0X0020U)


/* ----------------------------------------------------------------------------------------------------
          MII_STATUS                                  Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_MII_STATUS_T_4_SPRT                     (15U)          /* T4 SPRT Bit */
#define ADIN1200_BITL_MII_STATUS_T_4_SPRT                     (1U)           /* T4 SPRT Bit */
#define ADIN1200_BITM_MII_STATUS_T_4_SPRT                     (0X8000U)      /* T4 SPRT Bit */

#define ADIN1200_BITP_MII_STATUS_FD_100_SPRT                  (14U)          /* FD_100_SPRT */
#define ADIN1200_BITL_MII_STATUS_FD_100_SPRT                  (1U)
#define ADIN1200_BITM_MII_STATUS_FD_100_SPRT                  (0X4000U)

#define ADIN1200_BITP_MII_STATUS_HD_100_SPRT                  (13U)          /* HD_100_SPRT  Bit */
#define ADIN1200_BITL_MII_STATUS_HD_100_SPRT                  (1U)
#define ADIN1200_BITM_MII_STATUS_HD_100_SPRT                  (0X2000U)

#define ADIN1200_BITP_MII_STATUS_FD_10_SPRT                   (12U)          /* FD_10_SPRT Bit */
#define ADIN1200_BITL_MII_STATUS_FD_10_SPRT                   (1U)
#define ADIN1200_BITM_MII_STATUS_FD_10_SPRT                   (0X1000U)

#define ADIN1200_BITP_MII_STATUS_HD_10_SPRT                   (11U)          /* HD_10_SPRT Bit */
#define ADIN1200_BITL_MII_STATUS_HD_10_SPRT                   (1U)
#define ADIN1200_BITM_MII_STATUS_HD_10_SPRT                   (0X0800U)

#define ADIN1200_BITP_MII_STATUS_FD_T_2_SPRT                  (10U)          /* FD_T_2_SPRT Bit */
#define ADIN1200_BITL_MII_STATUS_FD_T_2_SPRT                  (1U)
#define ADIN1200_BITM_MII_STATUS_FD_T_2_SPRT                  (0X0400U)

#define ADIN1200_BITP_MII_STATUS_HD_T_2_SPRT                  (9U)           /* HD_T_2_SPRT Bit */
#define ADIN1200_BITL_MII_STATUS_HD_T_2_SPRT                  (1U)
#define ADIN1200_BITM_MII_STATUS_HD_T_2_SPRT                  (0X0200U)

#define ADIN1200_BITP_MII_STATUS_EXT_STAT_SPRT                (8U)           /* EXT_STAT_SPRT Bit */
#define ADIN1200_BITL_MII_STATUS_EXT_STAT_SPRT                (1U)
#define ADIN1200_BITM_MII_STATUS_EXT_STAT_SPRT                (0X0100U)

#define ADIN1200_BITP_MII_STATUS_UNIDIR_ABLE                  (7U)           /* UNIDIR_ABLE Bit */
#define ADIN1200_BITL_MII_STATUS_UNIDIR_ABLE                  (1U)
#define ADIN1200_BITM_MII_STATUS_UNIDIR_ABLE                  (0X0080U)

#define ADIN1200_BITP_MII_STATUS_MF_PREAM_SUP_ABLE            (6U)           /* MF_PREAM_SUP_ABLE Bit */
#define ADIN1200_BITL_MII_STATUS_MF_PREAM_SUP_ABLE            (1U)
#define ADIN1200_BITM_MII_STATUS_MF_PREAM_SUP_ABLE            (0X0040U)

#define ADIN1200_BITP_MII_STATUS_AUTONEG_DONE                 (5U)           /* Autonegotiation Done Bit */
#define ADIN1200_BITL_MII_STATUS_AUTONEG_DONE                 (1U)
#define ADIN1200_BITM_MII_STATUS_AUTONEG_DONE                 (0X0020U)

#define ADIN1200_BITP_MII_STATUS_REM_FLT_LAT                  (4U)           /* REM_FLT_LAT Bit */
#define ADIN1200_BITL_MII_STATUS_REM_FLT_LAT                  (1U)
#define ADIN1200_BITM_MII_STATUS_REM_FLT_LAT                  (0X0010U)

#define ADIN1200_BITP_MII_STATUS_AUTONEG_ABLE                 (3U)           /* AUTONEG_ABLE Bit */
#define ADIN1200_BITL_MII_STATUS_AUTONEG_ABLE                 (1U)
#define ADIN1200_BITM_MII_STATUS_AUTONEG_ABLE                 (0X0008U)

#define ADIN1200_BITP_MII_STATUS_LINK_STAT_LAT                (2U)           /* LINK_STAT_LAT Bit */
#define ADIN1200_BITL_MII_STATUS_LINK_STAT_LAT                (1U)
#define ADIN1200_BITM_MII_STATUS_LINK_STAT_LAT                (0X0004U)

#define ADIN1200_BITP_MII_STATUS_JABBER_DET_LAT               (1U)           /* JABBER_DET_LAT Bit. */
#define ADIN1200_BITL_MII_STATUS_JABBER_DET_LAT               (1U)
#define ADIN1200_BITM_MII_STATUS_JABBER_DET_LAT               (0X0002U)

#define ADIN1200_BITP_MII_STATUS_EXT_CAPABLE                  (0U)           /* EXT_CAPABLE Bit */
#define ADIN1200_BITL_MII_STATUS_EXT_CAPABLE                  (1U)
#define ADIN1200_BITM_MII_STATUS_EXT_CAPABLE                  (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          PHY_ID_1                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_PHY_ID_1_PHY_ID_1                       (0U)           /* Organizationally Unique Identifier */
#define ADIN1200_BITL_PHY_ID_1_PHY_ID_1                       (16U)
#define ADIN1200_BITM_PHY_ID_1_PHY_ID_1                       (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          PHY_ID_2                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_PHY_ID_2_PHY_ID_2_OUI                   (10U)          /* Organizationally Unique Identifier */
#define ADIN1200_BITL_PHY_ID_2_PHY_ID_2_OUI                   (6U)
#define ADIN1200_BITM_PHY_ID_2_PHY_ID_2_OUI                   (0XFC00U)

#define ADIN1200_BITP_PHY_ID_2_MODEL_NUM                      (4U)           /* Model Number */
#define ADIN1200_BITL_PHY_ID_2_MODEL_NUM                      (6U)
#define ADIN1200_BITM_PHY_ID_2_MODEL_NUM                      (0X03F0U)

#define ADIN1200_BITP_PHY_ID_2_REV_NUM                        (0U)           /* Revision Number */
#define ADIN1200_BITL_PHY_ID_2_REV_NUM                        (4U)
#define ADIN1200_BITM_PHY_ID_2_REV_NUM                        (0X000FU)

/* ----------------------------------------------------------------------------------------------------
          AUTONEG_ADV                               Value             Description
   ---------------------------------------------------------------------------------------------------- */

#define ADIN1200_BITP_AUTONEG_ADV_NEXT_PAGE_ADV               (15U)          /* NEXT_PAGE_ADV  */
#define ADIN1200_BITL_AUTONEG_ADV_NEXT_PAGE_ADV               (1U)
#define ADIN1200_BITM_AUTONEG_ADV_NEXT_PAGE_ADV               (0X8000U)

#define ADIN1200_BITP_AUTONEG_ADV_REM_FLT_ADV                 (13U)          /* REM_FLT_ADV */
#define ADIN1200_BITL_AUTONEG_ADV_REM_FLT_ADV                 (1U)
#define ADIN1200_BITM_AUTONEG_ADV_REM_FLT_ADV                 (0X2000U)

#define ADIN1200_BITP_AUTONEG_ADV_EXT_NEXT_PAGE_ADV           (12U)          /* NEXT_PAGE_ADV  */
#define ADIN1200_BITL_AUTONEG_ADV_EXT_NEXT_PAGE_ADV           (1U)
#define ADIN1200_BITM_AUTONEG_ADV_EXT_NEXT_PAGE_ADV           (0X1000U)

#define ADIN1200_BITP_AUTONEG_ADV_APAUSE_ADV                  (11U)          /* APAUSE_ADV */
#define ADIN1200_BITL_AUTONEG_ADV_APAUSE_ADV                  (1U)
#define ADIN1200_BITM_AUTONEG_ADV_APAUSE_ADV                  (0X0800U)

#define ADIN1200_BITP_AUTONEG_ADV_PAUSE_ADV                   (10U)          /* PAUSE_ADV  */
#define ADIN1200_BITL_AUTONEG_ADV_PAUSE_ADV                   (1U)
#define ADIN1200_BITM_AUTONEG_ADV_PAUSE_ADV                   (0X0400U)

#define ADIN1200_BITP_AUTONEG_ADV_T_4_ADV                     (9U)           /* T_4_ADV */
#define ADIN1200_BITL_AUTONEG_ADV_T_4_ADV                     (1U)
#define ADIN1200_BITM_AUTONEG_ADV_T_4_ADV                     (0X0200U)

#define ADIN1200_BITP_AUTONEG_ADV_FD_100_ADV                  (8U)           /* ADV_FD_100_ADV */
#define ADIN1200_BITL_AUTONEG_ADV_FD_100_ADV                  (1U)
#define ADIN1200_BITM_AUTONEG_ADV_FD_100_ADV                  (0X0100U)

#define ADIN1200_BITP_AUTONEG_ADV_HD_100_ADV                  (7U)           /* ADV_HD_100_ADV */
#define ADIN1200_BITL_AUTONEG_ADV_HD_100_ADV                  (1U)
#define ADIN1200_BITM_AUTONEG_ADV_HD_100_ADV                  (0X0080U)

#define ADIN1200_BITP_AUTONEG_ADV_FD_10_ADV                   (6U)           /* ADV_FD_10_ADV */
#define ADIN1200_BITL_AUTONEG_ADV_FD_10_ADV                   (1U)
#define ADIN1200_BITM_AUTONEG_ADV_FD_10_ADV                   (0X0040U)

#define ADIN1200_BITP_AUTONEG_ADV_HD_10_ADV                   (5U)           /* ADV_HD_10_ADV */
#define ADIN1200_BITL_AUTONEG_ADV_HD_10_ADV                   (1U)
#define ADIN1200_BITM_AUTONEG_ADV_HD_10_ADV                   (0X0020U)

#define ADIN1200_BITP_AUTONEG_ADV_SELECTOR_ADV                (0U)           /* SELECTOR_ADV  */
#define ADIN1200_BITL_AUTONEG_ADV_SELECTOR_ADV                (5U)
#define ADIN1200_BITM_AUTONEG_ADV_SELECTOR_ADV                (0X001FU)


/* ----------------------------------------------------------------------------------------------------
          LP_ABILITY                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_LP_ABILITY_LP_NEXT_PAGE                 (15U)          /* LP_NEXT_PAGE */
#define ADIN1200_BITL_LP_ABILITY_LP_NEXT_PAGE                 (1U)
#define ADIN1200_BITM_LP_ABILITY_LP_NEXT_PAGE                 (0X8000U)

#define ADIN1200_BITP_LP_ABILITY_LP_ACK                       (14U)          /* LP_ACK */
#define ADIN1200_BITL_LP_ABILITY_LP_ACK                       (1U)
#define ADIN1200_BITM_LP_ABILITY_LP_ACK                       (0X4000U)

#define ADIN1200_BITP_LP_ABILITY_LP_REM_FLT                   (13U)          /* LP_REM_FLT */
#define ADIN1200_BITL_LP_ABILITY_LP_REM_FLT                   (1U)
#define ADIN1200_BITM_LP_ABILITY_LP_REM_FLT                   (0X2000U)

#define ADIN1200_BITP_LP_ABILITY_LP_EXT_NEXT_PAGE_ABLE        (12U)          /* LP_EXT_NEXT_PAGE_ABLE */
#define ADIN1200_BITL_LP_ABILITY_LP_EXT_NEXT_PAGE_ABLE        (1U)
#define ADIN1200_BITM_LP_ABILITY_LP_EXT_NEXT_PAGE_ABLE        (0X1000U)

#define ADIN1200_BITP_LP_ABILITY_LP_APAUSE_ABLE               (11U)          /* LP_APAUSE_ABLE */
#define ADIN1200_BITL_LP_ABILITY_LP_APAUSE_ABLE               (1U)
#define ADIN1200_BITM_LP_ABILITY_LP_APAUSE_ABLE               (0X0800U)

#define ADIN1200_BITP_LP_ABILITY_LP_PAUSE_ABLE                (10U)          /* LP_PAUSE_ABLE */
#define ADIN1200_BITL_LP_ABILITY_LP_PAUSE_ABLE                (1U)
#define ADIN1200_BITM_LP_ABILITY_LP_PAUSE_ABLE                (0X0400U)

#define ADIN1200_BITP_LP_ABILITY_LP_T_4_ABLE                  (9U)           /* LP_T_4_ABLE */
#define ADIN1200_BITL_LP_ABILITY_LP_T_4_ABLE                  (1U)
#define ADIN1200_BITM_LP_ABILITY_LP_T_4_ABLE                  (0X0200U)

#define ADIN1200_BITP_LP_ABILITY_LP_FD_100_ABLE               (8U)           /* LP_FD_100_ABLE */
#define ADIN1200_BITL_LP_ABILITY_LP_FD_100_ABLE               (1U)
#define ADIN1200_BITM_LP_ABILITY_LP_FD_100_ABLE               (0X0100U)

#define ADIN1200_BITP_LP_ABILITY_LP_HD_100_ABLE               (7U)           /* LP_HD_100_ABLE */
#define ADIN1200_BITL_LP_ABILITY_LP_HD_100_ABLE               (1U)
#define ADIN1200_BITM_LP_ABILITY_LP_HD_100_ABLE               (0X0080U)

#define ADIN1200_BITP_LP_ABILITY_LP_FD_10_ABLE                (6U)           /* LP_FD_10_ABLE */
#define ADIN1200_BITL_LP_ABILITY_LP_FD_10_ABLE                (1U)
#define ADIN1200_BITM_LP_ABILITY_LP_FD_10_ABLE                (0X0040U)

#define ADIN1200_BITP_LP_ABILITY_LP_HD_10_ABLE                (5U)           /* LP_HD_10_ABLE */
#define ADIN1200_BITL_LP_ABILITY_LP_HD_10_ABLE                (1U)
#define ADIN1200_BITM_LP_ABILITY_LP_HD_10_ABLE                (0X0020U)

#define ADIN1200_BITP_LP_ABILITY_LP_SELECTOR                  (0U)           /* LP_SELECTOR */
#define ADIN1200_BITL_LP_ABILITY_LP_SELECTOR                  (5U)
#define ADIN1200_BITM_LP_ABILITY_LP_SELECTOR                  (0X001FU)

/* ----------------------------------------------------------------------------------------------------
          AUTONEG_EXP                                Value             Description
   ---------------------------------------------------------------------------------------------------- */

#define ADIN1200_BITP_AUTONEG_EXP_RX_NP_LOC_ABLE              (6U)           /* RX_NP_LOC_ABLE  */
#define ADIN1200_BITL_AUTONEG_EXP_RX_NP_LOC_ABLE              (1U)
#define ADIN1200_BITM_AUTONEG_EXP_RX_NP_LOC_ABLE              (0X0040U)

#define ADIN1200_BITP_AUTONEG_EXP_RX_NP_LOC                   (5U)           /* RX_NP_LOC */
#define ADIN1200_BITL_AUTONEG_EXP_RX_NP_LOC                   (1U)
#define ADIN1200_BITM_AUTONEG_EXP_RX_NP_LOC                   (0X0020U)

#define ADIN1200_BITP_AUTONEG_EXP_PAR_DET_FLT                 (4U)           /* PAR_DET_FLT */
#define ADIN1200_BITL_AUTONEG_EXP_PAR_DET_FLT                 (1U)
#define ADIN1200_BITM_AUTONEG_EXP_PAR_DET_FLT                 (0X0010U)

#define ADIN1200_BITP_AUTONEG_EXP_LP_NP_ABLE                  (3U)           /* LP_NP_ABLE */
#define ADIN1200_BITL_AUTONEG_EXP_LP_NP_ABLE                  (1U)
#define ADIN1200_BITM_AUTONEG_EXP_LP_NP_ABLE                  (0X0008U)

#define ADIN1200_BITP_AUTONEG_EXP_NP_ABLE                     (2U)           /* NP_ABLE */
#define ADIN1200_BITL_AUTONEG_EXP_NP_ABLE                     (1U)
#define ADIN1200_BITM_AUTONEG_EXP_NP_ABLE                     (0X0004U)

#define ADIN1200_BITP_AUTONEG_EXP_PAGE_RX_LAT                 (1U)           /* PAGE_RX_LAT */
#define ADIN1200_BITL_AUTONEG_EXP_PAGE_RX_LAT                 (1U)
#define ADIN1200_BITM_AUTONEG_EXP_PAGE_RX_LAT                 (0X0002U)

#define ADIN1200_BITP_AUTONEG_LP_AUTONEG_ABLE                 (0U)           /* LP_AUTONEG_ABLE */
#define ADIN1200_BITL_AUTONEG_LP_AUTONEG_ABLE                 (1U)
#define ADIN1200_BITM_AUTONEG_LP_AUTONEG_ABLE                 (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          TX_NEXT_PAGE                              Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_TX_NEXT_PAGE_NP_NEXT_PAGE               (15U)          /* NP_NEXT_PAGE */
#define ADIN1200_BITL_TX_NEXT_PAGE_NP_NEXT_PAGE               (1U)
#define ADIN1200_BITM_TX_NEXT_PAGE_NP_NEXT_PAGE               (0X8000U)

#define ADIN1200_BITP_TX_NEXT_PAGE_NP_MSG_PAGE                (13U)          /* NP_MSG_PAGE */
#define ADIN1200_BITL_TX_NEXT_PAGE_NP_MSG_PAGE                (1U)
#define ADIN1200_BITM_TX_NEXT_PAGE_NP_MSG_PAGE                (0X2000U)

#define ADIN1200_BITP_TX_NEXT_PAGE_NP_ACK_2                   (12U)          /* NP_ACK_2 */
#define ADIN1200_BITL_TX_NEXT_PAGE_NP_ACK_2                   (1U)
#define ADIN1200_BITM_TX_NEXT_PAGE_NP_ACK_2                   (0X1000U)

#define ADIN1200_BITP_TX_NEXT_PAGE_NP_TOGGLE                  (11U)          /* NP_TOGGLE */
#define ADIN1200_BITL_TX_NEXT_PAGE_NP_TOGGLE                  (1U)
#define ADIN1200_BITM_TX_NEXT_PAGE_NP_TOGGLE                  (0X0800U)

#define ADIN1200_BITP_TX_NEXT_PAGE_NP_CODE                    (10U)          /* NP_CODE */
#define ADIN1200_BITL_TX_NEXT_PAGE_NP_CODE                    (11U)
#define ADIN1200_BITM_TX_NEXT_PAGE_NP_CODE                    (0X07FFU)

/* ----------------------------------------------------------------------------------------------------
          LP_RX_NEXT_PAGE                              Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_LP_RX_NEXT_PAGE_LP_NP_NEXT_PAGE         (15U)          /* LP_NP_NEXT_PAGE */
#define ADIN1200_BITL_LP_RX_NEXT_PAGE_LP_NP_NEXT_PAGE         (1U)
#define ADIN1200_BITM_LP_RX_NEXT_PAGE_LP_NP_NEXT_PAGE         (0X8000U)

#define ADIN1200_BITP_LP_RX_NEXT_PAGE_LP_NP_ACK               (14U)          /* LP_NP_ACK */
#define ADIN1200_BITL_LP_RX_NEXT_PAGE_LP_NP_ACK               (1U)
#define ADIN1200_BITM_LP_RX_NEXT_PAGE_LP_NP_ACK               (0X4000U)

#define ADIN1200_BITP_LP_RX_NEXT_PAGE_LP_NP_MSG_PAGE          (13U)          /* LP_NP_MSG_PAGE */
#define ADIN1200_BITL_LP_RX_NEXT_PAGE_LP_NP_MSG_PAGE          (1U)
#define ADIN1200_BITM_LP_RX_NEXT_PAGE_LP_NP_MSG_PAGE          (0X2000U)

#define ADIN1200_BITP_LP_RX_NEXT_PAGE_LP_NP_ACK_2             (12U)          /* LP_NP_ACK_2 */
#define ADIN1200_BITL_LP_RX_NEXT_PAGE_LP_NP_ACK_2             (1U)
#define ADIN1200_BITM_LP_RX_NEXT_PAGE_LP_NP_ACK_2             (0X1000U)

#define ADIN1200_BITP_LP_RX_NEXT_PAGE_LP_NP_TOGGLE            (11U)          /* LP_NP_TOGGLE */
#define ADIN1200_BITL_LP_RX_NEXT_PAGE_LP_NP_TOGGLE            (1U)
#define ADIN1200_BITM_LP_RX_NEXT_PAGE_LP_NP_TOGGLE            (0X0800U)

#define ADIN1200_BITP_LP_RX_NEXT_PAGE_LP_NP_CODE              (10U)          /* LP_NP_CODE */
#define ADIN1200_BITL_LP_RX_NEXT_PAGE_LP_NP_CODE              (11U)
#define ADIN1200_BITM_LP_RX_NEXT_PAGE_LP_NP_CODE              (0X07FFU)


/* ----------------------------------------------------------------------------------------------------
          MSTR_SLV_STATUS                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_MSTR_SLV_STATUS_MSTR_SLV_FLT            (15U)          /* MSTR_SLV_FLT - Only in ADINPHY 1G */
#define ADIN1200_BITL_MSTR_SLV_STATUS_MSTR_SLV_FLT            (1U)
#define ADIN1200_BITM_MSTR_SLV_STATUS_MSTR_SLV_FLT            (0X8000U)

#define ADIN1200_BITP_MSTR_SLV_STATUS_MSTR_RSLVD              (14U)          /* MSTR_RSLVD - Only in ADINPHY 1G */
#define ADIN1200_BITL_MSTR_SLV_STATUS_MSTR_RSLVD              (1U)
#define ADIN1200_BITM_MSTR_SLV_STATUS_MSTR_RSLVD              (0X4000U)

#define ADIN1200_BITP_MSTR_SLV_STATUS_LOC_RCVR_STATUS         (13U)          /* LOC_RCVR_STATUS */
#define ADIN1200_BITL_MSTR_SLV_STATUS_LOC_RCVR_STATUS         (1U)
#define ADIN1200_BITM_MSTR_SLV_STATUS_LOC_RCVR_STATUS         (0X2000U)

#define ADIN1200_BITP_MSTR_SLV_STATUS_REM_RCVR_STATUS         (12U)          /* REM_RCVR_STATUS */
#define ADIN1200_BITL_MSTR_SLV_STATUS_REM_RCVR_STATUS         (1U)
#define ADIN1200_BITM_MSTR_SLV_STATUS_REM_RCVR_STATUS         (0X1000U)

#define ADIN1200_BITP_MSTR_SLV_STATUS_LP_FD_1000_ABLE         (11U)          /* LP_FD_1000_ABLE */
#define ADIN1200_BITL_MSTR_SLV_STATUS_LP_FD_1000_ABLE         (1U)
#define ADIN1200_BITM_MSTR_SLV_STATUS_LP_FD_1000_ABLE         (0X0800U)

#define ADIN1200_BITP_MSTR_SLV_STATUS_LP_HD_1000_ABLE         (10U)          /* LP_HD_1000_ABLE */
#define ADIN1200_BITL_MSTR_SLV_STATUS_LP_HD_1000_ABLE         (1U)
#define ADIN1200_BITM_MSTR_SLV_STATUS_LP_HD_1000_ABLE         (0X0400U)

#define ADIN1200_BITP_MSTR_SLV_STATUS_IDLE_ERR_CNT            (0U)           /* IDLE_ERR_CNT */
#define ADIN1200_BITL_MSTR_SLV_STATUS_IDLE_ERR_CNT            (8U)
#define ADIN1200_BITM_MSTR_SLV_STATUS_IDLE_ERR_CNT            (0X00FFU)

/* ----------------------------------------------------------------------------------------------------
          EXT_STATUS                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_EXT_STATUS_FD_1000_X_SPRT               (15U)          /* FD_1000_X_SPRT */
#define ADIN1200_BITL_EXT_STATUS_FD_1000_X_SPRT               (1U)
#define ADIN1200_BITM_EXT_STATUS_FD_1000_X_SPRT               (0X8000U)

#define ADIN1200_BITP_EXT_STATUS_HD_1000_X_SPRT               (14U)          /* HD_1000_X_SPRT */
#define ADIN1200_BITL_EXT_STATUS_HD_1000_X_SPRT               (1U)
#define ADIN1200_BITM_EXT_STATUS_HD_1000_X_SPRT               (0X4000U)

#define ADIN1200_BITP_EXT_STATUS_FD_1000_SPRT                 (13U)          /* FD_1000_SPRT */
#define ADIN1200_BITL_EXT_STATUS_FD_1000_SPRT                 (1U)
#define ADIN1200_BITM_EXT_STATUS_FD_1000_SPRT                 (0X2000U)

#define ADIN1200_BITP_EXT_STATUS_HD_1000_SPRT                 (12U)          /* HD_1000_SPRT */
#define ADIN1200_BITL_EXT_STATUS_HD_1000_SPRT                 (1U)
#define ADIN1200_BITM_EXT_STATUS_HD_1000_SPRT                 (0X1000U)

/* ----------------------------------------------------------------------------------------------------
          EXT_REG_PTR                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_EXT_REG_PTR_EXT_REG_PTR                 (0U)           /* EXT_REG_PTR*/
#define ADIN1200_BITL_EXT_REG_PTR_EXT_REG_PTR                 (15U)
#define ADIN1200_BITM_EXT_REG_PTR_EXT_REG_PTR                 (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          EXT_REG_DATA                               Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_EXT_REG_DATA_EXT_REG_DATA               (0U)           /* EXT_REG_DATA*/
#define ADIN1200_BITL_EXT_REG_DATA_EXT_REG_DATA               (15U)
#define ADIN1200_BITM_EXT_REG_DATA_EXT_REG_DATA               (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          PHY_CTRL_1                                 Value              Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_PHY_CTRL_1_AUTO_MDI_EN                  (10U)          /* Enable/Disable Auto MDI/MDIX */
#define ADIN1200_BITL_PHY_CTRL_1_AUTO_MDI_EN                  (1U)
#define ADIN1200_BITM_PHY_CTRL_1_AUTO_MDI_EN                  (0X0400U)

#define ADIN1200_BITP_PHY_CTRL_1_MAN_MDIX                     (9U)           /* Operate in MDIX/MDI Configuration */
#define ADIN1200_BITL_PHY_CTRL_1_MAN_MDIX                     (1U)
#define ADIN1200_BITM_PHY_CTRL_1_MAN_MDIX                     (0X0200U)

#define ADIN1200_BITP_PHY_CTRL_1_DIAG_CLK_EN                  (2U)           /* Enable PHY Diagnostics Clock.*/
#define ADIN1200_BITL_PHY_CTRL_1_DIAG_CLK_EN                  (1U)
#define ADIN1200_BITM_PHY_CTRL_1_DIAG_CLK_EN                  (0X0004U)

/* ----------------------------------------------------------------------------------------------------
          PHY_CTRL_STATUS_1                          Value              Description
   ---------------------------------------------------------------------------------------------------- */

#define ADIN1200_BITP_PHY_CTRL_STATUS_1_LB_ALL_DIG_SEL        (12U)          /* Enable PHY Diagnostics Clock.*/
#define ADIN1200_BITL_PHY_CTRL_STATUS_1_LB_ALL_DIG_SEL        (1U)
#define ADIN1200_BITM_PHY_CTRL_STATUS_1_LB_ALL_DIG_SEL        (0X1000U)

#define ADIN1200_BITP_PHY_CTRL_STATUS_1_LB_LD_SEL             (10U)          /* Enable PHY Diagnostics Clock.*/
#define ADIN1200_BITL_PHY_CTRL_STATUS_1_LB_LD_SEL             (1U)
#define ADIN1200_BITM_PHY_CTRL_STATUS_1_LB_LD_SEL             (0X0400U)

#define ADIN1200_BITP_PHY_CTRL_STATUS_1_LB_REMOTE_EN          (9U)           /* Enable PHY Diagnostics Clock.*/
#define ADIN1200_BITL_PHY_CTRL_STATUS_1_LB_REMOTE_EN          (1U)
#define ADIN1200_BITM_PHY_CTRL_STATUS_1_LB_REMOTE_EN          (0X0200U)

#define ADIN1200_BITP_PHY_CTRL_STATUS_1_ISOLATE_RX            (8U)           /* Enable PHY Diagnostics Clock.*/
#define ADIN1200_BITL_PHY_CTRL_STATUS_1_ISOLATE_RX            (1U)
#define ADIN1200_BITM_PHY_CTRL_STATUS_1_ISOLATE_RX            (0X0100U)

#define ADIN1200_BITP_PHY_CTRL_STATUS_1_LB_EXT_EN             (7U)           /* Enable PHY Diagnostics Clock.*/
#define ADIN1200_BITL_PHY_CTRL_STATUS_1_LB_EXT_EN             (1U)
#define ADIN1200_BITM_PHY_CTRL_STATUS_1_LB_EXT_EN             (0X0080U)

#define ADIN1200_BITP_PHY_CTRL_STATUS_1_LB_TX_SUP             (6U)           /* Enable PHY Diagnostics Clock.*/
#define ADIN1200_BITL_PHY_CTRL_STATUS_1_LB_TX_SUP             (1U)
#define ADIN1200_BITM_PHY_CTRL_STATUS_1_LB_TX_SUP             (0X0040U)

#define ADIN1200_BITP_PHY_CTRL_STATUS_1_LB_MII_LS_OK          (0U)           /* Enable PHY Diagnostics Clock.*/
#define ADIN1200_BITL_PHY_CTRL_STATUS_1_LB_MII_LS_OK          (1U)
#define ADIN1200_BITM_PHY_CTRL_STATUS_1_LB_MII_LS_OK          (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          RX_ERR_CNT                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_RX_ERR_CNT_RX_ERR_CNT                   (0U)           /* RX_ERR_CNT.*/
#define ADIN1200_BITL_RX_ERR_CNT_RX_ERR_CNT                   (15U)
#define ADIN1200_BITM_RX_ERR_CNT_RX_ERR_CNT                   (0XFFFFU)

/* ----------------------------------------------------------------------------------------------------
          PHY_CTRL_STATUS_2                         Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_PHY_CTRL_STATUS_2_NRG_PD_EN             (3U)           /* Setting this bit enables energy detect power-down*/
#define ADIN1200_BITL_PHY_CTRL_STATUS_2_NRG_PD_EN             (1U)
#define ADIN1200_BITM_PHY_CTRL_STATUS_2_NRG_PD_EN             (0X0008U)

#define ADIN1200_BITP_PHY_CTRL_STATUS_2_NRG_PD_TX_EN          (2U)           /* Transmits pulse in power down mode */
#define ADIN1200_BITL_PHY_CTRL_STATUS_2_NRG_PD_TX_EN          (1U)
#define ADIN1200_BITM_PHY_CTRL_STATUS_2_NRG_PD_TX_EN          (0X0004U)

#define ADIN1200_BITP_PHY_CTRL_STATUS_2_PHY_IN_NRG_PD         (1U)           /* status bit*/
#define ADIN1200_BITL_PHY_CTRL_STATUS_2_PHY_IN_NRG_PD         (1U)
#define ADIN1200_BITM_PHY_CTRL_STATUS_2_PHY_IN_NRG_PD         (0X0002U)

/* ----------------------------------------------------------------------------------------------------
          PHY_CTRL_2                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_PHY_CTRL_2_DN_SPEED_TO_100_EN           (11U)          /* DN_SPEED_TO_100_EN - only for ADINPHY 1G*/
#define ADIN1200_BITL_PHY_CTRL_2_DN_SPEED_TO_100_EN           (1U)
#define ADIN1200_BITM_PHY_CTRL_2_DN_SPEED_TO_100_EN           (0X0800U)

#define ADIN1200_BITP_PHY_CTRL_2_DN_SPEED_TO_10_EN            (10U)          /* DN_SPEED_TO_10_EN */
#define ADIN1200_BITL_PHY_CTRL_2_DN_SPEED_TO_10_EN            (1U)
#define ADIN1200_BITM_PHY_CTRL_2_DN_SPEED_TO_10_EN            (0X0400U)

#define ADIN1200_BITP_PHY_CTRL_2_GROUP_MDIO_EN                (6U)           /* GROUP_MDIO_EN */
#define ADIN1200_BITL_PHY_CTRL_2_GROUP_MDIO_EN                (1U)
#define ADIN1200_BITM_PHY_CTRL_2_GROUP_MDIO_EN                (0X0040U)

#define ADIN1200_BITP_PHY_CTRL_2_CLK_CNTRL                    (1U)           /* CLK_CNTRL */
#define ADIN1200_BITL_PHY_CTRL_2_CLK_CNTRL                    (3U)
#define ADIN1200_BITM_PHY_CTRL_2_CLK_CNTRL                    (0X000EU)

/* ----------------------------------------------------------------------------------------------------
          PHY_CTRL_3                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_PHY_CTRL_3_LINK_EN                      (13U)          /* Setting this bit enables linking*/
#define ADIN1200_BITL_PHY_CTRL_3_LINK_EN                      (1U)
#define ADIN1200_BITM_PHY_CTRL_3_LINK_EN                      (0X2000U)

#define ADIN1200_BITP_PHY_CTRL_3_NUM_SPEED_RETRY              (10U)          /* NUM_SPEED_RETRY*/
#define ADIN1200_BITL_PHY_CTRL_3_NUM_SPEED_RETRY              (3U)
#define ADIN1200_BITM_PHY_CTRL_3_NUM_SPEED_RETRY              (0X1C00U)
/* ----------------------------------------------------------------------------------------------------
          IRQ_MASK                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_IRQ_MASK_CBL_DIAG_IRQ_EN                (10U)          /* CBL_DIAG_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_CBL_DIAG_IRQ_EN                (1U)
#define ADIN1200_BITM_IRQ_MASK_CBL_DIAG_IRQ_EN                (0X0400U)

#define ADIN1200_BITP_IRQ_MASK_MDIO_SYNC_IRQ_EN               (9U)           /* MDIO_SYNC_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_MDIO_SYNC_IRQ_EN               (1U)
#define ADIN1200_BITM_IRQ_MASK_MDIO_SYNC_IRQ_EN               (0X0200U)

#define ADIN1200_BITP_IRQ_MASK_AN_STAT_CHNG_IRQ_EN            (8U)           /* AN_STAT_CHNG_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_AN_STAT_CHNG_IRQ_EN            (1U)
#define ADIN1200_BITM_IRQ_MASK_AN_STAT_CHNG_IRQ_EN            (0X0100U)

#define ADIN1200_BITP_IRQ_MASK_FC_FG_IRQ_EN                   (7U)           /* FC_FG_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_FC_FG_IRQ_EN                   (1U)
#define ADIN1200_BITM_IRQ_MASK_FC_FG_IRQ_EN                   (0X0080U)

#define ADIN1200_BITP_IRQ_MASK_PAGE_RX_IRQ_EN                 (6U)           /* PAGE_RX_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_PAGE_RX_IRQ_EN                 (1U)
#define ADIN1200_BITM_IRQ_MASK_PAGE_RX_IRQ_EN                 (0X0040U)

#define ADIN1200_BITP_IRQ_MASK_IDLE_ERR_CNT_IRQ_EN            (5U)           /* IDLE_ERR_CNT_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_IDLE_ERR_CNT_IRQ_EN            (1U)
#define ADIN1200_BITM_IRQ_MASK_IDLE_ERR_CNT_IRQ_EN            (0X0020U)

#define ADIN1200_BITP_IRQ_MASK_FIFO_OU_IRQ_EN                 (4U)           /* FIFO_OU_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_FIFO_OU_IRQ_EN                 (1U)
#define ADIN1200_BITM_IRQ_MASK_FIFO_OU_IRQ_EN                 (0X0010U)

#define ADIN1200_BITP_IRQ_MASK_RX_STAT_CHNG_IRQ_EN            (3U)           /* RX_STAT_CHNG_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_RX_STAT_CHNG_IRQ_EN            (1U)
#define ADIN1200_BITM_IRQ_MASK_RX_STAT_CHNG_IRQ_EN            (0X0008U)

#define ADIN1200_BITP_IRQ_MASK_LNK_STAT_CHNG_IRQ_EN           (2U)           /* Link Status Changed Interrupt Enable Bit */
#define ADIN1200_BITL_IRQ_MASK_LNK_STAT_CHNG_IRQ_EN           (1U)
#define ADIN1200_BITM_IRQ_MASK_LNK_STAT_CHNG_IRQ_EN           (0X0004U)

#define ADIN1200_BITP_IRQ_MASK_SPEED_CHNG_IRQ_EN              (1U)           /* Speed Changed Interrupt Enable Bit. */
#define ADIN1200_BITL_IRQ_MASK_SPEED_CHNG_IRQ_EN              (1U)
#define ADIN1200_BITM_IRQ_MASK_SPEED_CHNG_IRQ_EN              (0X0002U)

#define ADIN1200_BITP_IRQ_MASK_HW_IRQ_EN                      (0U)           /* Harware Interrupt pin */
#define ADIN1200_BITL_IRQ_MASK_HW_IRQ_EN                      (1U)
#define ADIN1200_BITM_IRQ_MASK_HW_IRQ_EN                      (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          IRQ_STATUS                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_IRQ_MASK_CBL_DIAG_IRQ_STAT              (10U)          /* CBL_DIAG_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_CBL_DIAG_IRQ_STAT              (1U)
#define ADIN1200_BITM_IRQ_MASK_CBL_DIAG_IRQ_STAT              (0X0400U)

#define ADIN1200_BITP_IRQ_MASK_MDIO_SYNC_IRQ_STAT             (9U)           /* MDIO_SYNC_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_MDIO_SYNC_IRQ_STAT             (1U)
#define ADIN1200_BITM_IRQ_MASK_MDIO_SYNC_IRQ_STAT             (0X0200U)

#define ADIN1200_BITP_IRQ_MASK_AN_STAT_CHNG_IRQ_STAT          (8U)           /* AN_STAT_CHNG_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_AN_STAT_CHNG_IRQ_STAT          (1U)
#define ADIN1200_BITM_IRQ_MASK_AN_STAT_CHNG_IRQ_STAT          (0X0100U)

#define ADIN1200_BITP_IRQ_MASK_FC_FG_IRQ_STAT                 (7U)           /* FC_FG_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_FC_FG_IRQ_STAT                 (1U)
#define ADIN1200_BITM_IRQ_MASK_FC_FG_IRQ_STAT                 (0X0080U)

#define ADIN1200_BITP_IRQ_MASK_PAGE_RX_IRQ_STAT               (6U)           /* PAGE_RX_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_PAGE_RX_IRQ_STAT               (1U)
#define ADIN1200_BITM_IRQ_MASK_PAGE_RX_IRQ_STAT               (0X0040U)

#define ADIN1200_BITP_IRQ_MASK_IDLE_ERR_CNT_IRQ_STAT          (5U)           /* IDLE_ERR_CNT_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_IDLE_ERR_CNT_IRQ_STAT          (1U)
#define ADIN1200_BITM_IRQ_MASK_IDLE_ERR_CNT_IRQ_STAT          (0X0020U)

#define ADIN1200_BITP_IRQ_MASK_FIFO_OU_IRQ_STAT               (4U)           /* FIFO_OU_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_FIFO_OU_IRQ_STAT               (1U)
#define ADIN1200_BITM_IRQ_MASK_FIFO_OU_IRQ_STAT               (0X0010U)

#define ADIN1200_BITP_IRQ_MASK_RX_STAT_CHNG_IRQ_STAT          (3U)           /* RX_STAT_CHNG_IRQ_EN */
#define ADIN1200_BITL_IRQ_MASK_RX_STAT_CHNG_IRQ_STAT          (1U)
#define ADIN1200_BITM_IRQ_MASK_RX_STAT_CHNG_IRQ_STAT          (0X0008U)

#define ADIN1200_BITP_IRQ_MASK_LNK_STAT_CHNG_IRQ_STAT         (2U)           /* Link Status Changed Interrupt Enable Bit */
#define ADIN1200_BITL_IRQ_MASK_LNK_STAT_CHNG_IRQ_STAT         (1U)
#define ADIN1200_BITM_IRQ_MASK_LNK_STAT_CHNG_IRQ_STAT         (0X0004U)

#define ADIN1200_BITP_IRQ_MASK_SPEED_CHNG_IRQ_STAT            (1U)           /* Speed Changed Interrupt Enable Bit. */
#define ADIN1200_BITL_IRQ_MASK_SPEED_CHNG_IRQ_STAT            (1U)
#define ADIN1200_BITM_IRQ_MASK_SPEED_CHNG_IRQ_STAT            (0X0002U)

#define ADIN1200_BITP_IRQ_STATUS_IRQ_PENDING                  (0U)           /* Interrupt pending status bit */
#define ADIN1200_BITL_IRQ_STATUS_IRQ_PENDING                  (1U)
#define ADIN1200_BITM_IRQ_STATUS_IRQ_PENDING                  (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          PHY_STATUS_1                               Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_PHY_STATUS_1_PHY_IN_STNDBY              (15U)          /* PHY_IN_STNDBY */
#define ADIN1200_BITL_PHY_STATUS_1_PHY_IN_STNDBY              (1U)
#define ADIN1200_BITM_PHY_STATUS_1_PHY_IN_STNDBY              (0X8000U)

#define ADIN1200_BITP_PHY_STATUS_1_MSTR_SLV_FLT_STAT          (14U)          /* MSTR_SLV_FLT_STAT - Only in ADINPHY 1G */
#define ADIN1200_BITL_PHY_STATUS_1_MSTR_SLV_FLT_STAT          (1U)
#define ADIN1200_BITM_PHY_STATUS_1_MSTR_SLV_FLT_STAT          (0X4000U)

#define ADIN1200_BITP_PHY_STATUS_1_PAR_DET_FLT_STAT           (13U)          /* PAR_DET_FLT_STAT */
#define ADIN1200_BITL_PHY_STATUS_1_PAR_DET_FLT_STAT           (1U)
#define ADIN1200_BITM_PHY_STATUS_1_PAR_DET_FLT_STAT           (0X2000U)

#define ADIN1200_BITP_PHY_STATUS_1_AUTONEG_STAT               (12U)          /* AUTONEG_STAT */
#define ADIN1200_BITL_PHY_STATUS_1_AUTONEG_STAT               (1U)
#define ADIN1200_BITM_PHY_STATUS_1_AUTONEG_STAT               (0X1000U)

#define ADIN1200_BITP_PHY_STATUS_1_PAIR_01_SWAP               (11U)          /* PAIR_01_SWAP */
#define ADIN1200_BITL_PHY_STATUS_1_PAIR_01_SWAP               (1U)
#define ADIN1200_BITM_PHY_STATUS_1_PAIR_01_SWAP               (0X0800U)

#define ADIN1200_BITP_PHY_STATUS_1_B_10_POL_INV               (10U)          /* B_10_POL_INV */
#define ADIN1200_BITL_PHY_STATUS_1_B_10_POL_INV               (1U)
#define ADIN1200_BITM_PHY_STATUS_1_B_10_POL_INV               (0X0400U)

#define ADIN1200_BITP_PHY_STATUS_1_HCD_TECH                   (7U)           /* HCD_TECH */
#define ADIN1200_BITL_PHY_STATUS_1_HCD_TECH                   (3U)
#define ADIN1200_BITM_PHY_STATUS_1_HCD_TECH                   (0X0380U)

#define ADIN1200_BITP_PHY_STATUS_1_LINK_STAT                  (6U)           /* LINK_STAT */
#define ADIN1200_BITL_PHY_STATUS_1_LINK_STAT                  (1U)
#define ADIN1200_BITM_PHY_STATUS_1_LINK_STAT                  (0X0040U)

#define ADIN1200_BITP_PHY_STATUS_1_TX_EN_STAT                 (5U)           /* TX_EN_STAT*/
#define ADIN1200_BITL_PHY_STATUS_1_TX_EN_STAT                 (1U)
#define ADIN1200_BITM_PHY_STATUS_1_TX_EN_STAT                 (0X0020U)

#define ADIN1200_BITP_PHY_STATUS_1_RX_DV_STAT                 (4U)           /* RX_DV_STAT */
#define ADIN1200_BITL_PHY_STATUS_1_RX_DV_STAT                 (1U)
#define ADIN1200_BITM_PHY_STATUS_1_RX_DV_STAT                 (0X0010U)

#define ADIN1200_BITP_PHY_STATUS_1_COL_STAT                   (3U)           /* COL_STAT */
#define ADIN1200_BITL_PHY_STATUS_1_COL_STAT                   (1U)
#define ADIN1200_BITM_PHY_STATUS_1_COL_STAT                   (0X0008U)

#define ADIN1200_BITP_PHY_STATUS_1_AUTONEG_SUP                (2U)           /* AUTONEG_SUP */
#define ADIN1200_BITL_PHY_STATUS_1_AUTONEG_SUP                (1U)
#define ADIN1200_BITM_PHY_STATUS_1_AUTONEG_SUP                (0X0004U)

#define ADIN1200_BITP_PHY_STATUS_1_LP_PAUSE_ADV               (1U)           /* LP_PAUSE_ADV */
#define ADIN1200_BITL_PHY_STATUS_1_LP_PAUSE_ADV               (1U)
#define ADIN1200_BITM_PHY_STATUS_1_LP_PAUSE_ADV               (0X0002U)

#define ADIN1200_BITP_PHY_STATUS_1_LP_APAUSE_ADV              (0U)           /* LP_APAUSE_ADV */
#define ADIN1200_BITL_PHY_STATUS_1_LP_APAUSE_ADV              (1U)
#define ADIN1200_BITM_PHY_STATUS_1_LP_APAUSE_ADV              (0X0001U)

/* ----------------------------------------------------------------------------------------------------
          LED Control 1 Register                     Value             Description
  ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_LED_CTRL_1_LED_A_EXT_CFG_EN             (10U)          /* LED_A_EXT_CFG_EN */
#define ADIN1200_BITL_LED_CTRL_1_LED_A_EXT_CFG_EN             (1U)
#define ADIN1200_BITM_LED_CTRL_1_LED_A_EXT_CFG_EN             (0X0400U)

#define ADIN1200_BITP_LED_CTRL_1_LED_PAT_PAUSE_DUR            (4U)           /* LED_PAT_PAUSE_DUR */
#define ADIN1200_BITL_LED_CTRL_1_LED_PAT_PAUSE_DUR            (4U)
#define ADIN1200_BITM_LED_CTRL_1_LED_PAT_PAUSE_DUR            (0X00F0U)

#define ADIN1200_BITP_LED_CTRL_1_LED_PUL_STR_DUR_SEL          (2U)           /* LED_PUL_STR_DUR_SEL */
#define ADIN1200_BITL_LED_CTRL_1_LED_PUL_STR_DUR_SEL          (2U)
#define ADIN1200_BITM_LED_CTRL_1_LED_PUL_STR_DUR_SEL          (0X000CU)

#define ADIN1200_BITP_LED_CTRL_1_LED_OE_N                     (1U)           /* LED_OE_N */
#define ADIN1200_BITL_LED_CTRL_1_LED_OE_N                     (1U)
#define ADIN1200_BITM_LED_CTRL_1_LED_OE_N                     (0X0002U)

#define ADIN1200_BITP_LED_CTRL_1_LED_PUL_STR_EN               (0U)           /* LED_PUL_STR_ENs */
#define ADIN1200_BITL_LED_CTRL_1_LED_PUL_STR_EN               (1U)
#define ADIN1200_BITM_LED_CTRL_1_LED_PUL_STR_EN               (0X0001U)

/*----------------------------------------------------------------------------------------------------
        LED Control 2 Register                       Value           Description
---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_LED_CTRL_2_LED_A_CFG                    (0U)           /* LED_A_CFG s*/
#define ADIN1200_BITL_LED_CTRL_2_LED_A_CFG                    (4U)
#define ADIN1200_BITM_LED_CTRL_2_LED_A_CFG                    (0X000FU)


/*----------------------------------------------------------------------------------------------------
        LED Control 3 Register                       Value             Description
---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_LED_CTRL_3_LED_PAT_SEL                  (14U)          /* LED_PAT_SEL */
#define ADIN1200_BITL_LED_CTRL_3_LED_PAT_SEL                  (2U)
#define ADIN1200_BITM_LED_CTRL_3_LED_PAT_SEL                  (0XC000U)

#define ADIN1200_BITP_LED_CTRL_3_LED_PAT_TICK_DUR             (8U)           /* LED_PAT_TICK_DUR */
#define ADIN1200_BITL_LED_CTRL_3_LED_PAT_TICK_DUR             (6U)
#define ADIN1200_BITM_LED_CTRL_3_LED_PAT_TICK_DUR             (0X3F00U)

#define ADIN1200_BITP_LED_CTRL_3_LED_PAT                      (0U)           /* LED_PAT */
#define ADIN1200_BITL_LED_CTRL_3_LED_PAT                      (8U)
#define ADIN1200_BITM_LED_CTRL_3_LED_PAT                      (0X00FFU)

/* ----------------------------------------------------------------------------------------------------
          PHY_STATUS_2                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_PHY_STATUS_2_PAIR_23_SWAP               (14U)          /* PAIR_23_SWAP - Only in ADINPHY 1G**/
#define ADIN1200_BITL_PHY_STATUS_2_PAIR_23_SWAP               (1U)
#define ADIN1200_BITM_PHY_STATUS_2_PAIR_23_SWAP               (0X3000U)

#define ADIN1200_BITP_PHY_STATUS_2_PAIR_3_POL_INV             (13U)          /* PAIR_3_POL_INV - Only in ADINPHY 1G**/
#define ADIN1200_BITL_PHY_STATUS_2_PAIR_3_POL_INV             (1U)
#define ADIN1200_BITM_PHY_STATUS_2_PAIR_3_POL_INV             (0X2000U)

#define ADIN1200_BITP_PHY_STATUS_2_PAIR_2_POL_INV             (12U)          /* PAIR_2_POL_INV - Only in ADINPHY 1G**/
#define ADIN1200_BITL_PHY_STATUS_2_PAIR_2_POL_INV             (1U)
#define ADIN1200_BITM_PHY_STATUS_2_PAIR_2_POL_INV             (0X1000U)

#define ADIN1200_BITP_PHY_STATUS_2_PAIR_1_POL_INV             (11U)          /* PHY_STATUS_2_PAIR_1_POL_INV */
#define ADIN1200_BITL_PHY_STATUS_2_PAIR_1_POL_INV             (1U)
#define ADIN1200_BITM_PHY_STATUS_2_PAIR_1_POL_INV             (0X0800U)

#define ADIN1200_BITP_PHY_STATUS_2_PAIR_0_POL_INV             (10U)          /* PHY_STATUS_2_PAIR_0_POL_INV */
#define ADIN1200_BITL_PHY_STATUS_2_PAIR_0_POL_INV             (1U)
#define ADIN1200_BITM_PHY_STATUS_2_PAIR_0_POL_INV             (0X0400U)

#define ADIN1200_BITP_PHY_STATUS_2_B_1000_DSCR_ACQ_ERR        (0U)           /* B_1000_DSCR_ACQ_ERR - Only in ADINPHY 1G*/
#define ADIN1200_BITL_PHY_STATUS_2_B_1000_DSCR_ACQ_ERR        (1U)
#define ADIN1200_BITM_PHY_STATUS_2_B_1000_DSCR_ACQ_ERR        (0X001U)

/* ====================================================================================================
        ADINPHY 1G Module Register BitPositions, Lengths, Masks and Enumerations Definitions
   ==================================================================================================== */

/* ----------------------------------------------------------------------------------------------------
          MSTR_SLV_CONTROL                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define ADIN1200_BITP_MSTR_SLV_CONTROL_TST_MODE               (13U)          /* TST_MODE */
#define ADIN1200_BITL_MSTR_SLV_CONTROL_TST_MODE               (3U)
#define ADIN1200_BITM_MSTR_SLV_CONTROL_TST_MODE               (0XE000U)

#define ADIN1200_BITP_MSTR_SLV_CONTROL_MAN_MSTR_SLV_EN_ADV    (12U)          /* MAN_MSTR_SLV_EN_ADV */
#define ADIN1200_BITL_MSTR_SLV_CONTROL_MAN_MSTR_SLV_EN_ADV    (1U)
#define ADIN1200_BITM_MSTR_SLV_CONTROL_MAN_MSTR_SLV_EN_ADV    (0X1000U)

#define ADIN1200_BITP_MSTR_SLV_CONTROL_MAN_MSTR_ADV           (11U)          /* MAN_MSTR_ADV */
#define ADIN1200_BITL_MSTR_SLV_CONTROL_MAN_MSTR_ADV           (1U)
#define ADIN1200_BITM_MSTR_SLV_CONTROL_MAN_MSTR_ADV           (0X0800U)

#define ADIN1200_BITP_MSTR_SLV_CONTROL_PREF_MSTR_ADV          (10U)          /* PREF_MSTR_ADV */
#define ADIN1200_BITL_MSTR_SLV_CONTROL_PREF_MSTR_ADV          (1U)
#define ADIN1200_BITM_MSTR_SLV_CONTROL_PREF_MSTR_ADV          (0X0400U)

#define ADIN1200_BITP_MSTR_SLV_CONTROL_FD_1000_ADV            (9U)           /* FD_1000_ADV */
#define ADIN1200_BITL_MSTR_SLV_CONTROL_FD_1000_ADV            (1U)
#define ADIN1200_BITM_MSTR_SLV_CONTROL_FD_1000_ADV            (0X0200U)

#define ADIN1200_BITP_MSTR_SLV_CONTROL_HD_1000_ADV            (8U)           /* HD_1000_ADV */
#define ADIN1200_BITL_MSTR_SLV_CONTROL_HD_1000_ADV            (1U)
#define ADIN1200_BITM_MSTR_SLV_CONTROL_HD_1000_ADV            (0X0100U)


#endif /* ADIN_PHY_ADDR_RDEF_22_H */

/*! @endcond */
