#include "SensorAppUtils.h"
#include <stdio.h>
#include <inttypes.h>

namespace SensorHardwareUtils {

void initializePowerManagementPins(PowerManagementPins& pins) {
    // Platform-specific hardware initialization
    // Maps platform-specific pins to generic hardware interface

    // Forward declare the pin handles - they will be defined by the BSP
    // Use void* to avoid needing BSP headers in this compilation unit
    extern void* BB_VBUS_EN;
    extern void* BB_PL_BUCK_EN;
    extern void* VBUS_EN;
    extern void* PL_BUCK_EN;

    // Suppress unused variable warnings
    (void)VBUS_EN;
    (void)PL_BUCK_EN;

    // Try Bristleback pins first (most common case)
    pins.vbus_en = &BB_VBUS_EN;
    pins.pl_buck_en = &BB_PL_BUCK_EN;
    pins.t_vbus_time_settle = 500;

    // Note: If we need to support RS232 expander, we can add runtime detection
    // or use a different approach. For now, assume Bristleback platform.
}

void initializePower(const PowerManagementPins& pins) {
    // Platform-agnostic power initialization using abstracted hardware interface
    // Forward declarations for functions that will be available when included by app files
    extern bool IOWrite(void* pinHandle, uint8_t value);
    extern void vTaskDelay(uint32_t ticks);

    IOWrite(pins.vbus_en, 0);
    vTaskDelay(500); // Wait for Vbus to stabilize
    IOWrite(pins.pl_buck_en, 0);
}

int createSensorTopic(char* topic_buffer, size_t buffer_size, const char* topic_suffix) {
    // Forward declarations for functions that will be available when included by app files
    extern uint64_t getNodeId(void);

    int topic_str_len = snprintf(topic_buffer, buffer_size,
                                "sensor/%016" PRIx64 "%s", getNodeId(), topic_suffix);
    // Simple assertion without using configASSERT macro to avoid macro expansion issues
    if (!(topic_str_len > 0 && topic_str_len < (int)buffer_size)) {
        // Return error code instead of causing null pointer dereference
        return -1;
    }
    return topic_str_len;
}

} // namespace SensorHardwareUtils
