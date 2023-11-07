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
  uint64_t _node_list[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
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
  memcpy(&element->sensor_data[i], sensor_data, sizeof(aanderaa_aggregations_t));
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
    memcpy(&element->sensor_data[i], sensor_data, sizeof(aanderaa_aggregations_t));
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
  } else if (msg_type == REPORT_BUILDER_INCREMENT_SAMPLE_COUNT) {
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
        case REPORT_BUILDER_INCREMENT_SAMPLE_COUNT:
          _ctx._sample_counter++;
          printf("Incrementing sample counter to %d\n", _ctx._sample_counter);
          if (_ctx._sample_counter >= _ctx._samplesPerReport) {
            if (_ctx._samplesPerReport > 0 && _ctx._transmitAggregations) {
              size_t size_list = sizeof(_ctx._node_list);
              uint32_t num_nodes = 0;
              // TODO - here we need to also pull the Last Full Configuration
              // and make sure we are only adding sensors that are present in
              // the LFC and in the topology.
              if (topology_sampler_get_node_list(_ctx._node_list, size_list, num_nodes,
                                                TOPO_TIMEOUT_MS)) {
                printf("Got node list, size: %d\n", num_nodes);
                sensor_report_encoder_context_t context;
                uint8_t cbor_buffer[1024]; // TODO - make this dynamic?
                sensor_report_encoder_open_report(cbor_buffer, sizeof(cbor_buffer), num_nodes, context);
                for (size_t i = 0; i < num_nodes; i++) {
                  report_builder_element_t *element = _ctx._reportBuilderLinkedList.findElement(_ctx._node_list[i]);
                  sensor_report_encoder_open_sensor(context, _ctx._samplesPerReport);
                  for (uint32_t j = 0; j < _ctx._samplesPerReport; j++) {
                    sensor_report_encoder_open_sample(context, 5); // 5 is the number of sample_members for an aanderaa sample

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
                // Send cbor_buffer to the other side.
                bm_serial_pub(getNodeId(),APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_TOPIC,
                    sizeof(APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_TOPIC),
                    cbor_buffer,
                    cbor_buffer_len,
                    APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_TYPE,
                    APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_VERSION);
              } else {
                printf("Failed to get node list\n");
              }
              printf("Clearing the list\n");
              _ctx._reportBuilderLinkedList.clear();
            }
            printf("Clearing the sample counter\n");
            _ctx._sample_counter = 0;
          }
          break;
        case REPORT_BUILDER_SAMPLE_MESSAGE:
          if (_ctx._samplesPerReport > 0) {
            printf("Adding sample for %" PRIx64 " to list\n", item.node_id);
            _ctx._reportBuilderLinkedList.addSample(item.node_id, item.sensor_type, item.sensor_data, _ctx._samplesPerReport, _ctx._sample_counter);
          } else {
            printf("samplesPerReport is 0, not adding sample to list\n");
          }
          break;
      }
    }
  }
}
