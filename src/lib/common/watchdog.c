
/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "iwdg.h"
#include "task_priorities.h"

// If you want to use the memfault watchdog, uncomment the following include,
// enable the LPTIM2 clock, and uncomment the memfault_software_watchdog_enable()
// call in startIWDGTask() and the memfault_software_watchdog_feed() call in watchdogFeed()
// #include "memfault/ports/watchdog.h"

void watchdogFeed() {
  LL_IWDG_ReloadCounter(IWDG);
  // memfault_software_watchdog_feed();
}

/***************************************************************************************
 *
 * Potential Watchdog issue relating to STM32u575 Errata 2.2.26
 *
 * We have observed that the Aanderaa Current Meter RS232 application has been
 * unexpectedly resetting during some of our testing. Through testing and git bisecting
 * we were able to determine that the commit that changed the watchdog from PR#316
 * was the commit that introduced the issue. Interestingly with the watchdog changes
 * this vApplicationIdleHook gets called before we enter Stop mode. Errata 2.2.26 states
 * that there are certain conditions that if met upon entering sleep would result in
 * the processor being stuck in stop mode with a system reset being the only recovery
 * method. So this errata, along with other theories are being investigated to root
 * cause the unexpected resets.
 *
 * While we are doing that we are going to go back to using the old watchdog for the
 * Current Meter Aanderaa RS232 application.
 *
***************************************************************************************/
void vApplicationIdleHook(void) {
    watchdogFeed();
}

//
// This is a VERY basic watchdog. It runs as the lowest priority task
// If any other task doesn't yield within 5 seconds, this task will not
// be serviced and a watchdog reset will occur.
// This means no task can run (uninterrupted) for more than 5 seconds.
//
// General Operation:
// The MEMFAULT LPTIM watchdog will kick in and handle coredumps / reset reason etc.
// Before writing the coredump (which takes a while), we pet the IWDG one more time.
// We then reset via the memfault handler.
//
// TODO - implement a better design that perhaps lets other tasks check-in
//        periodically and the watchdog is only refreshed when several
//        conditions are met.
//
#ifdef APP_AANDERAA_CURRENT_METER
static void iWDGTask( void *parameters );

void startIWDGTask() {
  // memfault_software_watchdog_enable();
  BaseType_t rval;
  rval = xTaskCreate(
              iWDGTask,
              "IWDG",
              configMINIMAL_STACK_SIZE,
              NULL,
              IWDG_TASK_PRIORITY,
              NULL);

  configASSERT(rval == pdTRUE);
}

static void iWDGTask( void *parameters ) {
  // Don't warn about unused parameters
  (void) parameters;

  for(;;) {
    watchdogFeed();
    vTaskDelay(2 * 1000);
  }
}
#endif // APP_AANDERAA_CURRENT_METER

void IWDG_IRQHandler(void) {
  LL_IWDG_EnableWriteAccess(IWDG);
  while (LL_IWDG_IsReady(IWDG) != 1)
  {
  }
  LL_IWDG_SetPrescaler(IWDG, LL_IWDG_PRESCALER_1024);
  LL_IWDG_ReloadCounter(IWDG);
  MEMFAULT_ASSERT_EXTRA_AND_REASON(0, kMfltRebootReason_HardwareWatchdog);
}
