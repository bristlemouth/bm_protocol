
/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "iwdg.h"
#include "task_priorities.h"
#include "memfault/ports/watchdog.h"

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

void watchdogFeed() {
  LL_IWDG_ReloadCounter(IWDG);
  // memfault_software_watchdog_feed();
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
static void iWDGTask( void *parameters ) {
  // Don't warn about unused parameters
  (void) parameters;

  for(;;) {
    watchdogFeed();
    vTaskDelay(2 * 1000);
  }
}

void IWDG_IRQHandler(void) {
  LL_IWDG_EnableWriteAccess(IWDG);
  while (LL_IWDG_IsReady(IWDG) != 1)
  {
  }
  LL_IWDG_SetPrescaler(IWDG, LL_IWDG_PRESCALER_1024);
  LL_IWDG_ReloadCounter(IWDG);
  MEMFAULT_ASSERT_EXTRA_AND_REASON(0, kMfltRebootReason_HardwareWatchdog);
}
