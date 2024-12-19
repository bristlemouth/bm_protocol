#include "rbrCodaSensor.h"
#include "app_config.h"
#include "avgSampler.h"
#include "spotter.h"
#include "pubsub.h"
#include "bm_rbr_data_msg.h"
#include "bridgeLog.h"
#include "cbor.h"
#include "device_info.h"
#include "reportBuilder.h"
#include "semphr.h"
#include "stm32_rtc.h"
#include "topology_sampler.h"
#include "app_util.h"
#include <new>
#ifdef RAW_PRESSURE_ENABLE
#include "rbrPressureProcessor.h"
#endif // RAW_PRESSURE_ENABLE

bool RbrCodaSensor::subscribe() {
  bool rval = false;
  char *sub = static_cast<char *>(pvPortMalloc(BM_TOPIC_MAX_LEN));
  configASSERT(sub);
  int topic_strlen =
      snprintf(sub, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", node_id, subtag);
  if (topic_strlen > 0) {
    rval = bm_sub_wl(sub, topic_strlen, rbrCodaSubCallback) == BmOK;
  }
  vPortFree(sub);
  return rval;
}

void RbrCodaSensor::rbrCodaSubCallback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                       const uint8_t *data, uint16_t data_len, uint8_t type,
                                       uint8_t version) {
  (void)type;
  (void)version;
  printf("RBR CODA data received from node %016" PRIx64 " On topic: %.*s\n", node_id, topic_len,
         topic);
  RbrCoda_t *rbr_coda = static_cast<RbrCoda_t *>(sensorControllerFindSensorById(node_id));
  if (rbr_coda && rbr_coda->type == SENSOR_TYPE_RBR_CODA) {
    if (xSemaphoreTake(rbr_coda->_mutex, portMAX_DELAY)) {
      static BmRbrDataMsg::Data rbr_data;
      if (BmRbrDataMsg::decode(rbr_data, data, data_len) == CborNoError) {
#ifdef RAW_PRESSURE_ENABLE
    if(rbrPressureProcessorIsStarted()) {
      if(!rbrPressureProcessorAddSample(rbr_data, 1000)) {
        printf("Failed to add sample to rbrPressureProcessor\n");
      }
    }
#endif // RAW_PRESSURE_ENABLE
        char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
        configASSERT(log_buf);
        rbr_coda->temp_deg_c.addSample(rbr_data.temperature_deg_c);
        rbr_coda->pressure_deci_bar.addSample(rbr_data.pressure_deci_bar);
        rbr_coda->reading_count++;
        // Large floats get formatted in scientific notation,
        // so we print integer seconds and millis separately.
        uint64_t reading_time_sec = rbr_data.header.reading_time_utc_ms / 1000U;
        uint32_t reading_time_millis = rbr_data.header.reading_time_utc_ms % 1000U;
        uint64_t sensor_reading_time_sec = rbr_data.header.sensor_reading_time_ms / 1000U;
        uint32_t sensor_reading_time_millis = rbr_data.header.sensor_reading_time_ms % 1000U;

        uint32_t current_timestamp = pdTICKS_TO_MS(xTaskGetTickCount());
        if ((current_timestamp - rbr_coda->last_timestamp > rbr_coda->configured_reading_period_ms + 1000u) ||
            rbr_coda->reading_count == 1U) {
          printf("Updating rbr_coda %016" PRIx64 " node position, current_time = %" PRIu32
                 ", last_time = %" PRIu32 ", reading count: %" PRIu32 "\n",
                 node_id, current_timestamp, rbr_coda->last_timestamp, rbr_coda->reading_count);
          rbr_coda->node_position = topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));
        }
        rbr_coda->last_timestamp = current_timestamp;

        // Use the latest sensor type to determine the sensor type string
        const char *sensor_type_str;
        if (rbr_data.sensor_type == BmRbrDataMsg::SensorType::TEMPERATURE) {
          sensor_type_str = "RBR.T";
        } else if (rbr_data.sensor_type == BmRbrDataMsg::SensorType::PRESSURE) {
          sensor_type_str = "RBR.D";
        } else if (rbr_data.sensor_type == BmRbrDataMsg::SensorType::PRESSURE_AND_TEMPERATURE) {
          sensor_type_str = "RBR.DT";
        } else {
          sensor_type_str = "RBR.UNKNOWN";
        }

        size_t log_buflen =
            snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                     "%016" PRIx64 "," // Node Id
                     "%" PRIi8 ","     // node_position
                     "%s,"             // node_app_name
                     "%" PRIu64 ","    // reading_uptime_millis
                     "%" PRIu64 "."    // reading_time_utc_ms seconds part
                     "%03" PRIu32 ","  // reading_time_utc_ms millis part
                     "%" PRIu64 "."    // sensor_reading_time_ms seconds part
                     "%03" PRIu32 ","  // sensor_reading_time_ms millis part
                     "%.3f,"           // temperature_deg_c
                     "%.3f\n",         // pressure_deci_bar
                     node_id, rbr_coda->node_position, sensor_type_str,
                     rbr_data.header.reading_uptime_millis, reading_time_sec,
                     reading_time_millis, sensor_reading_time_sec, sensor_reading_time_millis,
                     rbr_data.temperature_deg_c, rbr_data.pressure_deci_bar);
        if (log_buflen > 0) {
          BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_IND, log_buf, log_buflen);
        } else {
          printf("ERROR: Failed to print rbr_coda individual log\n");
        }
        vPortFree(log_buf);
      }
      xSemaphoreGive(rbr_coda->_mutex);
    } else {
      printf("Failed to get the subbed RBR CODA mutex after getting a new reading\n");
    }
  }
}

void RbrCodaSensor::aggregate(void) {
  char *log_buf = static_cast<char *>(pvPortMalloc(SENSOR_LOG_BUF_SIZE));
  configASSERT(log_buf);
  if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
    size_t log_buflen = 0;
    rbr_coda_aggregations_t aggs = {.temp_mean_deg_c = NAN,
                                    .pressure_mean_deci_bar = NAN,
                                    .pressure_stdev_deci_bar = NAN,
                                    .reading_count = 0,
                                    .sensor_type = BmRbrDataMsg::SensorType::UNKNOWN};
    aggs.sensor_type = rbrCodaGetSensorType(node_id);
    latest_sensor_type = aggs.sensor_type;
    if (temp_deg_c.getNumSamples() >= MIN_READINGS_FOR_AGGREGATION) {
      aggs.temp_mean_deg_c = temp_deg_c.getMean();
      aggs.pressure_mean_deci_bar = pressure_deci_bar.getMean();
      aggs.pressure_stdev_deci_bar = pressure_deci_bar.getStd(aggs.pressure_mean_deci_bar);
      aggs.reading_count = reading_count;

      if (aggs.temp_mean_deg_c < TEMP_SAMPLE_MEMBER_MIN) {
        aggs.temp_mean_deg_c = -HUGE_VAL;
      } else if (aggs.temp_mean_deg_c > TEMP_SAMPLE_MEMBER_MAX) {
        aggs.temp_mean_deg_c = HUGE_VAL;
      }

      if (aggs.pressure_mean_deci_bar < PRESSURE_SAMPLE_MEMBER_MIN ||
          aggs.pressure_mean_deci_bar > PRESSURE_SAMPLE_MEMBER_MAX) {
        aggs.pressure_mean_deci_bar = NAN;
      }

      if (aggs.pressure_stdev_deci_bar < PRESSURE_STDEV_SAMPLE_MEMBER_MIN) {
        aggs.pressure_stdev_deci_bar = NAN;
      } else if (aggs.pressure_stdev_deci_bar > PRESSURE_STDEV_SAMPLE_MEMBER_MAX) {
        aggs.pressure_stdev_deci_bar = HUGE_VAL;
      }
    }
    static constexpr uint8_t TIME_STR_BUFSIZE = 50;
    char time_str[TIME_STR_BUFSIZE];
    if (!logRtcGetTimeStr(time_str, TIME_STR_BUFSIZE, true)) {
      printf("Failed to get RTC time string for RBR CODA aggregation\n");
      snprintf(time_str, TIME_STR_BUFSIZE, "0");
    }

    int8_t node_position = topology_sampler_get_node_position(node_id, pdTICKS_TO_MS(5000));

    // Use the latest sensor type to determine the sensor type string
    const char *sensor_type_str;
    if (latest_sensor_type == BmRbrDataMsg::SensorType::TEMPERATURE) {
      sensor_type_str = "RBR.T";
    } else if (latest_sensor_type == BmRbrDataMsg::SensorType::PRESSURE) {
      sensor_type_str = "RBR.D";
    } else if (latest_sensor_type == BmRbrDataMsg::SensorType::PRESSURE_AND_TEMPERATURE) {
      sensor_type_str = "RBR.DT";
    } else {
      sensor_type_str = "RBR.UNKNOWN";
    }

    log_buflen = snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                          "%016" PRIx64 "," // Node Id
                          "%" PRIi8 ","     // node_position
                          "%s,"             // node_app_name
                          "%s,"             // timeStamp(ticks/UTC)
                          "%" PRIu32 ","    // reading_count
                          "%.3f,"           // temp_mean_deg_c
                          "%.3f,"           // pressure_mean_deci_bar
                          "%.3f\n",         // pressure_stdev_deci_bar
                          node_id, node_position, sensor_type_str, time_str, aggs.reading_count,
                          aggs.temp_mean_deg_c, aggs.pressure_mean_deci_bar,
                          aggs.pressure_stdev_deci_bar);
    if (log_buflen > 0) {
      BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_AGG, log_buf, log_buflen);
    } else {
      printf("ERROR: Failed to print rbr_coda aggregation log\n");
    }
    reportBuilderAddToQueue(node_id, SENSOR_TYPE_RBR_CODA, static_cast<void *>(&aggs),
                            sizeof(rbr_coda_aggregations_t), REPORT_BUILDER_SAMPLE_MESSAGE);
    temp_deg_c.clear();
    pressure_deci_bar.clear();
    reading_count = 0;
    xSemaphoreGive(_mutex);
  } else {
    printf("Failed to get the RBR CODA mutex after getting a new reading\n");
  }
  vPortFree(log_buf);
}

RbrCoda_t *createRbrCodaSub(uint64_t node_id, uint32_t rbr_coda_agg_period_ms,
                            uint32_t averager_max_samples, uint32_t configured_reading_period_ms) {
  RbrCoda_t *new_sub = static_cast<RbrCoda_t *>(pvPortMalloc(sizeof(RbrCoda_t)));
  new_sub = new (new_sub) RbrCoda_t();
  configASSERT(new_sub);

  new_sub->_mutex = xSemaphoreCreateMutex();
  configASSERT(new_sub->_mutex);

  new_sub->node_id = node_id;
  new_sub->type = SENSOR_TYPE_RBR_CODA;
  new_sub->next = NULL;
  new_sub->rbr_coda_agg_period_ms = rbr_coda_agg_period_ms;
  new_sub->temp_deg_c.initBuffer(averager_max_samples);
  new_sub->pressure_deci_bar.initBuffer(averager_max_samples);
  new_sub->reading_count = 0;
  new_sub->configured_reading_period_ms = configured_reading_period_ms;
  return new_sub;
}

/**
 * @brief Retrieves the type of the sensor from the RBR Coda device.
 *
 * This function gets the RBR Coda type for the type of the sensor
 * currently in use. The sensor type is returned as an enumerated value
 * representing different possible sensor types. It gets the type from
 * the network configurations CBOR map.
 *
 * @return An enumeration representing the type of the sensor.
 * The specific values of the enumeration will depend on the types of sensors
 * supported by the RBR Coda device.
 */
BmRbrDataMsg::SensorType_t RbrCodaSensor::rbrCodaGetSensorType(uint64_t node_id) {
  BmRbrDataMsg::SensorType current_sensor_type = BmRbrDataMsg::SensorType::UNKNOWN;

  uint32_t network_crc32 = 0;
  uint32_t cbor_config_size = 0;
  uint8_t *network_config =
      report_builder_alloc_last_network_config(network_crc32, cbor_config_size);

  if (network_config != NULL) {
    CborParser parser;
    CborValue all_nodes_array, individual_node_array, node_array_member;
    char *app_name_str = NULL;
    char *key_str = NULL;
    do {
      if (cbor_parser_init(network_config, cbor_config_size, 0, &parser, &all_nodes_array) !=
          CborNoError) {
        printf("Failed to initialize the cbor parser\n");
        break;
      }
      if (!cbor_value_is_array(&all_nodes_array)) {
        printf("Failed to get the nodes array\n");
        break;
      }
      // Get the number of nodes
      size_t num_nodes;
      if (cbor_value_get_array_length(&all_nodes_array, &num_nodes) != CborNoError) {
        printf("Failed to get the number of nodes\n");
        break;
      }

      if (cbor_value_enter_container(&all_nodes_array, &individual_node_array) != CborNoError) {
        printf("Failed to enter the nodes array\n");
        break;
      }

      for (size_t node_idx = 0; node_idx < num_nodes; node_idx++) {
        if (!cbor_value_is_array(&individual_node_array)) {
          printf("Failed to get the node sub array\n");
          break;
        }
        // We know that there are 5 elements in the node sub array, so lets make sure there are!
        size_t num_elements;
        if (cbor_value_get_array_length(&individual_node_array, &num_elements) != CborNoError) {
          printf("Failed to get the number of elements in the node sub array\n");
          break;
        }
        // NUM_CONFIG_FIELDS_PER_NODE is defined in topology_sampler.h
        if (num_elements != NUM_CONFIG_FIELDS_PER_NODE) {
          printf("The number of elements in the node sub array is not 5\n");
          break;
        }

        if (cbor_value_enter_container(&individual_node_array, &node_array_member) !=
            CborNoError) {
          printf("Failed to enter the node sub array\n");
          break;
        }

        uint64_t extracted_node_id;
        if (cbor_value_get_uint64(&node_array_member, &extracted_node_id) != CborNoError) {
          printf("Failed to get the node id\n");
          break;
        }
        if (extracted_node_id != node_id) {
          // advance to the end of the individual node array before closing the individual node array
          // container and advancing to the next all node array member.
          if (cbor_value_advance(&node_array_member) != CborNoError) {
            printf("Failed to advance the node array member\n");
            break;
          }
          if (cbor_value_advance(&node_array_member) != CborNoError) {
            printf("Failed to advance the node array member\n");
            break;
          }
          if (cbor_value_advance(&node_array_member) != CborNoError) {
            printf("Failed to advance the node array member\n");
            break;
          }
          if (cbor_value_advance(&node_array_member) != CborNoError) {
            printf("Failed to advance the node array member\n");
            break;
          }
          if (cbor_value_advance(&node_array_member) != CborNoError) {
            printf("Failed to advance the node array member\n");
            break;
          }
          cbor_value_leave_container(&individual_node_array, &node_array_member);
          continue;
        } else {
          // Here we have found the RBR node we are looking for
          if (cbor_value_advance(&node_array_member) != CborNoError) {
            printf("Failed to advance the node array member\n");
            break;
          }
          if (!cbor_value_is_text_string(&node_array_member)) {
            printf("expected string but it is not a string\n");
            break;
          }
          size_t len;
          if (cbor_value_get_string_length(&node_array_member, &len) != CborNoError) {
            printf("Failed to get the length of the node app name\n");
            break;
          }
          app_name_str = static_cast<char *>(pvPortMalloc(len + 1));
          configASSERT(app_name_str);
          if (cbor_value_copy_text_string(&node_array_member, app_name_str, &len, NULL) !=
              CborNoError) {
            break;
          }
          app_name_str[len] = '\0';
          printf("RBR Node app name: %s\n", app_name_str);
          if (cbor_value_advance(&node_array_member) != CborNoError) {
            printf("Failed to advance the node array member\n");
            break;
          }
          if (!cbor_value_is_unsigned_integer(&node_array_member)) {
            printf("expected uint but it is not a string\n");
            break;
          }
          uint64_t gitSha;
          if (cbor_value_get_uint64(&node_array_member, &gitSha) != CborNoError) {
            printf("Failed to get the git sha\n");
            break;
          }
          printf("Git SHA: %" PRIx64 "\n", gitSha);
          if (cbor_value_advance(&node_array_member) != CborNoError) {
            printf("Failed to advance the node array member\n");
            break;
          }
          if (!cbor_value_is_unsigned_integer(&node_array_member)) {
            printf("expected uint but it is not a string\n");
            break;
          }
          uint64_t sys_confg_crc;
          if (cbor_value_get_uint64(&node_array_member, &sys_confg_crc) != CborNoError) {
            printf("Failed to get the git sha\n");
            break;
          }
          printf("sys_confg_crc: %" PRIx64 "\n", sys_confg_crc);
          if (cbor_value_advance(&node_array_member) != CborNoError) {
            printf("Failed to advance the node array member\n");
            break;
          }
          if (!cbor_value_is_map(&node_array_member)) {
            printf("expected map but it is not a map\n");
            break;
          }
          CborValue sys_cfg_map;
          if (cbor_value_enter_container(&node_array_member, &sys_cfg_map) != CborNoError) {
            printf("Failed to enter the sys_cfg_map\n");
            break;
          }
          // Loop through the map to find the rbrCodaType config
          while (!cbor_value_at_end(&node_array_member)) {
            if (!cbor_value_is_text_string(&sys_cfg_map)) {
              printf("expected string but it is not a string\n");
              break;
            }
            size_t len;
            if (cbor_value_get_string_length(&sys_cfg_map, &len) != CborNoError) {
              printf("Failed to get the length of the sys_cfg_map key\n");
              break;
            }
            key_str = static_cast<char *>(pvPortMalloc(len + 1));
            configASSERT(key_str);
            if (cbor_value_copy_text_string(&sys_cfg_map, key_str, &len, NULL) != CborNoError) {
              break;
            }
            key_str[len] = '\0';
            if (cbor_value_advance(&sys_cfg_map) != CborNoError) {
              printf("Failed to advance sys_cfg_map past string key\n");
              break;
            }
            if (strcmp(key_str, "rbrCodaType") == 0) {
              if (!cbor_value_is_unsigned_integer(&sys_cfg_map)) {
                printf("expected uint but it is not a string\n");
                break;
              }
              uint64_t sensor_type;
              if (cbor_value_get_uint64(&sys_cfg_map, &sensor_type) != CborNoError) {
                printf("Failed to get the sensor type\n");
                break;
              }
              current_sensor_type = static_cast<BmRbrDataMsg::SensorType_t>(sensor_type);
              printf("rbrCodaType: %" PRIu64 "\n", sensor_type);
              if (current_sensor_type == BmRbrDataMsg::SensorType::UNKNOWN) {
                printf("RBR type UKNOWN\n");
              } else if (current_sensor_type == BmRbrDataMsg::SensorType::TEMPERATURE) {
                printf("RBR type RBR.T\n");
              } else if (current_sensor_type == BmRbrDataMsg::SensorType::PRESSURE) {
                printf("RBR type RBR.D\n");
              } else if (current_sensor_type ==
                         BmRbrDataMsg::SensorType::PRESSURE_AND_TEMPERATURE) {
                printf("RBR type RBR.DT\n");
              } else {
                printf("Failed to get the sensor type, setting to UNKOWN\n");
              }
              break;
            }
            if (cbor_value_advance(&sys_cfg_map) != CborNoError) {
              printf("Failed to advance sys_cfg_map past a value\n");
              break;
            }
          }
          break;
        }
      }
    } while (0);
    if (app_name_str) {
      vPortFree(app_name_str);
    }
    if (key_str) {
      vPortFree(key_str);
    }
    vPortFree(network_config);
  }
  return current_sensor_type;
}
