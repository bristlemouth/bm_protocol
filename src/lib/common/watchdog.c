#include <stddef.h>

#include "bm_osal.h"
#include "bm_hal.h"
#include "task_priorities.h"

// If you want to use the memfault watchdog, uncomment the following include,
// enable the LPTIM2 clock, and uncomment the memfault_software_watchdog_enable()
// call in startIWDGTask() and the memfault_software_watchdog_feed() call in watchdogFeed()
// #include "memfault/ports/watchdog.h"

static void iWDGTask( void *parameters );

// Opaque pointer to the hardware watchdog instance.
static bm_hal_wd_t wd = NULL;

void startIWDGTask() {
  // memfault_software_watchdog_enable();

  bm_osal_base_t rval;

  // Set the hardware watchdog handle
  wd = bm_hal_wd_get_handle();
  if (wd == NULL) {
    // Handle error: watchdog not available
    return;
  }

  // Create a task for the watchdog
  rval = bm_osal_task_create(
              iWDGTask,
              "IWDG",
              bm_osal_task_get_min_stack_size(),
              NULL,
              IWDG_TASK_PRIORITY,
              NULL);

  bm_osal_assert(rval == BM_OSAL_TRUE);
  if (rval == 1) {
    return;
  }
  // bm_osal_assert()
}

void watchdogFeed() {
  bm_hal_wd_reload_counter(wd);
  bm_hal_wd_log();
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
    bm_osal_task_delay(2 * 1000);
  }
}
