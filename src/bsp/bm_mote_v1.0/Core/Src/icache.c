/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    icache.c
  * @brief   This file provides code for the configuration
  *          of the ICACHE instances.
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
/* Includes ------------------------------------------------------------------*/
#include "icache.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* ICACHE init function */
void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  ICACHE_RegionConfigTypeDef pRegionConfig = {0};

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Configure and enable a region for memory remapping.
  */
  if (HAL_ICACHE_Disable() != HAL_OK)
  {
    Error_Handler();
  }
  pRegionConfig.BaseAddress = 0x0;
  pRegionConfig.RemapAddress = 0x0;
  pRegionConfig.Size = ICACHE_REGIONSIZE_2MB;
  pRegionConfig.TrafficRoute = ICACHE_MASTER1_PORT;
  pRegionConfig.OutputBurstType = ICACHE_OUTPUT_BURST_WRAP;
  if (HAL_ICACHE_EnableRemapRegion(ICACHE_REGION_0, &pRegionConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
