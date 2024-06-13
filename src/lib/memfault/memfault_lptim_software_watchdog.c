// Based off of https://github.com/memfault/memfault-firmware-sdk/blob/0.33.2/ports/stm32cube/h7/lptim_software_watchdog.c

#include "memfault/ports/watchdog.h"

#include <stdbool.h>

#include "stm32u5xx_hal.h"

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/core/math.h"

#ifndef MEMFAULT_SOFWARE_WATCHDOG_SOURCE
# define MEMFAULT_SOFWARE_WATCHDOG_SOURCE LPTIM2_BASE
#endif

#ifndef MEMFAULT_LPTIM_ENABLE_FREEZE_DBGMCU
// By default, we automatically halt the software watchdog timer when
// the core is halted in debug mode. This prevents a watchdog from firing
// immediately upon resumption.
# define MEMFAULT_LPTIM_ENABLE_FREEZE_DBGMCU 1
#endif

#ifndef MEMFAULT_EXC_HANDLER_WATCHDOG
# if MEMFAULT_SOFWARE_WATCHDOG_SOURCE == LPTIM2_BASE
#  error "Port expects following define to be set: -DMEMFAULT_EXC_HANDLER_WATCHDOG=LPTIM2_IRQHandler"
# else
#  error "Port expects following define to be set: -DMEMFAULT_EXC_HANDLER_WATCHDOG=LPTIM<N>_IRQHandler"
# endif
#endif

#if ((MEMFAULT_SOFWARE_WATCHDOG_SOURCE != LPTIM1_BASE) && \
     (MEMFAULT_SOFWARE_WATCHDOG_SOURCE != LPTIM2_BASE))

# error "MEMFAULT_SOFWARE_WATCHDOG_SOURCE must be one of LPTIM[1-2]_BASE"
#endif

//! We wire the LPTIM -> LSI so the clock frequency will be 32kHz
#define MEMFAULT_LPTIM_CLOCK_FREQ_HZ 32000U
#define MEMFAULT_LPTIM_PRESCALER 128
#define MEMFAULT_LPTIM_MAX_COUNT 0xFFFF

#define MEMFAULT_LPTIM_HZ (MEMFAULT_LPTIM_CLOCK_FREQ_HZ / MEMFAULT_LPTIM_PRESCALER)

#define MEMFAULT_MS_PER_SEC 1000
#define MEMFAULT_LPTIM_MAX_TIMEOUT_SEC ((MEMFAULT_LPTIM_MAX_COUNT + 1) / MEMFAULT_LPTIM_HZ)

#if MEMFAULT_WATCHDOG_SW_TIMEOUT_SECS > MEMFAULT_LPTIM_MAX_TIMEOUT_SEC
#error "MEMFAULT_WATCHDOG_SW_TIMEOUT_SECS exceeds maximum value supported by hardware"
#endif

static LPTIM_HandleTypeDef s_lptim_cfg;
static uint32_t s_timeout_ms = (MEMFAULT_WATCHDOG_SW_TIMEOUT_SECS * MEMFAULT_MS_PER_SEC);

static void prv_lptim_clock_config(RCC_PeriphCLKInitTypeDef *config) {
  switch ((uint32_t)MEMFAULT_SOFWARE_WATCHDOG_SOURCE) {
    case LPTIM1_BASE:
      *config = (RCC_PeriphCLKInitTypeDef )  {
        .PeriphClockSelection = RCC_PERIPHCLK_LPTIM1,
        .Lptim1ClockSelection = RCC_LPTIM1CLKSOURCE_LSI
      };
      break;

    case LPTIM2_BASE:
      *config = (RCC_PeriphCLKInitTypeDef )  {
        .PeriphClockSelection = RCC_PERIPHCLK_LPTIM2,
        .Lptim2ClockSelection = RCC_LPTIM2CLKSOURCE_LSI
      };
      break;

    default:
      *config =  (RCC_PeriphCLKInitTypeDef) { 0 };
      break;
  }
}

static void prv_lptim_clk_enable(void) {
  switch (MEMFAULT_SOFWARE_WATCHDOG_SOURCE) {
    case LPTIM1_BASE:
      __HAL_RCC_LPTIM1_CLK_ENABLE();
      __HAL_RCC_LPTIM1_FORCE_RESET();
      __HAL_RCC_LPTIM1_RELEASE_RESET();
      break;

    case LPTIM2_BASE:
      __HAL_RCC_LPTIM2_CLK_ENABLE();
      __HAL_RCC_LPTIM2_FORCE_RESET();
      __HAL_RCC_LPTIM2_RELEASE_RESET();
      break;

    default:
      break;
  }
}

static void prv_lptim_clk_freeze_during_dbg(void) {
#if MEMFAULT_LPTIM_ENABLE_FREEZE_DBGMCU
  switch (MEMFAULT_SOFWARE_WATCHDOG_SOURCE) {
    case LPTIM1_BASE:
      __HAL_DBGMCU_FREEZE_LPTIM1();
      break;

    case LPTIM2_BASE:
      __HAL_DBGMCU_FREEZE_LPTIM2();
      break;

    default:
      break;
  }
#endif
}

static void prv_lptim_irq_enable(void) {
  switch (MEMFAULT_SOFWARE_WATCHDOG_SOURCE) {
    case LPTIM1_BASE:
       NVIC_ClearPendingIRQ(LPTIM1_IRQn);
       NVIC_EnableIRQ(LPTIM1_IRQn);
      break;

    case LPTIM2_BASE:
       NVIC_ClearPendingIRQ(LPTIM2_IRQn);
       NVIC_EnableIRQ(LPTIM2_IRQn);
      break;

    default:
      break;
  }
}

int memfault_software_watchdog_enable(void) {
  // We will drive the Low Power Timer (LPTIM) from the Low-speed internal (LSI) oscillator
  // (~32kHz). This source will run while in low power modes (just like the IWDG hardware watchdog)
  const bool lsi_on = (RCC->BDCR & RCC_BDCR_LSION) == RCC_BDCR_LSION;
  if (!lsi_on) {
    __HAL_RCC_LSI_ENABLE();

    const uint32_t tickstart = HAL_GetTick();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == 0) {
      if ((HAL_GetTick() - tickstart) > 2U) {
        return HAL_TIMEOUT;
      }
    }
  }

  // The LPTIM can be driven from multiple clock sources so we need to explicitly connect
  // it to the LSI clock that we just enabled.
  RCC_PeriphCLKInitTypeDef lptim_clock_config;
  prv_lptim_clock_config(&lptim_clock_config);
  HAL_StatusTypeDef rv = HAL_RCCEx_PeriphCLKConfig(&lptim_clock_config);
  if (rv != HAL_OK) {
    return rv;
  }

  // Enable the LPTIM clock and reset the peripheral
  prv_lptim_clk_enable();
  prv_lptim_clk_freeze_during_dbg();

  s_lptim_cfg = (LPTIM_HandleTypeDef) {
    .Instance = (LPTIM_TypeDef *)MEMFAULT_SOFWARE_WATCHDOG_SOURCE,
    .Init = {
      .Clock = {
        .Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC,
#if MEMFAULT_LPTIM_PRESCALER == 128
        .Prescaler = LPTIM_PRESCALER_DIV128,
#else
#error "Unexpected Prescaler value"
#endif
      },
      .Trigger = {
        .Source = LPTIM_TRIGSOURCE_SOFTWARE,
      },
      .Period = MEMFAULT_MIN((s_timeout_ms * MEMFAULT_LPTIM_HZ) / MEMFAULT_MS_PER_SEC,
                                      MEMFAULT_LPTIM_MAX_COUNT),
      .UpdateMode = LPTIM_UPDATE_IMMEDIATE,
      .CounterSource = LPTIM_COUNTERSOURCE_INTERNAL,
      .RepetitionCounter = 0,
      // NB: not used in config but HAL expects valid values here
      .Input1Source = LPTIM_INPUT1SOURCE_GPIO,
      .Input2Source = LPTIM_INPUT2SOURCE_GPIO,
    },
  };
  rv = HAL_LPTIM_Init(&s_lptim_cfg);
  if (rv != HAL_OK) {
    return rv;
  }

  prv_lptim_irq_enable();

  memfault_software_watchdog_update_timeout(s_timeout_ms);
  return 0;
}

/*
 * https://community.st.com/s/question/0D50X0000BNL3UvSQL/stm32-major-lptimer-documentation-shortcoming-or-is-it-a-silicon-design-error
 * Apparently the LPTIM CR register on the STM32L462xx doesn't have a documented COUNTRST feature.
 * Thus, we refresh the count of our LPTIM peripheral by stopping / starting.
 */
int memfault_software_watchdog_feed(void) {
  if ((s_lptim_cfg.Instance->CR & LPTIM_CR_COUNTRST) != 0) {
    // A COUNTRST is already in progress, no work to do
    return 0;
  }
  __HAL_LPTIM_RESET_COUNTER(&s_lptim_cfg);

  return 0;
}

int memfault_software_watchdog_update_timeout(uint32_t timeout_ms) {
  if (timeout_ms > (MEMFAULT_LPTIM_MAX_TIMEOUT_SEC * MEMFAULT_MS_PER_SEC)) {
    return -1;
  }

  HAL_StatusTypeDef rv = HAL_LPTIM_Counter_Stop_IT(&s_lptim_cfg);
  if (rv != HAL_OK) {
    return rv;
  }

  rv = HAL_LPTIM_Counter_Start_IT(&s_lptim_cfg);
  if (rv != HAL_OK) {
    return rv;
  }

  return 0;
}

int memfault_software_watchdog_disable(void) {
  // Clear and disable interrupts
  __HAL_LPTIM_DISABLE_IT(&s_lptim_cfg, LPTIM_IT_ARRM);

  // Stop the counter
  return HAL_LPTIM_Counter_Stop(&s_lptim_cfg);
}
