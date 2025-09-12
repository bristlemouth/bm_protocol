#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Hardware Abstraction Utilities for Sensor Applications
 *
 * This library provides common hardware abstraction functionality
 * that can be used by sensor applications to handle platform-specific differences.
 */
namespace SensorHardwareUtils {

/**
 * @brief Hardware abstraction structure for power management pins
 */
struct PowerManagementPins {
    void* vbus_en;      // Platform-specific VBUS enable pin (will be cast to IOPinHandle_t*)
    void* pl_buck_en;   // Platform-specific PL_BUCK enable pin (will be cast to IOPinHandle_t*)
    uint32_t t_vbus_time_settle; // VBUS settle time in milliseconds
};

/**
 * @brief Initialize platform-specific power management pins
 * @param pins Output structure to populate with platform-specific pin handles
 */
void initializePowerManagementPins(PowerManagementPins& pins);

/**
 * @brief Initialize power using abstracted pin handles
 * @param pins Power management pin configuration
 */
void initializePower(const PowerManagementPins& pins);

/**
 * @brief Create a sensor topic string
 * @param topic_buffer Output buffer for topic string
 * @param buffer_size Size of output buffer
 * @param topic_suffix Sensor-specific topic suffix (e.g., "/sofar/aanderaa_conductivity_data")
 * @return Length of created topic string
 */
int createSensorTopic(char* topic_buffer, size_t buffer_size, const char* topic_suffix);

} // namespace SensorHardwareUtils
