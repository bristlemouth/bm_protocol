/**
 * @file aanderaaConductivitySensor_minimal_stub.cpp
 * @brief Minimal stub for AanderaaConductivitySensor class for unit tests
 *
 * This stub provides only the essential methods needed for report_builder_tests
 * without bringing in all the bridge-specific dependencies.
 */

#include "aanderaaConductivitySensor.h"

// Note: aanderaa_conductivity_NAN_AGG is already defined as constexpr in the header

// Minimal implementation of setupSensorPointers for tests
void AanderaaConductivitySensor::setupSensorPointers(report_builder_element_t *element,
                                                     const void **nan_sample,
                                                     void **dst) {
    *nan_sample = &AanderaaConductivitySensor::aanderaa_conductivity_NAN_AGG;
    *dst = &(static_cast<AanderaaConductivityAggregations *>(
        element->sensor_data))[element->sample_counter];
}
