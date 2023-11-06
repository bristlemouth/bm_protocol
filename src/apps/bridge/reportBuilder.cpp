#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "task_priorities.h"
#include "timer_callback_handler.h"
#include "timers.h"
#include <string.h>
#include "app_config.h"
#include "aanderaaController.h"
#include "reportBuilder.h"

#define REPORT_BUILDER_QUEUE_SIZE (16)

typedef struct {
  uint32_t _sample_counter;
  uint32_t _samplesPerReport;
  uint32_t _transmitAggregations;
  // TODO add a linked list of samples from nodes
} ReportBuilderContext_t;

static ReportBuilderContext_t _ctx;
static QueueHandle_t _report_builder_queue = NULL;

static void report_builder_task(void *parameters);

typedef struct report_builder_element_s {
  uint64_t node_id;
  uint8_t sensor_type; // TODO - actually use it, it's ignored for now
  aanderaa_aggregations_t *sensor_data; // TODO - make generic
  report_builder_element_s *next;
  report_builder_element_s *prev;
} report_builder_element_t;

class ReportBuilderLinkedList {
  private:


    report_builder_element_t* newElement(uint64_t node_id, uint8_t sensor_type, uint8_t *sensor_data, uint32_t samples_per_report, uint32_t sample_counter);
    void freeElement(report_builder_element_s **element_pointer);

    report_builder_element_t *head;
    report_builder_element_t *tail; // TODO - is this needed?
    size_t size;

  public:

    ReportBuilderLinkedList();
    ~ReportBuilderLinkedList();

    bool removeElement(report_builder_element_t *element);
    void insertAfter(report_builder_element_t *curr, uint64_t node_id, uint8_t sensor_type, uint8_t *sensor_data);
    void insertBefore(report_builder_element_t *curr, uint64_t node_id, uint8_t sensor_type, uint8_t *sensor_data);
    void clear();
    size_t getSize();
};

ReportBuilderLinkedList::ReportBuilderLinkedList() {
  head = NULL;
  tail = NULL;
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

report_builder_element_t* ReportBuilderLinkedList::newElement(uint64_t node_id, uint8_t sensor_type, uint8_t *sensor_data, uint32_t samples_per_report, uint32_t sample_counter) {
  report_builder_element_t *element = static_cast<report_builder_element_t *>(pvPortMalloc(sizeof(report_builder_element_t)));
  configASSERT(element != NULL);
  element->node_id = node_id;
  element->sensor_type = sensor_type; // Ignored for now
  // TODO - use the sensor type to determine the size of the sensor data
  element->sensor_data = static_cast<aanderaa_aggregations_t *>(pvPortMalloc(samples_per_report * sizeof(aanderaa_aggregations_t)));
  configASSERT(element->sensor_data != NULL);

  aanderaa_aggregations_t nan_agg = {.abs_speed_mean_cm_s = NAN,
                                     .abs_speed_std_cm_s = NAN,
                                     .direction_circ_mean_rad = NAN,
                                     .direction_circ_std_rad = NAN,
                                     .temp_mean_deg_c = NAN};
  // Back fill the sensor_data with NANs if we are not on sample_count 0.
  uint32_t i;
  for (i = 0; i < sample_counter; i++) {
    memcpy(&element->sensor_data[i], &nan_agg, sizeof(aanderaa_aggregations_t));
  }
  // Copy the sensor data into the elements array in the correct location within the buffer
  memcpy(&element->sensor_data[i], &sensor_data, sizeof(aanderaa_aggregations_t));
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
    } else {
      tail = element->prev;
    }
    freeElement(&element);
    size--;
    rval = true;
  }
  return rval;
}

void ReportBuilderLinkedList::insertAfter(report_builder_element_t *curr, uint64_t node_id, uint8_t sensor_type, uint8_t *sensor_data) {
   report_builder_element_s *element = newElement(node_id, sensor_type, sensor_data);
  if (curr != NULL) {
    element->next = curr->next;
    element->prev = curr;
    if (curr->next != NULL) {
      curr->next->prev = element;
    } else {
      tail = element;
    }
    curr->next = element;
    size++;
  } else {
    head = element;
    tail = element;
  }
}

void ReportBuilderLinkedList::insertBefore(report_builder_element_t *curr, uint64_t node_id, uint8_t sensor_type, uint8_t *sensor_data) {
  report_builder_element_t *element = newElement(node_id, sensor_type, sensor_data);
  if (curr != NULL) {
    element->next = curr;
    element->prev = curr->prev;
    if (curr->prev != NULL) {
      curr->prev->next = element;
    } else {
      head = element;
    }
    curr->prev = element;
    size++;
  } else {
    head = element;
    tail = element;
  }
}

void ReportBuilderLinkedList::clear() {
  report_builder_element_t *element = head;
  while (element != NULL) {
    report_builder_element_t *next = element->next;
    freeElement(&element);
    element = next;
  }
  head = NULL;
  tail = NULL;
  size = 0;
}

// Task init
void reportBuilderInit(cfg::Configuration* sys_cfg) {
  configASSERT(sys_cfg);
  _ctx._samplesPerReport = 0;
  sys_cfg->getConfig(AppConfig::SAMPLES_PER_REPORT, strlen(AppConfig::SAMPLES_PER_REPORT), _ctx._samplesPerReport);
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
          if (_ctx._sample_counter >= _ctx._samplesPerReport && _ctx._samplesPerReport > 0) {
            if (_ctx._transmitAggregations) {
              // TODO - compile report into Cbor and send it to spotter
            } else {
              // TODO - is there something we should do with the report if we don't want to send it?
              // like just log it?
            }
            // empty the linked list - I think that this will be easiest and leave us in a clean state instead of
            // having to deal with a linked list that has a bunch of old nodes in it.
          } else {
            // TODO - is there something we should do with the report if we don't want to send it?
            // like just log it? Is there a way to combine these two nested if else statements?
          }
          break;
        case REPORT_BUILDER_SAMPLE_MESSAGE:
          // TODO - add sample to the linked list
          break;
      }
    }
  }
}
