
// Includes from CubeMX Generated files
#include "main.h"

// Peripheral
#include "gpio.h"
#include "iwdg.h"

// MCUBoot
#include "bootutil/bootutil.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/image.h"
#include "boot_serial/boot_serial.h"
#include "mcuboot_config/mcuboot_logging.h"

#include "port/boot_log_port.h"
#include "port/boot_serial_port.h"
#include "port/boot_startup_port.h"
#include "port/boot_heap_port.h"

#include "bootloader_helper.h"

void boot_port_init( void ) {
  HAL_Init();
  SystemClock_Config();
  SystemPower_Config_ext();

  MX_GPIO_Init();

  // Enable the watchdog timer
  MX_IWDG_Init();

  // Enable hardfault on divide-by-zero
  SCB->CCR |= 0x10;

  // Enable null pointer dereference protection
  // memfault_enable_mpu();

  LL_GPIO_SetOutputPin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
  LL_GPIO_SetOutputPin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
  LL_GPIO_SetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);

  LL_GPIO_ResetOutputPin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
  LL_GPIO_ResetOutputPin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
}

static void start_app(uint32_t pc, uint32_t sp) {
  __asm volatile ("MSR msp, %0" : : "r" (sp) : );
  void (*application_reset_handler)(void) = (void *)pc;
  application_reset_handler();

  // Let the compiler know this won't ever be reached
  // Was causing issues with -Os
  __builtin_unreachable();
}

void boot_port_startup( struct boot_rsp *rsp ) {
  BOOT_LOG_INF("Starting Main Application");
  BOOT_LOG_INF("  Image Start Offset: 0x%x", (int)rsp->br_image_off);

  // We run from internal flash. Base address of this medium is 0x0
  uint32_t vector_table = 0x0 + rsp->br_image_off + rsp->br_hdr->ih_hdr_size;

  uint32_t *app_code = (uint32_t *)vector_table;
  uint32_t app_sp = app_code[0];
  uint32_t app_start = app_code[1];

  BOOT_LOG_INF("  Vector Table Start Address: 0x%x. PC=0x%x, SP=0x%x",
    (int)vector_table, app_start, app_sp);

  // Disable ticks/interrupts to prepare for new app
  HAL_SuspendTick();
  HAL_NVIC_DisableIRQ(TIM8_UP_IRQn);
  HAL_DeInit();

  SCB->VTOR = vector_table;

  start_app(app_start, app_sp);
}

int main(void) {

  // Before doing anything, check if we should enter ROM bootloader
  enterBootloaderIfNeeded();

  struct boot_rsp rsp;
  fih_int fih_rc = FIH_FAILURE;

  BOOT_LOG_INF("Starting bootloader...");

  /* Init required bootloader hardware, such as watchdog */
  boot_port_init();

  BOOT_LOG_INF("Bootloader port initialized.");
  BOOT_LOG_INF("Starting boot...");

  FIH_CALL(boot_go, fih_rc, &rsp);
  if (fih_not_eq(fih_rc, FIH_SUCCESS))
  {
    BOOT_LOG_ERR("Unable to find bootable image.");
    FIH_PANIC;
  }

  boot_port_startup( &rsp );

  // We should never make it here!
  assert(0);
}
