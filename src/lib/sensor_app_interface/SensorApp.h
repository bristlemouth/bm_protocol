#pragma once

#include "SensorAppUtils.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Generic base class template for sensor applications
 *
 * This template provides common functionality for all sensor applications including:
 * - Hardware abstraction and power management
 * - Topic creation and CBOR message handling
 * - Setup and loop patterns
 *
 * @tparam SensorType The sensor class (e.g., AanderaaConductivitySensor)
 * @tparam MessageType The message class (e.g., AanderaaConductivityMsg)
 * @tparam DataType The data structure type (e.g., AanderaaConductivityMsg::Data)
 */
template<typename SensorType, typename MessageType, typename DataType>
class SensorApp {
public:
    /**
     * @brief Initialize the sensor application
     */
    void setup();

    /**
     * @brief Main application loop
     */
    void loop();

protected:
    // Pure virtual methods that must be implemented by derived classes

    /**
     * @brief Get the sensor-specific topic suffix
     * @return Topic suffix string (e.g., "/sofar/aanderaa_conductivity_data")
     */
    virtual const char* getSensorTopicSuffix() const = 0;

    /**
     * @brief Get the maximum message size for this sensor
     * @return Maximum message size in bytes
     */
    virtual size_t getMaxMessageSize() const = 0;

    /**
     * @brief Encode sensor data into CBOR format
     * @param data Sensor data to encode
     * @param cbor_buffer Output buffer for CBOR data
     * @param buffer_size Size of output buffer
     * @param encoded_len Output parameter for actual encoded length
     * @return CBOR error code
     */
    virtual int encodeMessage(const DataType& data, uint8_t* cbor_buffer,
                             size_t buffer_size, size_t* encoded_len) const = 0;

    /**
     * @brief Get the human-readable sensor name
     * @return Sensor name string (e.g., "Aanderaa Conductivity")
     */
    virtual const char* getSensorName() const = 0;

private:
    SensorType sensor;
    char topic[128];
    SensorHardwareUtils::PowerManagementPins power_pins;

    /**
     * @brief Initialize hardware abstraction
     */
    void initializeHardware();

    /**
     * @brief Initialize power management
     */
    void initializePower();

    /**
     * @brief Initialize sensor topic
     */
    void initializeTopic();
};

// Template implementation - must be in header file
#include "SensorApp.cpp"
