// Template implementation file for SensorApp
// This file is included by SensorApp.h and contains the template method implementations

template<typename SensorType, typename MessageType, typename DataType>
void SensorApp<SensorType, MessageType, DataType>::setup() {
    initializeHardware();
    initializePower();
    initializeTopic();
    sensor.init();
}

template<typename SensorType, typename MessageType, typename DataType>
void SensorApp<SensorType, MessageType, DataType>::loop() {
    // Forward declarations for functions that will be available when included by app files
    extern void vTaskDelay(uint32_t ticks);
    extern bool publish(const char* topic, uint8_t* data, size_t len);

    if (sensor.read()) {
        DataType data = sensor.getData();

        uint8_t cbor_buffer[getMaxMessageSize()];
        size_t encoded_len;

        int cbor_err = encodeMessage(data, cbor_buffer, sizeof(cbor_buffer), &encoded_len);
        if (cbor_err == 0) { // CborNoError
            publish(topic, cbor_buffer, encoded_len);
        }
    }

    vTaskDelay(1000); // 1 second delay
}

template<typename SensorType, typename MessageType, typename DataType>
void SensorApp<SensorType, MessageType, DataType>::initializeHardware() {
    SensorHardwareUtils::initializePowerManagementPins(power_pins);
}

template<typename SensorType, typename MessageType, typename DataType>
void SensorApp<SensorType, MessageType, DataType>::initializePower() {
    SensorHardwareUtils::initializePower(power_pins);
}

template<typename SensorType, typename MessageType, typename DataType>
void SensorApp<SensorType, MessageType, DataType>::initializeTopic() {
    SensorHardwareUtils::createSensorTopic(topic, sizeof(topic), getSensorTopicSuffix());
}
