#pragma once
#include "SensorApp.h"
#include "AanderaaConductivitySensor.h"
#include "aanderaa_conductivity_msg.h"

/**
 * @brief Aanderaa Conductivity Sensor Application
 *
 * Specialized sensor application for Aanderaa 5990 conductivity sensors.
 * Inherits from SensorApp template to get common functionality.
 */
class AanderaaConductivityApp : public SensorApp<AanderaaConductivitySensor, AanderaaConductivityMsg, AanderaaConductivityMsg::Data> {
protected:
    const char* getSensorTopicSuffix() const override {
        return "/sofar/aanderaa_conductivity_data";
    }

    size_t getMaxMessageSize() const override {
        return 256;
    }

    int encodeMessage(const AanderaaConductivityMsg::Data& data, uint8_t* cbor_buffer,
                     size_t buffer_size, size_t* encoded_len) const override {
        return AanderaaConductivityMsg::encode(const_cast<AanderaaConductivityMsg::Data&>(data),
                                              cbor_buffer, buffer_size, encoded_len);
    }

    const char* getSensorName() const override {
        return "Aanderaa Conductivity";
    }
};
