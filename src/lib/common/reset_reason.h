// These are utils to allow the application to describe the reason for an intentional NVIC_SystemReset.
// Data is stored in .noinit, which is read on boot to determine reason for reset if SW reset.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// This value is stored in .noinit to confirm the data in the .noinit ResetReason was set intentionally prior to boot.
#define RESET_REASON_MAGIC 0xB8278F7D

typedef enum {
    RESET_REASON_NONE = 0,
    RESET_REASON_SD_FORMAT,
    RESET_REASON_SD_INSERTION,
    RESET_REASON_DEBUG_RESET,
    RESET_REASON_ENTER_CHARGE_MODE,
    RESET_REASON_MEM_FAULT,
    RESET_REASON_BATT_LOW,
    RESET_REASON_BOOTLOADER,
    RESET_REASON_SYSCFG,
    RESET_REASON_REBOOTCTL,
    RESET_REASON_IDLE,
    RESET_REASON_HWCFG,
    RESET_REASON_MCUBOOT,
    RESET_REASON_UPDATED_CFG,
    RESET_REASON_SD_USB,
    RESET_REASON_NOTECARD_FACTORY_RESET,
    RESET_REASON_INVALID,   // make sure this is always the last enum
    // If adding additional reset reasons, make sure they fit inside
    // reset reason syscfg so they can be properly reported!
    // Also don't forget to update the resetReasonLUT with
    // a string in reset_reason.c
} ResetReason_t;

void resetSystemFromISR(ResetReason_t reason);
void resetSystem(ResetReason_t reason);
ResetReason_t checkResetReason();
const char * getResetReasonString();

#ifdef __cplusplus
}
#endif
