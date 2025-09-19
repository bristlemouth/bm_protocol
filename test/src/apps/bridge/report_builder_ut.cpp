#include "aanderaaSensor.h"
#include "abstractSensor.h"
#include "borealisSensor.h"
#include "fff.h"
#include "helpers.hpp"
#include "pmeDissolvedOxygenSensor.h"
#include "rbrCodaSensor.h"
#include "reportBuilderList.h"
#include "seapointTurbiditySensor.h"
#include "aanderaaConductivitySensor.h"
#include "softSensor.h"
#include "gtest/gtest.h"
#include <stdlib.h>
#include <time.h>

DEFINE_FFF_GLOBALS;

#define MAX_SIZE_SENSOR_PAYLOAD UINT8_MAX

typedef struct {
  uint64_t node_id;
  const void *nan_struct;
  std::function<void *(void *, uint8_t)> converter;
  void *data;
  size_t size;
} SensorInfo_t;

static SensorInfo_t info[SENSOR_TYPE_COUNT] = {
    [SENSOR_TYPE_UNKNOWN] =
        {
            0,
            NULL,
            NULL,
            NULL,
            0,
        },
    [SENSOR_TYPE_AANDERAA] =
        {
            0,
            &AanderaaSensor::NAN_AGG,
            [](void *data, uint8_t idx) -> void * {
              return &(static_cast<aanderaa_aggregations_t *>(data))[idx];
            },
            NULL,
            sizeof(aanderaa_aggregations_t),
        },
    [SENSOR_TYPE_SOFT] =
        {
            0,
            &SoftSensor::SOFT_NAN_AGG,
            [](void *data, uint8_t idx) -> void * {
              return &(static_cast<soft_aggregations_t *>(data))[idx];
            },
            NULL,
            sizeof(soft_aggregations_t),
        },
    [SENSOR_TYPE_RBR_CODA] =
        {
            0,
            &RbrCodaSensor::RBR_CODA_NAN_AGG,
            [](void *data, uint8_t idx) -> void * {
              return &(static_cast<rbr_coda_aggregations_t *>(data))[idx];
            },
            NULL,
            sizeof(rbr_coda_aggregations_t),
        },
    [SENSOR_TYPE_SEAPOINT_TURBIDITY] =
        {
            0,
            &SeapointTurbiditySensor::seapoint_turbidity_NAN_AGG,
            [](void *data, uint8_t idx) -> void * {
              return &(static_cast<seapoint_turbidity_aggregations_t *>(data))[idx];
            },
            NULL,
            sizeof(seapoint_turbidity_aggregations_t),
        },
    [SENSOR_TYPE_AANDERAA_CONDUCTIVITY] =
        {
            0,
            &AanderaaConductivitySensor::aanderaa_conductivity_NAN_AGG,
            [](void *data, uint8_t idx) -> void * {
              return &(static_cast<aanderaa_conductivity_aggregations_t *>(data))[idx];
            },
            NULL,
            sizeof(aanderaa_conductivity_aggregations_t),
        },
    [SENSOR_TYPE_BOREALIS] =
        {
            0,
            &BorealisSensor::AOS_BOREALIS_NAN_AGG,
            [](void *data, uint8_t idx) -> void * {
              return &(static_cast<BorealisAggregationData_t *>(data))[idx];
            },
            NULL,
            sizeof(BorealisAggregationData_t),
        },
    [SENSOR_TYPE_PME_DO] =
        {
            0,
            &PmeDissolvedOxygenSensor::PME_DO_NAN_AGG,
            [](void *data, uint8_t idx) -> void * {
              return &(static_cast<pme_dissolved_oxygen_aggregations_t *>(data))[idx];
            },
            NULL,
            sizeof(pme_dissolved_oxygen_aggregations_t),
        },
    [SENSOR_TYPE_PME_WIPE] =
        {
            0,
            NULL,
            NULL,
            NULL,
            0,
        }
};

static void assign_info_node_ids(void) {
  uint32_t seed = time(NULL);
  rand_sequence_unique rsu(seed, seed + 1);

  for (size_t i = 0; i < SENSOR_TYPE_COUNT; i++) {
    info[i].node_id = rsu.next();
  }
}

static void assign_random_data(uint8_t samples_per_report, uint8_t fill_num_nan = 0) {
  rnd_gen rnd;
  uint8_t *data = NULL;
  size_t size = 0;

  for (size_t i = 0; i < SENSOR_TYPE_COUNT; i++) {

    // If this is a sensor that has aggregation data but also a scalable payload
    if (info[i].size) {
      size = info[i].size * samples_per_report;
      data = (uint8_t *)malloc(size);
      info[i].data = data;
      // Fill the data with nan values if required
      for (uint8_t j = 0; j < fill_num_nan; j++) {
        memcpy(data, info[i].nan_struct, info[i].size);
        // All items must be of the element size even if they are nans
        data += info[i].size;
        size -= info[i].size;
      }
      rnd.rnd_array(data, size);
    }
  }
}

static void free_random_data(void) {
  for (size_t i = 0; i < SENSOR_TYPE_COUNT; i++) {
    if (info[i].size) {
      free(info[i].data);
    }
  }
}

static void compare_data(ReportBuilderLinkedList &list, uint8_t samples_per_report,
                         bool use_nan, size_t num_nans = 0) {
  const void *cmp_list_p = NULL;
  const void *cmp_raw_p = NULL;
  void *cmp_offset_p = NULL;
  uint32_t cmp_size = 0;
  report_builder_element_t *element = NULL;
  bool tested = false;

  for (size_t i = 0; i < SENSOR_TYPE_COUNT; i++) {
    element = list.findElement(info[i].node_id);
    tested = false;

    if (element) {
      cmp_offset_p = static_cast<uint8_t *>(element->sensor_data) + num_nans * info[i].size;
      if (use_nan) {
        for (size_t j = 0; j < num_nans; j++) {
          cmp_list_p = info[i].converter(element->sensor_data, j);
          cmp_raw_p = info[i].nan_struct;
          cmp_size = info[i].size;
          ASSERT_NE(cmp_size, 0);
          EXPECT_EQ(memcmp(cmp_raw_p, cmp_list_p, cmp_size), 0);
        }

        if (num_nans == samples_per_report) {
          tested = true;
        }
      }

      if (cmp_offset_p <
          static_cast<uint8_t *>(element->sensor_data) + samples_per_report * info[i].size) {
        cmp_list_p = cmp_offset_p;
        cmp_raw_p = static_cast<uint8_t *>(info[i].data) + info[i].size * num_nans;
        cmp_size = info[i].size * samples_per_report - info[i].size * num_nans;
        ASSERT_NE(cmp_size, 0);
        EXPECT_EQ(memcmp(cmp_raw_p, cmp_list_p, cmp_size), 0);
        tested = true;
      }

      ASSERT_EQ(tested, true);
    }
  }
}

TEST(ReportBuilderLinkedList, AddingSamples) {
  rnd_gen rnd;
  ReportBuilderLinkedList list;
  uint8_t samples_per_report = rnd.rnd_int(10, 3);

  assign_info_node_ids();

  // Adding NAN Data And Compare
  for (size_t i = 0; i < SENSOR_TYPE_COUNT; i++) {
    size_t size = info[i].size;
    if (size) {
      list.findElementAndAddSampleToElement(info[i].node_id, i, NULL, size, samples_per_report,
                                            samples_per_report - 1);
    }
  }

  compare_data(list, samples_per_report, true, samples_per_report);

  list.clear();

  // Adding Random Data
  assign_random_data(samples_per_report);

  for (size_t i = 0; i < SENSOR_TYPE_COUNT; i++) {
    size_t size = info[i].size;
    if (size) {
      for (uint8_t j = 0; j < samples_per_report; j++) {
        list.findElementAndAddSampleToElement(info[i].node_id, i,
                                              info[i].converter(info[i].data, j), size,
                                              samples_per_report, j);
      }
    }
  }

  compare_data(list, samples_per_report, false);

  list.clear();
  free_random_data();

  uint8_t rand_nans = rnd.rnd_int(samples_per_report - 1, 2);
  // Test with backfilled NANs
  assign_random_data(samples_per_report, rand_nans);

  for (size_t i = 0; i < SENSOR_TYPE_COUNT; i++) {
    size_t size = info[i].size;
    if (size) {
      for (uint8_t j = rand_nans; j < samples_per_report; j++) {
        list.findElementAndAddSampleToElement(info[i].node_id, i,
                                              info[i].converter(info[i].data, j), size,
                                              samples_per_report, j);
      }
    }
  }

  compare_data(list, samples_per_report, true, rand_nans);

  list.clear();
  free_random_data();
}
