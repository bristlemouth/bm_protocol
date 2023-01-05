
#pragma once

#include "port/boot_assert_port.h"

#ifdef assert
#undef assert
#endif

#ifdef IS_BOOTLOADER
// From https://interrupt.memfault.com/blog/asserts-in-embedded-systems
#define GET_LR() __builtin_return_address(0)
#define GET_PC(_a) __asm volatile ("mov %0, pc" : "=r" (_a))

#define custom_assert()        \
  do {                         \
    void *pc;                  \
    GET_PC(pc);                \
    const void *lr = GET_LR(); \
    port_assert(pc, lr);       \
  } while (0)

#define assert(exp)            \
  do {                         \
    if (!(exp)) {              \
      custom_assert();         \
    }                          \
  } while (0)

void port_assert(const uint32_t *pc, const uint32_t *lr);
#else
#include "FreeRTOS.h"
#define assert(x) configASSERT(x)

#endif
