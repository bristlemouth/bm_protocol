#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

#include "port/boot_assert_port.h"
#include "port/boot_wdt_port.h"
#include "port/boot_heap_port.h"

#include "mcuboot_config/mcuboot_logging.h"

#include "bsp.h"
#include "printf.h"

#ifdef IS_BOOTLOADER
#include "iwdg.h"
#else
#include "watchdog.h"
#endif

#ifdef IS_BOOTLOADER
void port_assert(const uint32_t *pc, const uint32_t *lr) {
#ifndef MCUBOOT_HAVE_LOGGING
  (void)pc;
  (void)lr;
#endif
  MCUBOOT_LOG_ERR("ASSERT: PC:0x%08lX LR:0x%08lX\n\n", (uint32_t)pc, (uint32_t)lr);

  // Halt if debugging
  if(CoreDebug->DHCSR & 1) {
    __asm("bkpt 42");
  }

  NVIC_SystemReset();
}

void swoPutchar(char character, void* arg) {
  (void)arg;
  ITM_SendChar(character);
}

int vLog(const char* format, ...) {
  va_list va;
  va_start(va, format);
  int rval = vfctprintf(swoPutchar, NULL, format, va);
  va_end(va);

  return rval;
}

void boot_port_wdt_feed( void ) {
  LL_IWDG_ReloadCounter(IWDG);
  return;
}

#else
void boot_port_wdt_feed( void ) {
  watchdogFeed();
  return;
}
#endif

//
// The following functions are not implemented/used for our use case
//
// void boot_port_wdt_disable( void );}
// void *boot_port_malloc( size_t size );
// void  boot_port_free( void *mem );
// void *boot_port_realloc( void *ptr, size_t size );
// void boot_port_heap_init( void );
// void boot_port_log_init( void );
