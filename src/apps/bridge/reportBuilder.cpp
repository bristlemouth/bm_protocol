#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "task_priorities.h"
#include "timer_callback_handler.h"
#include "timers.h"
#include <string.h>
#include "aanderaaController.h"
#include "app_config.h"
#include "app_pub_sub.h"
#include "bm_serial.h"
#include "cbor_sensor_report_encoder.h"
#include "device_info.h"
#include "reportBuilder.h"

#define REPORT_BUILDER_QUEUE_SIZE (16)
#define TOPO_TIMEOUT_MS (1000)

typedef struct report_builder_element_s {
  uint64_t node_id;
  uint8_t sensor_type; // TODO - actually use it, it's ignored for now
  aanderaa_aggregations_t *sensor_data; // TODO - make generic
  report_builder_element_s *next;
  report_builder_element_s *prev;
} report_builder_element_t;

static aanderaa_aggregations_t NAN_AGG = {.abs_speed_mean_cm_s = NAN,
                                           .abs_speed_std_cm_s = NAN,
                                           .direction_circ_mean_rad = NAN,
                                           .direction_circ_std_rad = NAN,
                                           .temp_mean_deg_c = NAN};

class ReportBuilderLinkedList {
  private:

    report_builder_element_t* newElement(uint64_t node_id, uint8_t sensor_type, aanderaa_aggregations_t *sensor_data, uint32_t samples_per_report, uint32_t sample_counter);
    void freeElement(report_builder_element_s **element_pointer);
    void append(report_builder_element_t *curr, uint64_t node_id, uint8_t sensor_type, aanderaa_aggregations_t *sensor_data, uint32_t samples_per_report, uint32_t sample_counter);

    report_builder_element_t *head;
    size_t size;

  public:

    ReportBuilderLinkedList();
    ~ReportBuilderLinkedList();

    bool removeElement(report_builder_element_t *element);
    bool addSample(uint64_t node_id, uint8_t sensor_type, aanderaa_aggregations_t *sensor_data, uint32_t samples_per_report, uint32_t sample_counter);
    report_builder_element_t* findElement(uint64_t node_id);
    void clear();
    size_t getSize();
};

typedef struct ReportBuilderContext_s {
  uint32_t _sample_counter;
  uint32_t _samplesPerReport;
  uint32_t _transmitAggregations;
  ReportBuilderLinkedList _reportBuilderLinkedList;
  uint64_t _report_period_node_list[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
  uint32_t _report_period_num_nodes;
  uint32_t _report_period_max_network_crc32;
} ReportBuilderContext_t;

static ReportBuilderContext_t _ctx;
static QueueHandle_t _report_builder_queue = NULL;

static void report_builder_task(void *parameters);

ReportBuilderLinkedList::ReportBuilderLinkedList() {
  head = NULL;
  size = 0;
}

ReportBuilderLinkedList::~ReportBuilderLinkedList() {
  report_builder_element_t *element = head;
  while (element != NULL) {
    report_builder_element_t *next = element->next;
    freeElement(&element);
    element = next;
  }
}

report_builder_element_t* ReportBuilderLinkedList::newElement(uint64_t node_id, uint8_t sensor_type, aanderaa_aggregations_t *sensor_data, uint32_t samples_per_report, uint32_t sample_counter) {
  report_builder_element_t *element = static_cast<report_builder_element_t *>(pvPortMalloc(sizeof(report_builder_element_t)));
  configASSERT(element != NULL);
  element->node_id = node_id;
  element->sensor_type = sensor_type; // Ignored for now
  // TODO - use the sensor type to determine the size of the sensor data
  element->sensor_data = static_cast<aanderaa_aggregations_t *>(pvPortMalloc(samples_per_report * sizeof(aanderaa_aggregations_t)));
  configASSERT(element->sensor_data != NULL);
  memset(element->sensor_data, 0, samples_per_report * sizeof(aanderaa_aggregations_t));

  // Back fill the sensor_data with NANs if we are not on sample_count 0.
  uint32_t i;
  for (i = 0; i < sample_counter; i++) {
    memcpy(&element->sensor_data[i], &NAN_AGG, sizeof(aanderaa_aggregations_t));
  }
  // Copy the sensor data into the elements array in the correct location within the buffer
  if (sensor_data != NULL) {
    memcpy(&element->sensor_data[i], sensor_data, sizeof(aanderaa_aggregations_t));
  } else {
    memcpy(&element->sensor_data[i], &NAN_AGG, sizeof(aanderaa_aggregations_t));
  }
  element->next = NULL;
  element->prev = NULL;
  return element;
}

void ReportBuilderLinkedList::freeElement(report_builder_element_t **element_pointer) {
  if (element_pointer != NULL && *element_pointer != NULL) {
    vPortFree((*element_pointer)->sensor_data); // Free the data first
    vPortFree(*element_pointer);
    *element_pointer = NULL;
  }
}

bool ReportBuilderLinkedList::removeElement(report_builder_element_t *element) {
  bool rval = false;
  if (element != NULL) {
    if (element->prev != NULL) {
      element->prev->next = element->next;
    } else {
      head = element->next;
    }
    if (element->next != NULL) {
      element->next->prev = element->prev;
    }
    freeElement(&element);
    size--;
    rval = true;
  }
  return rval;
}

bool ReportBuilderLinkedList::addSample(uint64_t node_id, uint8_t sensor_type, aanderaa_aggregations_t *sensor_data, uint32_t samples_per_report, uint32_t sample_counter) {
  bool rval = false;
  report_builder_element_t *element = findElement(node_id);
  if (element != NULL) {
    // Back fill the sensor_data with NANs if we are not on the right sample count
    uint32_t i;
    for (i = 0; i < sample_counter; i++) {
      memcpy(&element->sensor_data[i], &NAN_AGG, sizeof(aanderaa_aggregations_t));
    }
    // Copy the sensor data into the elements array in the correct location within the buffer
    // If it is NULL then just fill it with NAN again
    if (sensor_data != NULL) {
      memcpy(&element->sensor_data[i], sensor_data, sizeof(aanderaa_aggregations_t));
    } else {
      memcpy(&element->sensor_data[i], &NAN_AGG, sizeof(aanderaa_aggregations_t));
    }
  } else {
    if (head == NULL && size == 0) {
      append(NULL, node_id, sensor_type, sensor_data, samples_per_report, sample_counter);
      rval = true;
    } else {
      report_builder_element_t *element = head;
      while (element->next != NULL) {
        element = element->next;
      }
      append(element, node_id, sensor_type, sensor_data, samples_per_report, sample_counter);
    }
  }
  return rval;
}

void ReportBuilderLinkedList::append(report_builder_element_t *curr, uint64_t node_id, uint8_t sensor_type, aanderaa_aggregations_t *sensor_data, uint32_t samples_per_report, uint32_t sample_counter) {
   report_builder_element_s *element = newElement(node_id, sensor_type, sensor_data, samples_per_report, sample_counter);
  if (curr != NULL) {
    element->next = curr->next;
    element->prev = curr;
    if (curr->next != NULL) {
      curr->next->prev = element;
    }
    curr->next = element;
    size++;
  } else {
    head = element;
  }
}

report_builder_element_t* ReportBuilderLinkedList::findElement(uint64_t node_id) {
  report_builder_element_t *element = head;
  while (element != NULL) {
    if (element->node_id == node_id) {
      break;
    }
    element = element->next;
  }
  return element;
}

void ReportBuilderLinkedList::clear() {
  report_builder_element_t *element = head;
  while (element != NULL) {
    report_builder_element_t *next = element->next;
    freeElement(&element);
    element = next;
  }
  head = NULL;
  size = 0;
}

size_t ReportBuilderLinkedList::getSize() {
  return size;
}

CborError encode_double_sample_member(CborEncoder &sample_array, void* sample_member){
  double local_sample_member = *(double*)sample_member;
  CborError err = CborNoError;
  err = cbor_encode_double(&sample_array, local_sample_member);
  return err;
}

void reportBuilderAddToQueue(uint64_t node_id, uint8_t sensor_type, aanderaa_aggregations_t *sensor_data, report_builder_message_e msg_type) {
  report_builder_queue_item_t item;
  item.message_type = msg_type;
  if (msg_type == REPORT_BUILDER_SAMPLE_MESSAGE) {
    item.node_id = node_id;
    item.sensor_type = sensor_type;
    item.sensor_data = sensor_data;
  } else if (msg_type == REPORT_BUILDER_INCREMENT_SAMPLE_COUNT || msg_type == REPORT_BUILDER_CHECK_CRC) {
    item.node_id = 0;
    item.sensor_type = 0;
    item.sensor_data = NULL;
  } else {
    configASSERT(0);
  }
  xQueueSend(_report_builder_queue, &item, portMAX_DELAY);
}

// Task init
void reportBuilderInit(cfg::Configuration* sys_cfg) {
  configASSERT(sys_cfg);
  _ctx._samplesPerReport = DEFAULT_SAMPLES_PER_REPORT;
  sys_cfg->getConfig(AppConfig::SAMPLES_PER_REPORT, strlen(AppConfig::SAMPLES_PER_REPORT), _ctx._samplesPerReport);
  _ctx._transmitAggregations = DEFAULT_TRANSMIT_AGGREGATIONS;
  sys_cfg->getConfig(AppConfig::TRANSMIT_AGGREGATIONS, strlen(AppConfig::TRANSMIT_AGGREGATIONS), _ctx._transmitAggregations);
  _ctx._sample_counter = 0;
  _report_builder_queue =
      xQueueCreate(REPORT_BUILDER_QUEUE_SIZE, sizeof(report_builder_queue_item_t));
  configASSERT(_report_builder_queue);
  // create task
  BaseType_t rval = xTaskCreate(report_builder_task, "REPORT_BUILDER", 1024, NULL,
                                REPORT_BUILDER_TASK_PRIORITY, NULL);
  configASSERT(rval == pdPASS);
}

// Task
static void report_builder_task(void *parameters) {
  (void) parameters;

  report_builder_queue_item_t item;

  while (1) {
    if (xQueueReceive(_report_builder_queue, &item,
                      portMAX_DELAY) == pdPASS) {
      switch (item.message_type) {
        case REPORT_BUILDER_INCREMENT_SAMPLE_COUNT: {
          _ctx._sample_counter++;
          printf("Incrementing sample counter to %d\n", _ctx._sample_counter);
          if (_ctx._sample_counter >= _ctx._samplesPerReport) {
            if (_ctx._samplesPerReport > 0 && _ctx._transmitAggregations) {
              // Need to have more than one node to report anything (1 node would be just the bridge)
              if (_ctx._report_period_num_nodes > 1) {
                uint32_t num_sensors = _ctx._report_period_num_nodes - 1; // Minus one since topo includes the bridge
                sensor_report_encoder_context_t context;
                uint8_t cbor_buffer[315];
                sensor_report_encoder_open_report(cbor_buffer, sizeof(cbor_buffer), num_sensors, context);
                // Start at index 1 to skip the bridge
                for (size_t i = 1; i < _ctx._report_period_num_nodes; i++) {
                  report_builder_element_t *element = _ctx._reportBuilderLinkedList.findElement(_ctx._report_period_node_list[i]);
                  if (element == NULL) {
                    printf("No data for node %" PRIx64 " in report period, adding it to the list\n", _ctx._report_period_node_list[i]);
                    // we have a sensor that may have been added at the end of the report period
                    // so lets add it to the list and this wil backfill all of the data so we can still send it
                    // in the report. We will need to pass (_ctx._sample_counter - 1) to make sure we don't overflow the sensor_data buffer.
                    // Also we will pass NULL into the sensor_data since we don't have any data for it yet and we will fill the whole thing with NANs
                    _ctx._reportBuilderLinkedList.addSample(_ctx._report_period_node_list[i], 0, NULL, _ctx._samplesPerReport, (_ctx._sample_counter - 1));
                  } else {
                    printf("Found data for node %" PRIx64 " adding it the the report\n", _ctx._report_period_node_list[i]);
                  }
                  sensor_report_encoder_open_sensor(context, _ctx._samplesPerReport);
                  for (uint32_t j = 0; j < _ctx._samplesPerReport; j++) {
                    sensor_report_encoder_open_sample(context, AANDERAA_NUM_SAMPLE_MEMBERS);

                    sensor_report_encoder_add_sample_member(context, encode_double_sample_member, &element->sensor_data[j].abs_speed_mean_cm_s);
                    sensor_report_encoder_add_sample_member(context, encode_double_sample_member, &element->sensor_data[j].abs_speed_std_cm_s);
                    sensor_report_encoder_add_sample_member(context, encode_double_sample_member, &element->sensor_data[j].direction_circ_mean_rad);
                    sensor_report_encoder_add_sample_member(context, encode_double_sample_member, &element->sensor_data[j].direction_circ_std_rad);
                    sensor_report_encoder_add_sample_member(context, encode_double_sample_member, &element->sensor_data[j].temp_mean_deg_c);

                    sensor_report_encoder_close_sample(context);
                  }
                  sensor_report_encoder_close_sensor(context);
                }
                sensor_report_encoder_close_report(context);
                size_t cbor_buffer_len = sensor_report_encoder_get_report_size_bytes(context);
                app_pub_sub_bm_bridge_sensor_report_data_t *message_buff = static_cast<app_pub_sub_bm_bridge_sensor_report_data_t *>(pvPortMalloc(sizeof(uint32_t) + sizeof(size_t) + cbor_buffer_len));
                configASSERT(message_buff != NULL);
                message_buff->bm_config_crc32 = _ctx._report_period_max_network_crc32;
                message_buff->cbor_buffer_len = cbor_buffer_len;
                memcpy(&message_buff->cbor_buffer, cbor_buffer, cbor_buffer_len);
                // Send cbor_buffer to the other side.
                bm_serial_pub(getNodeId(),APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_TOPIC,
                    sizeof(APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_TOPIC),
                    reinterpret_cast<uint8_t *>(message_buff),
                    sizeof(uint32_t) + sizeof(size_t) + cbor_buffer_len,
                    APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_TYPE,
                    APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_VERSION);
                vPortFree(message_buff);
              } else {
                printf("No nodes to send data for\n");
              }
              printf("Clearing the list\n");
              _ctx._reportBuilderLinkedList.clear();
            }
            printf("Clearing the sample counter\n");
            _ctx._sample_counter = 0;
            // Clear the report periods network associated data to rebuild it for the next report period
            _ctx._report_period_num_nodes = 0;
            _ctx._report_period_max_network_crc32 = 0;
            memset(_ctx._report_period_node_list, 0, sizeof(_ctx._report_period_node_list));
          }
          break;
        }

        case REPORT_BUILDER_SAMPLE_MESSAGE: {
          if (_ctx._samplesPerReport > 0) {
            printf("Adding sample for %" PRIx64 " to list\n", item.node_id);
            _ctx._reportBuilderLinkedList.addSample(item.node_id, item.sensor_type, item.sensor_data, _ctx._samplesPerReport, _ctx._sample_counter);
          } else {
            printf("samplesPerReport is 0, not adding sample to list\n");
          }
          break;
        }

        case REPORT_BUILDER_CHECK_CRC: {

          // IMPORTANT: If the bus is constantly on, the currentAggPeriodMin must be longer than the topology sampler period
          // otherwise the CRC will not update before the topology sampler sends its next check CRC.

          printf("Checking CRC in report builder!\n");
          // Get the latest crc - if it doens't match our saved one, pull the latest topology and compare the topology
          // to our current topology - if it doesn't match our current one (and mainly if it is bigger or the same size) then
          // update the crc and the topology list
          uint32_t temp_network_crc32 = 0;
          uint32_t temp_cbor_config_size = 0;
          uint8_t *last_network_config = topology_sampler_alloc_last_network_config(temp_network_crc32, temp_cbor_config_size);
          if (last_network_config != NULL) {
            printf("Got CRC %" PRIx32 " OLD CRC %" PRIx32 "\n", temp_network_crc32, _ctx._report_period_max_network_crc32);
            if (temp_network_crc32 != _ctx._report_period_max_network_crc32) {
              printf("Getting topology in report builder!\n");
              uint64_t temp_node_list[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
              size_t temp_node_list_size = sizeof(temp_node_list);
              uint32_t temp_num_nodes;
              if (topology_sampler_get_node_list(temp_node_list, temp_node_list_size, temp_num_nodes, TOPO_TIMEOUT_MS)) {
                printf("Got topology in report builder!\n");
                if (temp_num_nodes >= _ctx._report_period_num_nodes) {
                  printf("Updating CRC and topology in report builder!\n");
                  _ctx._report_period_num_nodes = temp_num_nodes;
                  memcpy(_ctx._report_period_node_list, temp_node_list, sizeof(temp_node_list));
                  _ctx._report_period_max_network_crc32 = temp_network_crc32;
                } else {
                  printf("Not updating CRC and topology in report builder, new topology is smaller than the old one!\n");
                }
              } else {
                printf("Failed to get the node list in report builder!\n");
              }
            } else {
              printf("CRCs match, not updating\n");
            }
            // The last network config is not used here, but in the future it can be used to determine if varying sensor types have been
            // connected/disconnected to the network.
            vPortFree(last_network_config);
          }
          break;
        }

        default: {
          printf("Uknown message type received in report_builder_task\n");
          break;
        }
      }
    }
  }
}
