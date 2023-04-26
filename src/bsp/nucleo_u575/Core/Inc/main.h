/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"

#include "stm32u5xx_ll_iwdg.h"
#include "stm32u5xx_ll_rtc.h"
#include "stm32u5xx_ll_ucpd.h"
#include "stm32u5xx_ll_usart.h"
#include "stm32u5xx_ll_rcc.h"
#include "stm32u5xx_ll_system.h"
#include "stm32u5xx_ll_gpio.h"
#include "stm32u5xx_ll_exti.h"
#include "stm32u5xx_ll_lpgpio.h"
#include "stm32u5xx_ll_bus.h"
#include "stm32u5xx_ll_cortex.h"
#include "stm32u5xx_ll_utils.h"
#include "stm32u5xx_ll_pwr.h"
#include "stm32u5xx_ll_dma.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void SystemClock_Config(void);
void SystemPower_Config_ext(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define USER_BUTTON_Pin LL_GPIO_PIN_13
#define USER_BUTTON_GPIO_Port GPIOC
#define USER_BUTTON_EXTI_IRQn EXTI13_IRQn
#define VBUS_SENSE_Pin LL_GPIO_PIN_2
#define VBUS_SENSE_GPIO_Port GPIOC
#define FLASH_CS_Pin LL_GPIO_PIN_1
#define FLASH_CS_GPIO_Port GPIOA
#define ALARM_OUT_Pin LL_GPIO_PIN_3
#define ALARM_OUT_GPIO_Port GPIOA
#define ADIN_CS_Pin LL_GPIO_PIN_4
#define ADIN_CS_GPIO_Port GPIOA
#define ADIN_RST_Pin LL_GPIO_PIN_12
#define ADIN_RST_GPIO_Port GPIOF
#define UCPD_FLT_Pin LL_GPIO_PIN_14
#define UCPD_FLT_GPIO_Port GPIOB
#define UCPD1_CC2_Pin LL_GPIO_PIN_15
#define UCPD1_CC2_GPIO_Port GPIOB
#define ADIN_RDY_Pin LL_GPIO_PIN_15
#define ADIN_RDY_GPIO_Port GPIOD
#define ADIN_RDY_EXTI_IRQn EXTI15_IRQn
#define LED_RED_Pin LL_GPIO_PIN_2
#define LED_RED_GPIO_Port GPIOG
#define LED_GREEN_Pin LL_GPIO_PIN_7
#define LED_GREEN_GPIO_Port GPIOC
#define USART1_TX_Pin LL_GPIO_PIN_9
#define USART1_TX_GPIO_Port GPIOA
#define USART1_RX_Pin LL_GPIO_PIN_10
#define USART1_RX_GPIO_Port GPIOA
#define USB_OTG_FS_DM_Pin LL_GPIO_PIN_11
#define USB_OTG_FS_DM_GPIO_Port GPIOA
#define USB_OTG_FS_DP_Pin LL_GPIO_PIN_12
#define USB_OTG_FS_DP_GPIO_Port GPIOA
#define UCPD1_CC1_Pin LL_GPIO_PIN_15
#define UCPD1_CC1_GPIO_Port GPIOA
#define UCPD_DBn_Pin LL_GPIO_PIN_5
#define UCPD_DBn_GPIO_Port GPIOB
#define LED_BLUE_Pin LL_GPIO_PIN_7
#define LED_BLUE_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
