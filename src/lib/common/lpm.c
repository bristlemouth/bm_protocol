// Low Power Manager
//
// Derived from Jeff Tenney's great lptimtick and ulp.c/h examples
// see: https://github.com/jefftenney/LPTIM-Tick
//

#include <stdbool.h>
#include "stm32l4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lpm.h"

/*!
  Mark non-low power mode peripheral as active to inhibit deep sleep.

  \param[in] peripheralMask - bit mask of peripheral(s) that are active
  \return none
*/
void lpmInit() {
  //      Be sure the MCU wakes up from stop mode on the same clock we normally use as the core clock, if
  // possible.  Might as well give the MCU a head start getting the clock going while waking from STOP.
  //
  if ( (RCC->CFGR & RCC_CFGR_SWS_Msk) == RCC_CFGR_SWS_HSI ) {
    SET_BIT(RCC->CFGR, RCC_CFGR_STOPWUCK);
  } else if ( (RCC->CFGR & RCC_CFGR_SWS_Msk) == RCC_CFGR_SWS_MSI ) {
    CLEAR_BIT(RCC->CFGR, RCC_CFGR_STOPWUCK);
  }
}

static volatile uint32_t deepSleepForbiddenFlags = 0;

/*!
  Mark non-low power mode peripheral as active to inhibit deep sleep.

  \param[in] peripheralMask - bit mask of peripheral(s) that are active
  \return none
*/
void lpmPeripheralActive(uint32_t peripheralMask) {
  taskENTER_CRITICAL();
  deepSleepForbiddenFlags |= peripheralMask;
  taskEXIT_CRITICAL();
}

/*!
  (ISR safe) Mark non-low power mode peripheral as active to inhibit deep sleep.

  \param[in] peripheralMask - bit mask of peripheral(s) that are active
  \return none
*/
void lpmPeripheralActiveFromISR(uint32_t peripheralMask) {
  UBaseType_t uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
  deepSleepForbiddenFlags |= peripheralMask;
  taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
}

/*!
  Mark non-low power mode peripheral as inactive to allow deep sleep.

  \param[in] peripheralMask - bit mask of peripheral(s) that are no longer active
  \return none
*/
void lpmPeripheralInactive(uint32_t peripheralMask) {
  taskENTER_CRITICAL();
  deepSleepForbiddenFlags &= ~peripheralMask;
  taskEXIT_CRITICAL();
}


/*!
  (ISR safe) Mark non-low power mode peripheral as inactive to allow deep sleep.

  \param[in] peripheralMask - bit mask of peripheral(s) that are no longer active
  \return none
*/
void lpmPeripheralInactiveFromISR(uint32_t peripheralMask) {
  UBaseType_t uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
  deepSleepForbiddenFlags &= ~peripheralMask;
  taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
}

//      Functions vUlpPreSleepProcessing() and vUlpPostSleepProcessing() are called from a critical section,
// so we happily take a few shortcuts made safe by that usage model.
//
static uint32_t rccCfgrSave;
static uint32_t rccCrSave;

void lpmPreSleepProcessing() {
  bool useDeepSleep = false;
  if (deepSleepForbiddenFlags == 0) {
    useDeepSleep = true;
    MODIFY_REG(PWR->CR1, PWR_CR1_LPMS_Msk, PWR_CR1_LPMS_STOP2);
  } else if ((deepSleepForbiddenFlags & ~LPM_OK_IN_STOP1) == 0) {
    useDeepSleep = true;
    MODIFY_REG(PWR->CR1, PWR_CR1_LPMS_Msk, PWR_CR1_LPMS_STOP1);
  }

  if (useDeepSleep) {
    rccCrSave = RCC->CR;
    rccCfgrSave = RCC->CFGR;

    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  }
}

void lpmPostSleepProcessing() {
  if (SCB->SCR & SCB_SCR_SLEEPDEEP_Msk) {
    //      We may have been in deep sleep.  If we were, the hardware cleared several enable bits in the CR,
    // and it changed the selected system clock in CFGR.  Restore them now.  If we're restarting the PLL as
    // the CPU clock here, the CPU will not wait for it.  Instead, the CPU continues executing from the
    // wake-up clock (MSI in our case) until the PLL is stable and then the CPU starts using the PLL.
    //
    RCC->CR = rccCrSave;
    RCC->CFGR = rccCfgrSave;

    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

    //      This application bypasses the RTC shadow registers, so we don't need to clear the sync flag for
    // those registers.  They are always out of sync when coming out of deep sleep.
    //
    // RTC->ISR &= ~RTC_ISR_RSF;
  }
}
