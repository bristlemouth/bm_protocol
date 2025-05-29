// Bristlelmouth Includes
#include "bm_hal.h"

// FreeRTOS Includes
#include "FreeRTOS.h"

// STM32 Includes
#include "iwdg.h"

void bm_hal_wd_reload_counter() {
    LL_IWDG_ReloadCounter(IWDG);
}

// void* bm_hal_wd_get_handle(void) {
//     return IWDG;
// }

void IWDG_IRQHandler(void) {
  LL_IWDG_EnableWriteAccess(IWDG);
  while (LL_IWDG_IsReady(IWDG) != 1)
  {
  }
  LL_IWDG_SetPrescaler(IWDG, LL_IWDG_PRESCALER_1024);
  LL_IWDG_ReloadCounter(IWDG);
  MEMFAULT_ASSERT_EXTRA_AND_REASON(0, kMfltRebootReason_HardwareWatchdog);
}

void bm_hal_wd_log() {
//   memfault_software_watchdog_feed();
}
