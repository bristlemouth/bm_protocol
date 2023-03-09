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
#define GPIO2_Pin GPIO_PIN_13
#define GPIO2_GPIO_Port GPIOC
#define GPIO1_Pin GPIO_PIN_0
#define GPIO1_GPIO_Port GPIOH
#define ADIN_PWR_Pin GPIO_PIN_1
#define ADIN_PWR_GPIO_Port GPIOH
#define BM_CS_Pin GPIO_PIN_4
#define BM_CS_GPIO_Port GPIOA
#define BM_SCK_RX3_Pin GPIO_PIN_5
#define BM_SCK_RX3_GPIO_Port GPIOA
#define BM_MISO_Pin GPIO_PIN_6
#define BM_MISO_GPIO_Port GPIOA
#define BM_MOSI_TX3_Pin GPIO_PIN_7
#define BM_MOSI_TX3_GPIO_Port GPIOA
#define BM_INT_Pin GPIO_PIN_0
#define BM_INT_GPIO_Port GPIOB
#define FLASH_SCK_Pin GPIO_PIN_13
#define FLASH_SCK_GPIO_Port GPIOB
#define FLASH_MISO_Pin GPIO_PIN_14
#define FLASH_MISO_GPIO_Port GPIOB
#define FLASH_MOSI_Pin GPIO_PIN_15
#define FLASH_MOSI_GPIO_Port GPIOB
#define FLASH_CS_Pin GPIO_PIN_8
#define FLASH_CS_GPIO_Port GPIOA
#define ADIN_NSS_Pin GPIO_PIN_15
#define ADIN_NSS_GPIO_Port GPIOA
#define ADIN_SCK_Pin GPIO_PIN_3
#define ADIN_SCK_GPIO_Port GPIOB
#define ADIN_MISO_Pin GPIO_PIN_4
#define ADIN_MISO_GPIO_Port GPIOB
#define ADIN_MOSI_Pin GPIO_PIN_5
#define ADIN_MOSI_GPIO_Port GPIOB
#define BOOT_LED_Pin GPIO_PIN_3
#define BOOT_LED_GPIO_Port GPIOH
#define ADIN_INT_Pin GPIO_PIN_8
#define ADIN_INT_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
