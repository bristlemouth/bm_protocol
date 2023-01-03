
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// List of peripherals that cannot operate in STOP2 mode
// TODO - Assign these dynamically as needed?
#define LPM_USART2      (1UL << 0)
#define LPM_LPTIM2      (1UL << 1)
#define LPM_MIN_RUN     (1UL << 2)
#define LPM_USB         (1UL << 3)
#define LPM_SDMMC       (1UL << 4)
#define LPM_BOOT        (1UL << 5)
#define LPM_IRIDIUM     (1UL << 6)
#define LPM_I2S         (1UL << 7)
#define LPM_BRIDGE      (1UL << 8)
#define LPM_NOTECARD    (1UL << 9)

// If any of the above peripherals are ok in STOP1 mode, add them to LPM_OK_IN_STOP1
// #define LPM_OK_IN_STOP1 (LPM_LPTIM2)
#define LPM_OK_IN_STOP1 (0)

void lpmInit();

void lpmPeripheralActive(uint32_t peripheralMask);
void lpmPeripheralActiveFromISR(uint32_t peripheralMask);

void lpmPeripheralInactive(uint32_t peripheralMask);
void lpmPeripheralInactiveFromISR(uint32_t peripheralMask);

void lpmPreSleepProcessing();
void lpmPostSleepProcessing();

#ifdef __cplusplus
}
#endif
