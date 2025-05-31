/**
 * @file bm_hal_stm32h5xxx.c
 * @brief STM32H5xx-specific implementation of Bristlemouth HAL functions.
 *
 * Provides hardware-specific implementations of watchdog-related hooks used
 * across the system, including the IWDG reload function and fault logging.
 */

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------

// Bristlemouth Includes
#include "bm_hal.h"

// FreeRTOS Includes
#include "FreeRTOS.h"

// STM32 Includes
#include "iwdg.h"

// ====================================================
// Watchdog API Implementations
// ====================================================

/**
 * @brief Opaque watchdog handle used by bm_hal API.
 *
 * This struct wraps the platform-specific IWDG_TypeDef* and enables type-safe
 * abstraction of the watchdog instance. It hides implementation details from
 * users and allows for future extensibility without breaking the API.
 */
struct bm_hal_wd {
    IWDG_TypeDef* iwdg;
};

static struct bm_hal_wd stm32_hw_wd = {
    .iwdg = IWDG
};

bm_hal_wd_t bm_hal_wd_get_handle(void) {
    return &stm32_hw_wd;
}

void bm_hal_wd_reload_counter(bm_hal_wd_t wd) {
    if (wd && wd->iwdg) {
        LL_IWDG_ReloadCounter(wd->iwdg);
    }
}

// Optional log stub for watchdog diagnostics (e.g., Memfault)
void bm_hal_wd_log(void) {
    // memfault_software_watchdog_feed();
}

// -----------------------------------------------------------------------------
// Watchdog IWDG IRQ Handler
// -----------------------------------------------------------------------------

// NOTE: This IRQ handler reconfigures and reloads the IWDG on interrupt.
// It assumes use of the STM32 LL (Low-Layer) APIs.
void IWDG_IRQHandler(void) {
    LL_IWDG_EnableWriteAccess(IWDG);
    while (LL_IWDG_IsReady(IWDG) != 1) {
        // Wait until the IWDG is ready to accept configuration
    }
    LL_IWDG_SetPrescaler(IWDG, LL_IWDG_PRESCALER_1024);
    LL_IWDG_ReloadCounter(IWDG);

    MEMFAULT_ASSERT_EXTRA_AND_REASON(0, kMfltRebootReason_HardwareWatchdog);
}
