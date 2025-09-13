#pragma once

#include "bsp.h"
#include "aanderaa_conductivity_sensor.h"

// Use a constant instead of the macro to avoid redefinition issues
static constexpr int TOPIC_MAX_LEN = 255;

/**
 * @brief Sensor Application User Class
 *
 * Encapsulates the sensor application logic with platform-specific pin configuration.
 * Handles power management, sensor communication, data processing, and publishing.
 */
class SensorAppUser {
public:
    /**
     * @brief Initialize the sensor application with platform-specific pins
     * @param vbus_en_pin Platform-specific VBUS enable pin (e.g., &BB_VBUS_EN or &VBUS_EN)
     * @param pl_buck_en_pin Platform-specific PL_BUCK enable pin (e.g., &BB_PL_BUCK_EN or &PL_BUCK_EN)
     * @param vbus_settle_time_ms Time to wait for VBUS to stabilize in milliseconds
     */
    void setup_with_pins(IOPinHandle_t* vbus_en_pin, IOPinHandle_t* pl_buck_en_pin, uint32_t vbus_settle_time_ms);

    /**
     * @brief Main sensor application loop
     *
     * Reads sensor data, encodes it to CBOR, and publishes it via bristlemouth.
     * Should be called repeatedly from the main application loop.
     */
    void loop(void);

private:
    // Platform-specific pin handles - set by setup_with_pins()
    IOPinHandle_t* vbus_en_pin = nullptr;
    IOPinHandle_t* pl_buck_en_pin = nullptr;
    uint32_t vbus_settle_time_ms = 500;

    // Sensor instance
    AanderaaConductivitySensor aanderaa_conductivity_sensor;

    // Topic and message handling
    char aanderaa_conductivity_topic[TOPIC_MAX_LEN];
    int aanderaa_conductivity_topic_str_len;

    /**
     * @brief Create the topic string for publishing sensor data
     * @return Length of the created topic string
     */
    int createAanderaaConductivityDataTopic(void);
};
