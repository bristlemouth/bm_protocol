/**
 * @file bm_hal.h
 * @brief Bristlemouth Hardware Abstraction Layer.
 *
 * This header provides platform-specific hooks for low-level hardware functions
 * that may be required by the system. Currently, only basic and limited interface
 * is provided for a watchdog. But, in time, this may be expanded to include GPIO,
 * UART, and other hardware interfaces as needed.
 *
 */

#ifndef BM_HAL_H
#define BM_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

// ====================================================
// @name Watchdog API
// ====================================================

/** @brief Opaque handle for a hardware watchdog instance. */
typedef struct bm_hal_wd* bm_hal_wd_t;

/** @brief Returns a handle to the platform’s watchdog instance. */
bm_hal_wd_t bm_hal_wd_get_handle(void);

/** @brief Reloads the hardware watchdog timer to prevent system reset. */
void bm_hal_wd_reload_counter(bm_hal_wd_t wd);

/** @brief Logs watchdog-related debug info (platform-specific). */
void bm_hal_wd_log(void);

#ifdef __cplusplus
}
#endif

#endif // BM_HAL_H
