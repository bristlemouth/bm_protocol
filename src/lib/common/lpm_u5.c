// Low Power Manager
//
// Derived from Jeff Tenney's great lptimtick and ulp.c/h examples
// see: https://github.com/jefftenney/LPTIM-Tick
//

#include <stdbool.h>
// #include "stm32l4xx.h"
#include "stm32u5xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lpm.h"

#define SUPPORT_VREG_RANGES_1_THROUGH_3

/*!
  Mark non-low power mode peripheral as active to inhibit deep sleep.

  \param[in] peripheralMask - bit mask of peripheral(s) that are active
  \return none
*/
void lpmInit() {
  //      Turn on the peripheral clock to the PWR module.  We use that module to control low-power options.
  //
  __HAL_RCC_PWR_CLK_ENABLE();

  //      Use the brownout reset supervisor in its lowest-power mode.
  //
  PWR->CR1 |= PWR_CR1_ULPMEN;

  //      Select the DC-DC SMPS instead of the LDO.  The SMPS gives us the lowest power consumption.
  //
  PWR->CR3 |= PWR_CR3_REGSEL;

  //      Be sure the MCU wakes up from stop mode on the same clock we normally use as the core clock, if
  // possible.  Might as well give the MCU a head start getting the clock going while waking from STOP.
  //
  #define RCC_CFGR1_SW_HSI16  RCC_CFGR1_SW_0
  #define RCC_CFGR1_SW_MSIS   0
  if ( (RCC->CFGR1 & RCC_CFGR1_SW_Msk) == RCC_CFGR1_SW_HSI16 )
  {
    SET_BIT(RCC->CFGR1, RCC_CFGR1_STOPWUCK);
  }
  else if ( (RCC->CFGR1 & RCC_CFGR1_SW_Msk) == RCC_CFGR1_SW_MSIS )
  {
    CLEAR_BIT(RCC->CFGR1, RCC_CFGR1_STOPWUCK);
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
#ifdef SUPPORT_MSI_AT_48MHZ
static uint32_t rccIcscr1Save;
#endif
#ifdef SUPPORT_VREG_RANGES_1_THROUGH_3
static uint32_t pwrVosrSave;
#endif

void lpmPreSleepProcessing() {
  int useDeepSleep = pdFALSE;
   if (deepSleepForbiddenFlags == 0)
   {
      useDeepSleep = pdTRUE;
      #define PWR_CR1_LPMS_STOP2 PWR_CR1_LPMS_1
      MODIFY_REG(PWR->CR1, PWR_CR1_LPMS_Msk, PWR_CR1_LPMS_STOP2);
   }
   else if ((deepSleepForbiddenFlags & ~LPM_OK_IN_STOP1) == 0)
   {
      useDeepSleep = pdTRUE;
      #define PWR_CR1_LPMS_STOP1 PWR_CR1_LPMS_0
      MODIFY_REG(PWR->CR1, PWR_CR1_LPMS_Msk, PWR_CR1_LPMS_STOP1);
   }

   if (useDeepSleep)
   {
      rccCrSave = RCC->CR;
      rccCfgrSave = RCC->CFGR1;
      #ifdef SUPPORT_MSI_AT_48MHZ
      {
         rccIcscr1Save = RCC->ICSCR1;
      }
      #endif
      #ifdef SUPPORT_VREG_RANGES_1_THROUGH_3
      {
         pwrVosrSave = PWR->VOSR;
      }
      #endif

      SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
   }
}

void lpmPostSleepProcessing() {
  if (SCB->SCR & SCB_SCR_SLEEPDEEP_Msk)
   {
      //      We may have been in deep sleep.  If we were, the RCC cleared several enable bits in the CR, and
      // it changed the selected system clock in CFGR.  Restore them now.  If we're restarting the PLL as
      // the CPU clock here, the CPU will not wait for it.  Instead, the CPU continues executing from the
      // wake-up clock (MSI in our case) until the PLL is stable and then the CPU starts using the PLL.

      #ifdef SUPPORT_VREG_RANGES_1_THROUGH_3
      {
         //      In the STM32U5, we must first restore the previous regulator range because it may be required
         // to support the previous clock speeds.  In deep sleep, the PWR module sets range 4 automatically.
         // Wait for the regulator (and booster if necessary) to be ready before restoring the clocks.
         //
         do
         {
            PWR->VOSR = pwrVosrSave;
         } while (PWR->VOSR != pwrVosrSave);
      }
      #endif

      #ifdef SUPPORT_MSI_AT_48MHZ
      {
         RCC->ICSCR1 = rccIcscr1Save;
      }
      #endif

      RCC->CR = rccCrSave;
      RCC->CFGR1 = rccCfgrSave;

      SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

      //      This application bypasses the RTC shadow registers, so we don't need to clear the sync flag for
      // those registers.  They are always out of sync when coming out of deep sleep.
      //
      // RTC->ISR &= ~RTC_ISR_RSF;
   }
}
