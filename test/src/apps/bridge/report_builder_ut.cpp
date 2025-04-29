#include "aanderaaSensor.h"
#include "abstractSensor.h"
#include "borealisSensor.h"
#include "fff.h"
#include "helpers.hpp"
#include "pmeDissolvedOxygenSensor.h"
#include "rbrCodaSensor.h"
#include "reportBuilderList.h"
#include "seapointTurbiditySensor.h"
#include "softSensor.h"
#include "gtest/gtest.h"
#include <stdlib.h>
#include <time.h>

DEFINE_FFF_GLOBALS;

#define MAX_SIZE_SENSOR_PAYLOAD 256

typedef struct {
  uint64_t node_id;
  const void *nan_struct;
  size_t nan_size;
  std::function<void *(void *, uint8_t)> converter;
  void *data;
  size_t size;
} SensorInfo_t;

static SensorInfo_t info[SENSOR_TYPE_COUNT] = {
    [SENSOR_TYPE_UNKNOWN] =
        {
            0,
            NULL,
            0,
            NULL,
            NULL,
            0,
        },
    [SENSOR_TYPE_AANDERAA] =
        {
            0,
            &AanderaaSensor::NAN_AGG,
            sizeof(aanderaa_aggregations_t),
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
            sizeof(soft_aggregations_t),
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
            sizeof(rbr_coda_aggregations_t),
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
            sizeof(seapoint_turbidity_aggregations_t),
            [](void *data, uint8_t idx) -> void * {
              return &(static_cast<seapoint_turbidity_aggregations_t *>(data))[idx];
            },
            NULL,
            sizeof(seapoint_turbidity_aggregations_t),
        },
    [SENSOR_TYPE_BOREALIS_SPECTRUM] =
        {
            0,
            NULL,
            0,
            NULL,
            NULL,
            0,
        },
    [SENSOR_TYPE_BOREALIS_LEVELS] =
        {
            0,
            NULL,
            0,
            NULL,
            NULL,
            0,
        },
    [SENSOR_TYPE_PME_DO] =
        {
            0,
            &PmeDissolvedOxygenSensor::PME_DO_NAN_AGG,
            sizeof(pme_dissolved_oxygen_aggregations_t),
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
            0,
            NULL,
            NULL,
            0,
        },
    [SENSOR_TYPE_BOREALIS_LEVEL_STATISTICS] =
        {
            0,
            &BorealisSensorLevelStatistics::AOS_BOREALIS_NAN_AGG,
            sizeof(BorealisSensorLevelStatistics::LevelStatisticsData_t),
            [](void *data, uint8_t idx) -> void * {
              return &(static_cast<BorealisSensorLevelStatistics::LevelStatisticsData_t *>(
                  data))[idx];
            },
            NULL,
            0,
        },
    [SENSOR_TYPE_BOREALIS_RECORDING_STATUS] =
        {
            0,
            NULL,
            0,
            NULL,
            NULL,
            0,
        },
};

static void assign_info_node_ids(void) {
  uint32_t seed = time(NULL);
  rand_sequence_unique rsu(seed, seed + 1);

  for (size_t i = 0; i < SENSOR_TYPE_COUNT; i++) {
    info[i].node_id = rsu.next();
  }
}

static void assign_random_data(uint8_t samples_per_report) {
  rnd_gen rnd;
  uint8_t *data = NULL;
  size_t size = 0;

  for (size_t i = 0; i < SENSOR_TYPE_COUNT; i++) {

    // If this is a sensor that has aggregation data but also a scalable payload
    if (!info[i].size && info[i].nan_size) {
      info[i].size = rnd.rnd_int(MAX_SIZE_SENSOR_PAYLOAD, info[i].nan_size);
    }

    if (info[i].size) {
      size = info[i].size * samples_per_report;
      data = (uint8_t *)malloc(size);
      rnd.rnd_array(data, size);
      info[i].data = data;
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
                         bool use_nan) {
  const void *cmp_list_p = NULL;
  const void *cmp_raw_p = NULL;
  uint32_t cmp_size = 0;
  report_builder_element_t *element = NULL;

  for (size_t i = 0; i < SENSOR_TYPE_COUNT; i++) {
    element = list.findElement(info[i].node_id);

    if (element && cmp_raw_p) {
      for (size_t j = 0; j < samples_per_report; j++) {
        cmp_list_p = info[i].converter(element->sensor_data, j);
        if (use_nan) {
          cmp_raw_p = info[i].nan_struct;
          cmp_size = info[i].nan_size;
        } else {
          cmp_raw_p = info[i].converter(info[i].data, j);
          cmp_size = info[i].size;
        }
        ASSERT_NE(cmp_size, 0);
        ASSERT_EQ(memcmp(cmp_raw_p, cmp_list_p, cmp_size), 0);
      }
    }
  }
}

TEST(ReportBuilderLinkedList, AddingSamples) {
  rnd_gen rnd;
  ReportBuilderLinkedList list;
  uint8_t samples_per_report = rnd.rnd_int(5, 3);

  assign_info_node_ids();

  // Adding NAN Data And Compare
  for (size_t i = 0; i < SENSOR_TYPE_COUNT; i++) {
    size_t size = info[i].nan_size;
    if (size) {
      list.findElementAndAddSampleToElement(info[i].node_id, i, NULL, size, samples_per_report,
                                            samples_per_report - 1);
    }
  }

  compare_data(list, samples_per_report, true);

  list.clear();

  // Adding Random Data
  assign_random_data(samples_per_report);

  for (size_t i = 0; i < SENSOR_TYPE_COUNT; i++) {
    size_t size = info[i].size;
    if (size) {
      for (uint8_t j = 0; j < samples_per_report; j++) {
        list.findElementAndAddSampleToElement(info[i].node_id, i, info[i].data, size,
                                              samples_per_report, j);
      }
    }
  }

  compare_data(list, samples_per_report, false);

  list.clear();
  free_random_data();
}
