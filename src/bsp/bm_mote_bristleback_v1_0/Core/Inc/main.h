/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "stm32u5xx_ll_lpuart.h"
#include "stm32u5xx_ll_rcc.h"
#include "stm32u5xx_ll_rtc.h"
#include "stm32u5xx_ll_usart.h"
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
#define LED_GREEN_Pin LL_GPIO_PIN_13
#define LED_GREEN_GPIO_Port GPIOC
#define GPIO1_Pin LL_GPIO_PIN_0
#define GPIO1_GPIO_Port GPIOH
#define ADIN_PWR_Pin LL_GPIO_PIN_1
#define ADIN_PWR_GPIO_Port GPIOH
#define ADIN_RST_Pin LL_GPIO_PIN_0
#define ADIN_RST_GPIO_Port GPIOA
#define LED_BLUE_Pin LL_GPIO_PIN_1
#define LED_BLUE_GPIO_Port GPIOA
#define PAYLOAD_TX_Pin LL_GPIO_PIN_2
#define PAYLOAD_TX_GPIO_Port GPIOA
#define PAYLOAD_RX_Pin LL_GPIO_PIN_3
#define PAYLOAD_RX_GPIO_Port GPIOA
#define BB_PL_BUCK_EN_Pin LL_GPIO_PIN_4
#define BB_PL_BUCK_EN_GPIO_Port GPIOA
#define BM_SCK_RX3_Pin LL_GPIO_PIN_5
#define BM_SCK_RX3_GPIO_Port GPIOA
#define BM_MISO_Pin LL_GPIO_PIN_6
#define BM_MISO_GPIO_Port GPIOA
#define BM_MOSI_TX3_Pin LL_GPIO_PIN_7
#define BM_MOSI_TX3_GPIO_Port GPIOA
#define LED_RED_Pin LL_GPIO_PIN_0
#define LED_RED_GPIO_Port GPIOB
#define BB_VBUS_EN_Pin LL_GPIO_PIN_1
#define BB_VBUS_EN_GPIO_Port GPIOB
#define FLASH_SCK_Pin LL_GPIO_PIN_13
#define FLASH_SCK_GPIO_Port GPIOB
#define FLASH_MISO_Pin LL_GPIO_PIN_14
#define FLASH_MISO_GPIO_Port GPIOB
#define FLASH_MOSI_Pin LL_GPIO_PIN_15
#define FLASH_MOSI_GPIO_Port GPIOB
#define FLASH_CS_Pin LL_GPIO_PIN_8
#define FLASH_CS_GPIO_Port GPIOA
#define VUSB_DETECT_Pin LL_GPIO_PIN_9
#define VUSB_DETECT_GPIO_Port GPIOA
#define VUSB_DETECT_EXTI_IRQn EXTI9_IRQn
#define BB_3V3_EN_Pin LL_GPIO_PIN_10
#define BB_3V3_EN_GPIO_Port GPIOA
#define ADIN_CS_Pin LL_GPIO_PIN_15
#define ADIN_CS_GPIO_Port GPIOA
#define ADIN_SCK_Pin LL_GPIO_PIN_3
#define ADIN_SCK_GPIO_Port GPIOB
#define ADIN_MISO_Pin LL_GPIO_PIN_4
#define ADIN_MISO_GPIO_Port GPIOB
#define ADIN_MOSI_Pin LL_GPIO_PIN_5
#define ADIN_MOSI_GPIO_Port GPIOB
#define BOOT_LED_Pin LL_GPIO_PIN_3
#define BOOT_LED_GPIO_Port GPIOH
#define ADIN_INT_Pin LL_GPIO_PIN_8
#define ADIN_INT_GPIO_Port GPIOB
#define ADIN_INT_EXTI_IRQn EXTI8_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
