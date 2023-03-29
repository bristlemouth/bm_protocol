#include <stdint.h>
#include "stm32u5xx.h"
#ifndef IS_BOOTLOADER
#include "reset_reason.h"
#endif

#define ENTER_BOOTLOADER_MAGIC 0xB8278F6D
#define BOOTLOADER_START_ADDRESS 0x0BF90000

typedef void (*functionPointer_t)(void);

// Make sure the crash info is not re-initialized on reboot
uint32_t ulBootloaderMagic __attribute__ ((section (".noinit")));

void rebootIntoROMBootloader() {
  // Set magic value so we'll know to enter bootloader after reset
  ulBootloaderMagic = ENTER_BOOTLOADER_MAGIC;

  // Reset the device so that all peripherals are in the default state
  // Call enterBootloaderIfNeeded() as soon as possible after reboot
#ifndef IS_BOOTLOADER
    resetSystem(RESET_REASON_BOOTLOADER);
#else
    NVIC_SystemReset();
#endif
}

void enterBootloaderIfNeeded() {
  uint32_t cachedBootloaderMagic = ulBootloaderMagic;

  // Make sure we clear this every time so we don't get stuck in a bootloop
  ulBootloaderMagic = 0;

  if(cachedBootloaderMagic == ENTER_BOOTLOADER_MAGIC) {
    functionPointer_t bootloader = (functionPointer_t)(*(volatile uint32_t *)(BOOTLOADER_START_ADDRESS + 4));

    // Set new stack pointer
    __set_MSP(*(volatile uint32_t *)BOOTLOADER_START_ADDRESS);

    // Enter bootloader
    bootloader();
  }
}
