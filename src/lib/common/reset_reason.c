#include <stdint.h>
#include "debug.h"
#include "enumToStr.h"
// #include "log.h"
#include "reset_reason.h"
// #include "sd.h"
#include "bsp.h"

// Make sure the crash info is not re-initialized on reboot
uint32_t ulResetReasonMagic __attribute__ ((section (".noinit")));
ResetReason_t resetReason __attribute__((section(".noinit")));

// ISR safe reset reason (no log flush)
void resetSystemFromISR(ResetReason_t reason) {
    ulResetReasonMagic = RESET_REASON_MAGIC;
    resetReason = reason;

    // Reset the device so that all peripherals are in the default state
    NVIC_SystemReset();
}

void resetSystem(ResetReason_t reason) {
    // Try to flush SD logs if SD card is connected
    // if(sdIsMounted()) {
        // printf("Flushing log buffers.\n");
        // logFlushAll(1000);
    // }

    resetSystemFromISR(reason);
}

ResetReason_t checkResetReason() {
    uint32_t cachedResetReasonMagic = ulResetReasonMagic;
    static ResetReason_t cachedResetReason = RESET_REASON_NONE;

    // Make sure we clear this every time
    ulResetReasonMagic = 0;

    // if cached value is NONE(i.e. checkResetReason is being called for the first time since restart),
    // check if the magic number has been set
    if(cachedResetReason == RESET_REASON_NONE){
        // if the magic number has been set, update the cachedResetReason to the last reset reason
        // else set cachedResetReason to INVALID
        if(cachedResetReasonMagic == RESET_REASON_MAGIC){
            cachedResetReason = resetReason;
        }
        else{
            cachedResetReason = RESET_REASON_INVALID;
        }
    }
    // Clear the reset reason
    resetReason = RESET_REASON_INVALID;

    return cachedResetReason;
}

static const enumStrLUT_t resetReasonLUT[] = {
    {RESET_REASON_NONE, "No reset"},
    {RESET_REASON_SD_FORMAT, "SD format reset"},
    {RESET_REASON_SD_INSERTION, "SD card insertion reset"},
    {RESET_REASON_DEBUG_RESET, "Debug reset"},
    {RESET_REASON_ENTER_CHARGE_MODE, "Enter charge mode reset"},
    {RESET_REASON_MEM_FAULT, "mem_fault_reboot reset"},
    {RESET_REASON_BATT_LOW, "Batter level critical reset"},
    {RESET_REASON_BOOTLOADER, "Bootloader reset"},
    {RESET_REASON_SYSCFG, "sysCfg reset"},
    {RESET_REASON_REBOOTCTL, "Reboot Controller"},
    {RESET_REASON_IDLE, "Idle mode"},
    {RESET_REASON_HWCFG, "hwCfg reset"},
    {RESET_REASON_UPDATED_CFG, "sysCfg update"},
    {RESET_REASON_MCUBOOT, "MCUBoot reset"},
    {RESET_REASON_SD_USB, "SD USB Disconnect"},
    {RESET_REASON_INVALID, "Invalid reset or first power on since flashing"},
    // MUST be NULL terminated list otherwise things WILL break
    {0, NULL}
};

const char * getResetReasonString() {
    return enumToStr(resetReasonLUT, checkResetReason());
}
