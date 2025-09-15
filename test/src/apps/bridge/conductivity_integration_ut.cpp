// Bridge components
#include "reportBuilderList.h"
#include "aanderaaConductivitySensor.h"

// CBOR message handling
#include "aanderaa_conductivity_msg.h"

// Test framework
#include "gtest/gtest.h"
#include "fff.h"

// Shared test utilities
#include "conductivity_test_helpers.h"

// Test utilities
#include <cmath>
#include <cstring>

extern "C" {
#include "mock_bridgeLog.h"
}

DEFINE_FFF_GLOBALS;

// Helper function to verify aggregated data using standard test constants
void verifyAggregatedDataIntegrity(const aanderaa_conductivity_aggregations_t* actual,
                                  uint32_t expected_reading_count = 1) {
  EXPECT_NEAR(actual->conductivity_mean_ms_cm, ConductivityTestData::CONDUCTIVITY_MS_CM, ConductivityTestData::PRECISION_TOLERANCE);
  EXPECT_NEAR(actual->temperature_mean_deg_c, ConductivityTestData::TEMPERATURE_DEG_C, ConductivityTestData::PRECISION_TOLERANCE);
  EXPECT_NEAR(actual->salinity_mean_psu, ConductivityTestData::SALINITY_PSU, ConductivityTestData::PRECISION_TOLERANCE);
  EXPECT_NEAR(actual->water_density_mean_kg_m3, ConductivityTestData::WATER_DENSITY_KG_M3, ConductivityTestData::DENSITY_TOLERANCE);
  EXPECT_NEAR(actual->sound_speed_mean_m_s, ConductivityTestData::SOUND_SPEED_M_S, ConductivityTestData::SPEED_TOLERANCE);
  EXPECT_NEAR(actual->depth_mean_m, ConductivityTestData::DEPTH_M, ConductivityTestData::DEPTH_TOLERANCE);
  EXPECT_EQ(actual->reading_count, expected_reading_count);
}

/**
 * Integration tests for conductivity sensor data flow.
 * These tests verify the integration between CBOR message handling
 * and bridge data aggregation (ReportBuilderLinkedList).
 *
 * This adds testing testing that wasn't covered by the existing
 * report_builder_ut.cpp tests.
 */

// Test CBOR encoding/decoding integration
TEST(ConductivityIntegration, CborEncodeDecode) {
    // Create standard test sensor data
    AanderaaConductivityMsg::Data original_data = createStandardTestData();

    // Encode to CBOR
    uint8_t cbor_buffer[256];
    size_t cbor_size;
    CborError err = AanderaaConductivityMsg::encode(original_data, cbor_buffer, sizeof(cbor_buffer), &cbor_size);
    ASSERT_EQ(err, CborNoError);
    EXPECT_GT(cbor_size, 0);

    // Decode from CBOR
    AanderaaConductivityMsg::Data decoded_data;
    err = AanderaaConductivityMsg::decode(decoded_data, cbor_buffer, cbor_size);
    ASSERT_EQ(err, CborNoError);

    // Verify data integrity through encode/decode cycle
    verifyCompleteDataIntegrity(original_data, decoded_data);
}

// Test integration with ReportBuilderLinkedList (the existing 60% integration test)
TEST(ConductivityIntegration, ReportBuilderIntegration) {
    ReportBuilderLinkedList report_list;
    uint64_t test_node_id = 0x1234567890ABCDEF;

    // Create aggregated data using standard test constants
    aanderaa_conductivity_aggregations_t agg_data = {
        .conductivity_mean_ms_cm = ConductivityTestData::CONDUCTIVITY_MS_CM,
        .temperature_mean_deg_c = ConductivityTestData::TEMPERATURE_DEG_C,
        .salinity_mean_psu = ConductivityTestData::SALINITY_PSU,
        .water_density_mean_kg_m3 = ConductivityTestData::WATER_DENSITY_KG_M3,
        .sound_speed_mean_m_s = ConductivityTestData::SOUND_SPEED_M_S,
        .depth_mean_m = ConductivityTestData::DEPTH_M,
        .reading_count = 3
    };

    // Add to report builder (this is what reportBuilderAddToQueue would do)
    report_list.findElementAndAddSampleToElement(
        test_node_id,
        SENSOR_TYPE_AANDERAA_CONDUCTIVITY,
        &agg_data,
        sizeof(agg_data),
        1, // samples_per_report
        0  // sample_counter
    );

    // Verify data was stored correctly
    report_builder_element_t* element = report_list.findElement(test_node_id);
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(element->node_id, test_node_id);
    EXPECT_EQ(element->sensor_type, SENSOR_TYPE_AANDERAA_CONDUCTIVITY);
    EXPECT_EQ(element->sample_counter, 1);

    // Verify stored data matches input using helper function
    const aanderaa_conductivity_aggregations_t* stored_data =
        static_cast<const aanderaa_conductivity_aggregations_t*>(element->sensor_data);
    verifyAggregatedDataIntegrity(stored_data, 3);
}

// Test multi-sample aggregation integration
TEST(ConductivityIntegration, MultiSampleAggregation) {
    ReportBuilderLinkedList report_list;
    uint64_t test_node_id = 0x1234567890ABCDEF;

    // Add multiple samples with varying values
    for (int i = 0; i < 3; i++) {
        aanderaa_conductivity_aggregations_t agg_data = {
            .conductivity_mean_ms_cm = 50.0 + i * 0.1,
            .temperature_mean_deg_c = 23.0 + i * 0.05,
            .salinity_mean_psu = 35.0 + i * 0.02,
            .water_density_mean_kg_m3 = 1025.0 + i * 0.1,
            .sound_speed_mean_m_s = 1498.0 + i * 0.1,
            .depth_mean_m = 10.0 + i * 0.1,
            .reading_count = 1
        };

        report_list.findElementAndAddSampleToElement(
            test_node_id,
            SENSOR_TYPE_AANDERAA_CONDUCTIVITY,
            &agg_data,
            sizeof(agg_data),
            3, // samples_per_report
            i  // sample_counter
        );
    }

    // Verify all samples were stored
    report_builder_element_t* element = report_list.findElement(test_node_id);
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(element->sample_counter, 3);

    // Verify we can access all stored samples
    aanderaa_conductivity_aggregations_t* stored_data =
        static_cast<aanderaa_conductivity_aggregations_t*>(element->sensor_data);

    // Check first sample
    EXPECT_NEAR(stored_data[0].conductivity_mean_ms_cm, 50.0, 0.001);
    EXPECT_NEAR(stored_data[0].temperature_mean_deg_c, 23.0, 0.001);

    // Check last sample
    EXPECT_NEAR(stored_data[2].conductivity_mean_ms_cm, 50.2, 0.001);
    EXPECT_NEAR(stored_data[2].temperature_mean_deg_c, 23.1, 0.001);
}

// Test end-to-end data flow: CBOR → Aggregation → Report Builder
TEST(ConductivityIntegration, EndToEndDataFlow) {
    // 1. Start with standard test sensor data (simulating network input)
    AanderaaConductivityMsg::Data sensor_data = createStandardTestData();

    // 2. Encode to CBOR (simulating network transmission)
    uint8_t cbor_buffer[256];
    size_t cbor_size;
    CborError err = AanderaaConductivityMsg::encode(sensor_data, cbor_buffer, sizeof(cbor_buffer), &cbor_size);
    ASSERT_EQ(err, CborNoError);

    // 3. Decode CBOR (simulating sensor driver processing)
    AanderaaConductivityMsg::Data decoded_data;
    err = AanderaaConductivityMsg::decode(decoded_data, cbor_buffer, cbor_size);
    ASSERT_EQ(err, CborNoError);

    // 4. Convert to aggregation format (simulating sensor aggregation)
    aanderaa_conductivity_aggregations_t agg_data = {
        .conductivity_mean_ms_cm = decoded_data.conductivity_ms_cm,
        .temperature_mean_deg_c = decoded_data.temperature_deg_c,
        .salinity_mean_psu = decoded_data.salinity_psu,
        .water_density_mean_kg_m3 = decoded_data.water_density_kg_m3,
        .sound_speed_mean_m_s = decoded_data.sound_speed_m_s,
        .depth_mean_m = decoded_data.depth_m,
        .reading_count = 1
    };

    // 5. Add to report builder (simulating bridge aggregation)
    ReportBuilderLinkedList report_list;
    uint64_t test_node_id = 0x1234567890ABCDEF;

    report_list.findElementAndAddSampleToElement(
        test_node_id,
        SENSOR_TYPE_AANDERAA_CONDUCTIVITY,
        &agg_data,
        sizeof(agg_data),
        1, // samples_per_report
        0  // sample_counter
    );

    // 6. Verify end-to-end data integrity
    report_builder_element_t* element = report_list.findElement(test_node_id);
    ASSERT_NE(element, nullptr);

    const aanderaa_conductivity_aggregations_t* final_data =
        static_cast<const aanderaa_conductivity_aggregations_t*>(element->sensor_data);

    // Verify data survived the complete pipeline: CBOR → decode → aggregate → store
    verifyAggregatedDataIntegrity(final_data, 1);

    // This test demonstrates the complete integration:
    // Network CBOR → Sensor Processing → Bridge Aggregation → Report Generation
}
