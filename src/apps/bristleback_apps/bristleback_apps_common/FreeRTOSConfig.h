/* USER CODE BEGIN Header */
/*
 * FreeRTOS Kernel V10.2.1
 * Portion Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * Portion Copyright (C) 2019 StMicroelectronics, Inc.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */
/* USER CODE END Header */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * These parameters and more are described within the 'configuration' section of the
 * FreeRTOS API documentation available on the FreeRTOS.org web site.
 *
 * See http://www.freertos.org/a00110.html
 *----------------------------------------------------------*/

#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
  #include "lpm.h"
#endif

/* Ensure definitions are only used by the compiler, and not by the assembler. */
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
  #include <stdint.h>
  extern uint32_t SystemCoreClock;
#endif
#define configENABLE_FPU                         1
#define configENABLE_MPU                         0
#define configENABLE_TRUSTZONE                   0

#define configUSE_PREEMPTION                     1
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCPU_CLOCK_HZ                       ( SystemCoreClock )
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMAX_PRIORITIES                     ( 32 )
#define configMINIMAL_STACK_SIZE                 ((uint16_t)128)
#define configTOTAL_HEAP_SIZE                    ((size_t)1024*256)
#define configMAX_TASK_NAME_LEN                  ( 16 )
#define configUSE_TRACE_FACILITY                 1
#define configUSE_16_BIT_TICKS                   0
#define configUSE_MUTEXES                        1
#define configQUEUE_REGISTRY_SIZE                8
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0
#define configUSE_TICKLESS_IDLE                  2
#define configUSE_QUEUE_SETS                     1


/* Defaults to size_t for backward compatibility, but can be changed
   if lengths will always be less than the number of bytes in a size_t. */
#define configMESSAGE_BUFFER_LENGTH_TYPE         size_t

// Adding an extra task notification entry for sensor hub pub/sub and other stuff
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    3

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES                    0
#define configMAX_CO_ROUTINE_PRIORITIES          ( 2 )

/* Software timer definitions. */
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                ( 2 )
#define configTIMER_QUEUE_LENGTH                 32
#define configTIMER_TASK_STACK_DEPTH             256

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */
#define INCLUDE_vTaskPrioritySet             1
#define INCLUDE_uxTaskPriorityGet            1
#define INCLUDE_vTaskDelete                  1
#define INCLUDE_vTaskCleanUpResources        0
#define INCLUDE_vTaskSuspend                 1
#define INCLUDE_vTaskDelayUntil              1
#define INCLUDE_vTaskDelay                   1
#define INCLUDE_xTaskGetSchedulerState       1
#define INCLUDE_xTimerPendFunctionCall       1
#define INCLUDE_xQueueGetMutexHolder         1
#define INCLUDE_uxTaskGetStackHighWaterMark  1
#define INCLUDE_eTaskGetState                1

/*
 * The CMSIS-RTOS V2 FreeRTOS wrapper is dependent on the heap implementation used
 * by the application thus the correct define need to be enabled below
 */
#define USE_FreeRTOS_HEAP_4

/* Cortex-M specific definitions. */
#ifdef __NVIC_PRIO_BITS
 /* __BVIC_PRIO_BITS will be specified when CMSIS is being used. */
 #define configPRIO_BITS         __NVIC_PRIO_BITS
#else
 #define configPRIO_BITS         4
#endif

/* The lowest interrupt priority that can be used in a call to a "set priority"
function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY   15

/* The highest interrupt priority that can be used by any interrupt service
routine that makes calls to interrupt safe FreeRTOS API functions.  DO NOT CALL
INTERRUPT SAFE FREERTOS API FUNCTIONS FROM ANY INTERRUPT THAT HAS A HIGHER
PRIORITY THAN THIS! (higher priorities are lower numeric values. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/* Interrupt priorities used by the kernel port layer itself.  These are generic
to all Cortex-M ports, and do not rely on any particular library functions. */
#define configKERNEL_INTERRUPT_PRIORITY 		( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
/* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* Normal assert() semantics without relying on the provision of an assert.h
header file. */

#include "memfault/ports/freertos_trace.h"
#include "memfault/panics/assert.h"
#define configASSERT(x) MEMFAULT_ASSERT(x)


/* Definitions that map the FreeRTOS port interrupt handlers to their CMSIS
standard names. */
#define vPortSVCHandler    SVC_Handler
#define xPortPendSVHandler PendSV_Handler

/* IMPORTANT: This define is commented when used with STM32Cube firmware, when the timebase source is SysTick,
              to prevent overwriting SysTick_Handler defined within STM32Cube HAL */

#define xPortSysTickHandler SysTick_Handler

/* USER CODE BEGIN Defines */

/* CLI output buffer size*/
#define configCOMMAND_INT_MAX_OUTPUT_SIZE 1024

/* USER CODE END Defines */

#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
void PreSleepProcessing(uint32_t *ulExpectedIdleTime);
void PostSleepProcessing(uint32_t *ulExpectedIdleTime);
#endif /* defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__) */

/* The configPRE_SLEEP_PROCESSING() and configPOST_SLEEP_PROCESSING() macros
allow the application writer to add additional code before and after the MCU is
placed into the low power state respectively. */
#if configUSE_TICKLESS_IDLE == 1
#define configPRE_SLEEP_PROCESSING                        PreSleepProcessing
#define configPOST_SLEEP_PROCESSING                       PostSleepProcessing
#endif /* configUSE_TICKLESS_IDLE == 1 */

// #ifdef BUILD_DEBUG
// Use "slow" but more reliable stack overflow check
// Uncomment above line to only do this for debug builds
// Can also use 1 instead of 2 for faster, but less reliable overflow checks
#define configCHECK_FOR_STACK_OVERFLOW 2
// #endif

// Useful for debugging, takes up an additional 4 bytes per task
#define configRECORD_STACK_HIGH_ADDRESS 1

// Code is responsible for checking for malloc failures
#define configUSE_MALLOC_FAILED_HOOK 0

#define pdTICKS_TO_MS( xTicks )    ( ( uint32_t ) ( ( ( uint64_t ) ( xTicks ) * ( uint32_t ) 1000U ) / ( uint32_t ) configTICK_RATE_HZ ) )

#define pdMS_TO_TICKS( xTimeInMs )    ( ( uint32_t ) ( ( ( uint64_t ) ( xTimeInMs ) * ( uint32_t ) configTICK_RATE_HZ ) / ( uint32_t ) 1000U ) )

// Integrate lptimTick.c and low power manager
// For more information, see:
// https://gist.github.com/jefftenney/02b313fe649a14b4c75237f925872d72
//
#if ( configUSE_TICKLESS_IDLE == 2 )

  //      Preprocessor code in lptimTick.c requires that configTICK_RATE_HZ be a preprocessor-friendly numeric
  // literal.  As a result, the application *ignores* the CubeMX configuration of the FreeRTOS tick rate.
  //
  #undef  configTICK_RATE_HZ
  #define configTICK_RATE_HZ  1000UL  // <-- Set FreeRTOS tick rate here, not in CubeMX.

  //      Don't bother installing xPortSysTickHandler() into the vector table or including it in the firmware
  // image at all.  FreeRTOS doesn't use the SysTick timer nor xPortSysTickHandler() when lptimTick.c is
  // providing the OS tick.
  //
  #undef  xPortSysTickHandler

  // Use low power manager to determine if device can enter low power stop modes
  #define configPRE_SLEEP_PROCESSING(x)   lpmPreSleepProcessing()
  #define configPOST_SLEEP_PROCESSING(x)  lpmPostSleepProcessing()

#endif // configUSE_TICKLESS_IDLE == 2

// Custom trace code
#include "trace.h"

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_CONFIG_H */
