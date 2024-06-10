/******************************************************************************
 * @file     startup_Device.c
 * @brief    CMSIS-Core(M) Device Startup File for <Device>
 * @version  V2.0.0
 * @date     20. May 2019
 ******************************************************************************/
/*
 * Copyright (c) 2009-2019 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "stm32u575xx.h"
#include "FreeRTOS.h"

/*----------------------------------------------------------------------------
  External References
 *----------------------------------------------------------------------------*/
extern unsigned long _sidata;   /* start address for the initialization values of the .data section. defined in linker script */
extern unsigned long _sdata;    /* start address for the .data section. defined in linker script */
extern unsigned long _edata;    /* end address for the .data section. defined in linker script */

extern unsigned long _sbss;     /* start address for the .bss section. defined in linker script */
extern unsigned long _ebss;     /* end address for the .bss section. defined in linker script */

extern uint32_t _vtable;

extern unsigned long *_estack;        /* init value for the stack pointer. defined in linker script */

extern int main(void);
extern void __libc_init_array(void);

/*----------------------------------------------------------------------------
  Internal References
 *----------------------------------------------------------------------------*/
// void __NO_RETURN Default_Handler(void);
void __NO_RETURN Reset_Handler  (void);


/*----------------------------------------------------------------------------
  Exception / Interrupt Handler
 *----------------------------------------------------------------------------*/
/* Exceptions */
void NMI_Handler                    (void) __attribute__ ((weak, alias("Default_Handler")));
void HardFault_Handler              (void) __attribute__ ((weak, alias("Default_Handler")));
void MemManage_Handler              (void) __attribute__ ((weak, alias("Default_Handler")));
void BusFault_Handler               (void) __attribute__ ((weak, alias("Default_Handler")));
void UsageFault_Handler             (void) __attribute__ ((weak, alias("Default_Handler")));
void SVC_Handler                    (void) __attribute__ ((weak, alias("Default_Handler")));
void DebugMon_Handler               (void) __attribute__ ((weak, alias("Default_Handler")));
void PendSV_Handler                 (void) __attribute__ ((weak, alias("Default_Handler")));
void SysTick_Handler                (void) __attribute__ ((weak, alias("Default_Handler")));

void WWDG_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void PVD_PVM_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void RTC_IRQHandler               (void) __attribute__ ((weak, alias("Default_Handler")));
void RTC_S_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void TAMP_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void RAMCFG_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void FLASH_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void FLASH_S_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void GTZC_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void RCC_IRQHandler               (void) __attribute__ ((weak, alias("Default_Handler")));
void RCC_S_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI0_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI1_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI2_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI3_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI4_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI5_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI6_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI7_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI8_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI9_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI10_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI11_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI12_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI13_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI14_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI15_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void IWDG_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel0_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel1_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel2_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel3_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel4_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel5_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel6_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel7_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void ADC1_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void DAC1_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void FDCAN1_IT0_IRQHandler        (void) __attribute__ ((weak, alias("Default_Handler")));
void FDCAN1_IT1_IRQHandler        (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM1_BRK_IRQHandler          (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM1_UP_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM1_TRG_COM_IRQHandler      (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM1_CC_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM2_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM3_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM4_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM5_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM6_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM7_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM8_BRK_IRQHandler          (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM8_UP_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM8_TRG_COM_IRQHandler      (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM8_CC_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void I2C1_EV_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void I2C1_ER_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void I2C2_EV_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void I2C2_ER_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void SPI1_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void SPI2_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void USART1_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void USART2_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void USART3_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void UART4_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void UART5_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void LPUART1_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void LPTIM1_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void LPTIM2_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM15_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM16_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void TIM17_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void COMP_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void OTG_FS_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void CRS_IRQHandler               (void) __attribute__ ((weak, alias("Default_Handler")));
void FMC_IRQHandler               (void) __attribute__ ((weak, alias("Default_Handler")));
void OCTOSPI1_IRQHandler          (void) __attribute__ ((weak, alias("Default_Handler")));
void PWR_S3WU_IRQHandler          (void) __attribute__ ((weak, alias("Default_Handler")));
void SDMMC1_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void SDMMC2_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel8_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel9_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel10_IRQHandler  (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel11_IRQHandler  (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel12_IRQHandler  (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel13_IRQHandler  (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel14_IRQHandler  (void) __attribute__ ((weak, alias("Default_Handler")));
void GPDMA1_Channel15_IRQHandler  (void) __attribute__ ((weak, alias("Default_Handler")));
void I2C3_EV_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void I2C3_ER_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void SAI1_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void SAI2_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void TSC_IRQHandler               (void) __attribute__ ((weak, alias("Default_Handler")));
void RNG_IRQHandler               (void) __attribute__ ((weak, alias("Default_Handler")));
void FPU_IRQHandler               (void) __attribute__ ((weak, alias("Default_Handler")));
void HASH_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void LPTIM3_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void SPI3_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void I2C4_ER_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void I2C4_EV_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void MDF1_FLT0_IRQHandler         (void) __attribute__ ((weak, alias("Default_Handler")));
void MDF1_FLT1_IRQHandler         (void) __attribute__ ((weak, alias("Default_Handler")));
void MDF1_FLT2_IRQHandler         (void) __attribute__ ((weak, alias("Default_Handler")));
void MDF1_FLT3_IRQHandler         (void) __attribute__ ((weak, alias("Default_Handler")));
void UCPD1_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void ICACHE_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void LPTIM4_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void DCACHE1_IRQHandler           (void) __attribute__ ((weak, alias("Default_Handler")));
void ADF1_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void ADC4_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));
void LPDMA1_Channel0_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void LPDMA1_Channel1_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void LPDMA1_Channel2_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void LPDMA1_Channel3_IRQHandler   (void) __attribute__ ((weak, alias("Default_Handler")));
void DMA2D_IRQHandler             (void) __attribute__ ((weak, alias("Default_Handler")));
void DCMI_PSSI_IRQHandler         (void) __attribute__ ((weak, alias("Default_Handler")));
void OCTOSPI2_IRQHandler          (void) __attribute__ ((weak, alias("Default_Handler")));
void MDF1_FLT4_IRQHandler         (void) __attribute__ ((weak, alias("Default_Handler")));
void MDF1_FLT5_IRQHandler         (void) __attribute__ ((weak, alias("Default_Handler")));
void CORDIC_IRQHandler            (void) __attribute__ ((weak, alias("Default_Handler")));
void FMAC_IRQHandler              (void) __attribute__ ((weak, alias("Default_Handler")));


//
// ISR Tracing notes:
// To enable you must:
// * define TRACE_ENABLE and TRACE_ISR for build
// * Uncomment ISR_TRACE_WRAPPER for functions you want to trace
// * Wrap each relevant IRQHandler in vector table with ISR_TRACE(xxxx_IRQHandler)
//
#ifdef TRACE_ISR
// ISR_TRACE_WRAPPER(WWDG_IRQHandler);
// ISR_TRACE_WRAPPER(PVD_PVM_IRQHandler);
// ISR_TRACE_WRAPPER(RTC_IRQHandler);
// ISR_TRACE_WRAPPER(RTC_S_IRQHandler);
// ISR_TRACE_WRAPPER(TAMP_IRQHandler);
// ISR_TRACE_WRAPPER(RAMCFG_IRQHandler);
// ISR_TRACE_WRAPPER(FLASH_IRQHandler);
// ISR_TRACE_WRAPPER(FLASH_S_IRQHandler);
// ISR_TRACE_WRAPPER(GTZC_IRQHandler);
// ISR_TRACE_WRAPPER(RCC_IRQHandler);
// ISR_TRACE_WRAPPER(RCC_S_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI0_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI1_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI2_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI3_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI4_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI5_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI6_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI7_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI8_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI9_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI10_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI11_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI12_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI13_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI14_IRQHandler);
// ISR_TRACE_WRAPPER(EXTI15_IRQHandler);
// ISR_TRACE_WRAPPER(IWDG_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel0_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel1_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel2_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel3_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel4_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel5_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel6_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel7_IRQHandler);
// ISR_TRACE_WRAPPER(ADC1_IRQHandler);
// ISR_TRACE_WRAPPER(DAC1_IRQHandler);
// ISR_TRACE_WRAPPER(FDCAN1_IT0_IRQHandler);
// ISR_TRACE_WRAPPER(FDCAN1_IT1_IRQHandler);
// ISR_TRACE_WRAPPER(TIM1_BRK_IRQHandler);
// ISR_TRACE_WRAPPER(TIM1_UP_IRQHandler);
// ISR_TRACE_WRAPPER(TIM1_TRG_COM_IRQHandler);
// ISR_TRACE_WRAPPER(TIM1_CC_IRQHandler);
// ISR_TRACE_WRAPPER(TIM2_IRQHandler);
// ISR_TRACE_WRAPPER(TIM3_IRQHandler);
// ISR_TRACE_WRAPPER(TIM4_IRQHandler);
// ISR_TRACE_WRAPPER(TIM5_IRQHandler);
// ISR_TRACE_WRAPPER(TIM6_IRQHandler);
// ISR_TRACE_WRAPPER(TIM7_IRQHandler);
// ISR_TRACE_WRAPPER(TIM8_BRK_IRQHandler);
// ISR_TRACE_WRAPPER(TIM8_UP_IRQHandler);
// ISR_TRACE_WRAPPER(TIM8_TRG_COM_IRQHandler);
// ISR_TRACE_WRAPPER(TIM8_CC_IRQHandler);
// ISR_TRACE_WRAPPER(I2C1_EV_IRQHandler);
// ISR_TRACE_WRAPPER(I2C1_ER_IRQHandler);
// ISR_TRACE_WRAPPER(I2C2_EV_IRQHandler);
// ISR_TRACE_WRAPPER(I2C2_ER_IRQHandler);
// ISR_TRACE_WRAPPER(SPI1_IRQHandler);
// ISR_TRACE_WRAPPER(SPI2_IRQHandler);
// ISR_TRACE_WRAPPER(USART1_IRQHandler);
// ISR_TRACE_WRAPPER(USART2_IRQHandler);
// ISR_TRACE_WRAPPER(USART3_IRQHandler);
// ISR_TRACE_WRAPPER(UART4_IRQHandler);
// ISR_TRACE_WRAPPER(UART5_IRQHandler);
// ISR_TRACE_WRAPPER(LPUART1_IRQHandler);
// ISR_TRACE_WRAPPER(LPTIM1_IRQHandler);
// ISR_TRACE_WRAPPER(LPTIM2_IRQHandler);
// ISR_TRACE_WRAPPER(TIM15_IRQHandler);
// ISR_TRACE_WRAPPER(TIM16_IRQHandler);
// ISR_TRACE_WRAPPER(TIM17_IRQHandler);
// ISR_TRACE_WRAPPER(COMP_IRQHandler);
// ISR_TRACE_WRAPPER(OTG_FS_IRQHandler);
// ISR_TRACE_WRAPPER(CRS_IRQHandler);
// ISR_TRACE_WRAPPER(FMC_IRQHandler);
// ISR_TRACE_WRAPPER(OCTOSPI1_IRQHandler);
// ISR_TRACE_WRAPPER(PWR_S3WU_IRQHandler);
// ISR_TRACE_WRAPPER(SDMMC1_IRQHandler);
// ISR_TRACE_WRAPPER(SDMMC2_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel8_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel9_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel10_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel11_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel12_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel13_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel14_IRQHandler);
// ISR_TRACE_WRAPPER(GPDMA1_Channel15_IRQHandler);
// ISR_TRACE_WRAPPER(I2C3_EV_IRQHandler);
// ISR_TRACE_WRAPPER(I2C3_ER_IRQHandler);
// ISR_TRACE_WRAPPER(SAI1_IRQHandler);
// ISR_TRACE_WRAPPER(SAI2_IRQHandler);
// ISR_TRACE_WRAPPER(TSC_IRQHandler);
// ISR_TRACE_WRAPPER(RNG_IRQHandler);
// ISR_TRACE_WRAPPER(FPU_IRQHandler);
// ISR_TRACE_WRAPPER(HASH_IRQHandler);
// ISR_TRACE_WRAPPER(LPTIM3_IRQHandler);
// ISR_TRACE_WRAPPER(SPI3_IRQHandler);
// ISR_TRACE_WRAPPER(I2C4_ER_IRQHandler);
// ISR_TRACE_WRAPPER(I2C4_EV_IRQHandler);
// ISR_TRACE_WRAPPER(MDF1_FLT0_IRQHandler);
// ISR_TRACE_WRAPPER(MDF1_FLT1_IRQHandler);
// ISR_TRACE_WRAPPER(MDF1_FLT2_IRQHandler);
// ISR_TRACE_WRAPPER(MDF1_FLT3_IRQHandler);
// ISR_TRACE_WRAPPER(UCPD1_IRQHandler);
// ISR_TRACE_WRAPPER(ICACHE_IRQHandler);
// ISR_TRACE_WRAPPER(LPTIM4_IRQHandler);
// ISR_TRACE_WRAPPER(DCACHE1_IRQHandler);
// ISR_TRACE_WRAPPER(ADF1_IRQHandler);
// ISR_TRACE_WRAPPER(ADC4_IRQHandler);
// ISR_TRACE_WRAPPER(LPDMA1_Channel0_IRQHandler);
// ISR_TRACE_WRAPPER(LPDMA1_Channel1_IRQHandler);
// ISR_TRACE_WRAPPER(LPDMA1_Channel2_IRQHandler);
// ISR_TRACE_WRAPPER(LPDMA1_Channel3_IRQHandler);
// ISR_TRACE_WRAPPER(DMA2D_IRQHandler);
// ISR_TRACE_WRAPPER(DCMI_PSSI_IRQHandler);
// ISR_TRACE_WRAPPER(OCTOSPI2_IRQHandler);
// ISR_TRACE_WRAPPER(MDF1_FLT4_IRQHandler);
// ISR_TRACE_WRAPPER(MDF1_FLT5_IRQHandler);
// ISR_TRACE_WRAPPER(CORDIC_IRQHandler);
// ISR_TRACE_WRAPPER(FMAC_IRQHandler);
#else
#define ISR_TRACE(NAME) NAME
#endif

/*----------------------------------------------------------------------------
  Exception / Interrupt Vector table
 *----------------------------------------------------------------------------*/
__attribute__ ((section(".isr_vector")))
  void (* const g_pfnVectors[])(void) = {
  (void *)&_estack,                         /*     Initial Stack Pointer */
  Reset_Handler,                            /*     Reset Handler */
  NMI_Handler,                              /* -14 NMI Handler */
  HardFault_Handler,                        /* -13 Hard Fault Handler */
  MemManage_Handler,                        /* -12 MPU Fault Handler */
  BusFault_Handler,                         /* -11 Bus Fault Handler */
  UsageFault_Handler,                       /* -10 Usage Fault Handler */
  0,                                        /*     Reserved */
  0,                                        /*     Reserved */
  0,                                        /*     Reserved */
  0,                                        /*     Reserved */
  SVC_Handler,                              /*  -5 SVCall Handler */
  DebugMon_Handler,                         /*  -4 Debug Monitor Handler */
  0,                                        /*     Reserved */
  PendSV_Handler,                           /*  -2 PendSV Handler */
  SysTick_Handler,                          /*  -1 SysTick Handler */

  /* Interrupts */
  WWDG_IRQHandler,
  PVD_PVM_IRQHandler,
  RTC_IRQHandler,
  RTC_S_IRQHandler,
  TAMP_IRQHandler,
  RAMCFG_IRQHandler,
  FLASH_IRQHandler,
  FLASH_S_IRQHandler,
  GTZC_IRQHandler,
  RCC_IRQHandler,
  RCC_S_IRQHandler,
  EXTI0_IRQHandler,
  EXTI1_IRQHandler,
  EXTI2_IRQHandler,
  EXTI3_IRQHandler,
  EXTI4_IRQHandler,
  EXTI5_IRQHandler,
  EXTI6_IRQHandler,
  EXTI7_IRQHandler,
  EXTI8_IRQHandler,
  EXTI9_IRQHandler,
  EXTI10_IRQHandler,
  EXTI11_IRQHandler,
  EXTI12_IRQHandler,
  EXTI13_IRQHandler,
  EXTI14_IRQHandler,
  EXTI15_IRQHandler,
  IWDG_IRQHandler,
  0,
  GPDMA1_Channel0_IRQHandler,
  GPDMA1_Channel1_IRQHandler,
  GPDMA1_Channel2_IRQHandler,
  GPDMA1_Channel3_IRQHandler,
  GPDMA1_Channel4_IRQHandler,
  GPDMA1_Channel5_IRQHandler,
  GPDMA1_Channel6_IRQHandler,
  GPDMA1_Channel7_IRQHandler,
  ADC1_IRQHandler,
  DAC1_IRQHandler,
  FDCAN1_IT0_IRQHandler,
  FDCAN1_IT1_IRQHandler,
  TIM1_BRK_IRQHandler,
  TIM1_UP_IRQHandler,
  TIM1_TRG_COM_IRQHandler,
  TIM1_CC_IRQHandler,
  TIM2_IRQHandler,
  TIM3_IRQHandler,
  TIM4_IRQHandler,
  TIM5_IRQHandler,
  TIM6_IRQHandler,
  TIM7_IRQHandler,
  TIM8_BRK_IRQHandler,
  TIM8_UP_IRQHandler,
  TIM8_TRG_COM_IRQHandler,
  TIM8_CC_IRQHandler,
  I2C1_EV_IRQHandler,
  I2C1_ER_IRQHandler,
  I2C2_EV_IRQHandler,
  I2C2_ER_IRQHandler,
  SPI1_IRQHandler,
  SPI2_IRQHandler,
  USART1_IRQHandler,
  USART2_IRQHandler,
  USART3_IRQHandler,
  UART4_IRQHandler,
  UART5_IRQHandler,
  LPUART1_IRQHandler,
  LPTIM1_IRQHandler,
  LPTIM2_IRQHandler,
  TIM15_IRQHandler,
  TIM16_IRQHandler,
  TIM17_IRQHandler,
  COMP_IRQHandler,
  OTG_FS_IRQHandler,
  CRS_IRQHandler,
  FMC_IRQHandler,
  OCTOSPI1_IRQHandler,
  PWR_S3WU_IRQHandler,
  SDMMC1_IRQHandler,
  SDMMC2_IRQHandler,
  GPDMA1_Channel8_IRQHandler,
  GPDMA1_Channel9_IRQHandler,
  GPDMA1_Channel10_IRQHandler,
  GPDMA1_Channel11_IRQHandler,
  GPDMA1_Channel12_IRQHandler,
  GPDMA1_Channel13_IRQHandler,
  GPDMA1_Channel14_IRQHandler,
  ISR_TRACEGPDMA1_Channel15_IRQHandler,
  I2C3_EV_IRQHandler,
  I2C3_ER_IRQHandler,
  SAI1_IRQHandler,
  SAI2_IRQHandler,
  TSC_IRQHandler,
  0,
  RNG_IRQHandler,
  FPU_IRQHandler,
  HASH_IRQHandler,
  0,
  LPTIM3_IRQHandler,
  SPI3_IRQHandler,
  I2C4_ER_IRQHandler,
  I2C4_EV_IRQHandler,
  MDF1_FLT0_IRQHandler,
  MDF1_FLT1_IRQHandler,
  MDF1_FLT2_IRQHandler,
  MDF1_FLT3_IRQHandler,
  UCPD1_IRQHandler,
  ICACHE_IRQHandler,
  0,
  0,
  LPTIM4_IRQHandler,
  DCACHE1_IRQHandler,
  ADF1_IRQHandler,
  ADC4_IRQHandler,
  LPDMA1_Channel0_IRQHandler,
  LPDMA1_Channel1_IRQHandler,
  LPDMA1_Channel2_IRQHandler,
  LPDMA1_Channel3_IRQHandler,
  DMA2D_IRQHandler,
  DCMI_PSSI_IRQHandler,
  OCTOSPI2_IRQHandler,
  MDF1_FLT4_IRQHandler,
  MDF1_FLT5_IRQHandler,
  CORDIC_IRQHandler,
  FMAC_IRQHandler,

};

/*----------------------------------------------------------------------------
  Reset Handler called on controller reset
 *----------------------------------------------------------------------------*/
void Reset_Handler(void)
{

  SystemInit();                             /* CMSIS System Initialization */

#ifdef USE_BOOTLOADER
  SCB->VTOR = (uint32_t)&_vtable;
#endif

  uint32_t *pulSrc, *pulDest;

  pulSrc = &_sidata;
  for(pulDest = &_sdata; pulDest < &_edata; )
  {
    *(pulDest++) = *(pulSrc++);
  }

  for(pulDest = &_sbss; pulDest < &_ebss; ) {
    *(pulDest++) = 0;
  }

  __libc_init_array();

  main();

  while(1) {};
}

/*----------------------------------------------------------------------------
  Default Handler for Exceptions / Interrupts
 *----------------------------------------------------------------------------*/
void Default_Handler(void)
{
  configASSERT(0);
}
