#include "reportBuilder.h"
#include "FreeRTOS.h"
#include "aanderaaSensor.h"
#include "app_config.h"
#include "app_pub_sub.h"
#include "bm_serial.h"
#include "bridgeLog.h"
#include "cbor_sensor_report_encoder.h"
#include "device_info.h"
#include "queue.h"
#include "rbrCodaSensor.h"
#include "seapointTurbiditySensor.h"
#include "semphr.h"
#include "sensorController.h"
#include "softSensor.h"
#include "task.h"
#include "task_priorities.h"
#include "timer_callback_handler.h"
#include "timers.h"
#include "topology_sampler.h"
#include <string.h>

#define REPORT_BUILDER_QUEUE_SIZE (16)
#define TOPO_TIMEOUT_MS (1000)
#define NETWORK_CONFIG_MUTEX_TIMEOUT_MS (1000)

/*
  This is the current max length the cbor buffer can be for the sensor report
  The limiting factor is the max size we can send via Iridium minus the size
  of the messages header. The math for this is:

  340 bytes - ~25 bytes(header) = 315 bytes (bitpacked)

  Each sample bitpacked is ~12 bytes so we can fit 26 samples in a message

  26 * (sizeof(aanderaa_aggregations_t)) = 26 * (7 * sizeof(double) + sizeof(uint32_t)) =
  26 * (7 * 8 + 4) = 26 * 60 = 1560 bytes

  Then we need to add some overhead for the cbor array, lets just use 128 bytes.

  1560 + 128 = 1688 bytes
*/
#define MAX_SENSOR_REPORT_CBOR_LEN 1688

typedef struct report_builder_element_s {
  uint64_t node_id;
  uint8_t sensor_type;
  void *sensor_data;
  uint32_t sample_counter;
  report_builder_element_s *next;
  report_builder_element_s *prev;
} report_builder_element_t;

static aanderaa_aggregations_t NAN_AGG = {.abs_speed_mean_cm_s = NAN,
                                          .abs_speed_std_cm_s = NAN,
                                          .direction_circ_mean_rad = NAN,
                                          .direction_circ_std_rad = NAN,
                                          .temp_mean_deg_c = NAN,
                                          .abs_tilt_mean_rad = NAN,
                                          .std_tilt_mean_rad = NAN,
                                          .reading_count = 0};

static soft_aggregations_t SOFT_NAN_AGG = {.temp_mean_deg_c = NAN, .reading_count = 0};

static rbr_coda_aggregations_t RBR_CODA_NAN_AGG = {.temp_mean_deg_c = NAN,
                                                   .pressure_mean_deci_bar = NAN,
                                                   .pressure_stdev_deci_bar = NAN,
                                                   .reading_count = 0,
                                                   .sensor_type =
                                                       BmRbrDataMsg::SensorType::UNKNOWN};

static seapoint_turbidity_aggregations_t seapoint_turbidity_NAN_AGG = {
    .turbidity_s_mean_ftu = NAN, .turbidity_r_mean_ftu = NAN, .reading_count = 0};

class ReportBuilderLinkedList {
private:
  report_builder_element_t *newElement(uint64_t node_id, uint8_t sensor_type, void *sensor_data,
                                       uint32_t sensor_data_size, uint32_t samples_per_report,
                                       uint32_t sample_counter);
  void freeElement(report_builder_element_s **element_pointer);
  void append(report_builder_element_t *curr, uint64_t node_id, uint8_t sensor_type,
              void *sensor_data, uint32_t sensor_data_size, uint32_t samples_per_report,
              uint32_t sample_counter);
  void addSampleToElement(report_builder_element_t *element, uint8_t sensor_type,
                          void *sensor_data, uint32_t sample_counter);

  report_builder_element_t *head;
  size_t size;

public:
  ReportBuilderLinkedList();
  ~ReportBuilderLinkedList();

  void removeElement(report_builder_element_t *element);
  void findElementAndAddSampleToElement(uint64_t node_id, uint8_t sensor_type,
                                        void *sensor_data, uint32_t sensor_data_size,
                                        uint32_t samples_per_report, uint32_t sample_counter);
  report_builder_element_t *findElement(uint64_t node_id);
  void clear();
};

typedef struct ReportBuilderContext_s {
  uint32_t _sample_counter;
  uint32_t _samplesPerReport;
  uint32_t _transmitAggregations;
  ReportBuilderLinkedList _reportBuilderLinkedList;
  uint64_t _report_period_node_list[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
  abstractSensorType_e _report_period_sensor_type_list[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
  uint8_t *report_period_max_network_config_cbor;
  uint32_t report_period_max_network_config_cbor_len;
  SemaphoreHandle_t _config_mutex;
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

ReportBuilderLinkedList::~ReportBuilderLinkedList() { clear(); }

/**
 * @brief Creates a new element for the report builder linked list.
 *
 * This function allocates memory for a new report builder element and initializes its properties. It also allocates memory for the sensor data based on the number of samples per report and the size of the sensor data.
 *
 * @param node_id The ID of the node for the report.
 * @param sensor_type The type of the sensor for the report.
 * @param sensor_data Pointer to the sensor data for the report.
 * @param sensor_data_size The size of the sensor data.
 * @param samples_per_report The number of samples per report.
 * @param sample_counter The current sample counter.
 * @return A pointer to the newly created report builder element.
 *
 */
report_builder_element_t *
ReportBuilderLinkedList::newElement(uint64_t node_id, uint8_t sensor_type, void *sensor_data,
                                    uint32_t sensor_data_size, uint32_t samples_per_report,
                                    uint32_t sample_counter) {
  report_builder_element_t *element =
      static_cast<report_builder_element_t *>(pvPortMalloc(sizeof(report_builder_element_t)));
  configASSERT(element != NULL);
  element->node_id = node_id;
  element->sensor_type = sensor_type;
  element->sample_counter = 0;
  element->sensor_data =
      static_cast<void *>(pvPortMalloc(samples_per_report * sensor_data_size));
  configASSERT(element->sensor_data != NULL);
  memset(element->sensor_data, 0, samples_per_report * sensor_data_size);

  addSampleToElement(element, sensor_type, sensor_data, sample_counter);

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

void ReportBuilderLinkedList::removeElement(report_builder_element_t *element) {
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
  }
}

/**
 * @brief Adds sensor data to a specific element in the report builder linked list.
 *
 * This function takes a pointer to an element in the linked list and adds the sensor data to that element. The sensor data is added at the position specified by the sample counter.
 *
 * @param element Pointer to the element in the linked list.
 * @param sensor_type The type of the sensor for the report.
 * @param sensor_data Pointer to the sensor data for the report.
 * @param sample_counter The current sample counter, indicating the position at which to add the sensor data.
 */
void ReportBuilderLinkedList::addSampleToElement(report_builder_element_t *element,
                                                 uint8_t sensor_type, void *sensor_data,
                                                 uint32_t sample_counter) {
  switch (sensor_type) {
  case SENSOR_TYPE_AANDERAA: {
    if (element->sample_counter < sample_counter) {
      // Back fill the sensor_data with NANs if we are not on the right sample counter
      // We use the element->sample_counter to track within each element how many samples
      // the element has received.
      for (; element->sample_counter < sample_counter; element->sample_counter++) {
        memcpy(&(static_cast<aanderaa_aggregations_t *>(
                   element->sensor_data))[element->sample_counter],
               &NAN_AGG, sizeof(aanderaa_aggregations_t));
      }
    }

    // Copy the sensor data into the elements array in the correct location within the buffer
    // If it is NULL then just fill it with NAN again
    if (sensor_data != NULL) {
      memcpy(&(static_cast<aanderaa_aggregations_t *>(
                 element->sensor_data))[element->sample_counter],
             sensor_data, sizeof(aanderaa_aggregations_t));
    } else {
      memcpy(&(static_cast<aanderaa_aggregations_t *>(
                 element->sensor_data))[element->sample_counter],
             &NAN_AGG, sizeof(aanderaa_aggregations_t));
    }
    element->sample_counter++;
    break;
  }
  case SENSOR_TYPE_SOFT: {
    if (element->sample_counter < sample_counter) {
      // Back fill the sensor_data with NANs if we are not on the right sample counter
      // We use the element->sample_counter to track within each element how many samples
      // the element has received.
      for (; element->sample_counter < sample_counter; element->sample_counter++) {
        memcpy(&(static_cast<soft_aggregations_t *>(
                   element->sensor_data))[element->sample_counter],
               &SOFT_NAN_AGG, sizeof(soft_aggregations_t));
      }
    }
    if (sensor_data != NULL) {
      memcpy(
          &(static_cast<soft_aggregations_t *>(element->sensor_data))[element->sample_counter],
          sensor_data, sizeof(soft_aggregations_t));
    } else {
      memcpy(
          &(static_cast<soft_aggregations_t *>(element->sensor_data))[element->sample_counter],
          &SOFT_NAN_AGG, sizeof(soft_aggregations_t));
    }
    element->sample_counter++;
    break;
  }
  case SENSOR_TYPE_RBR_CODA: {
    if (element->sample_counter < sample_counter) {
      // Back fill the sensor_data with NANs if we are not on the right sample counter
      // We use the element->sample_counter to track within each element how many samples
      // the element has received.
      for (; element->sample_counter < sample_counter; element->sample_counter++) {
        memcpy(&(static_cast<rbr_coda_aggregations_t *>(
                   element->sensor_data))[element->sample_counter],
               &RBR_CODA_NAN_AGG, sizeof(rbr_coda_aggregations_t));
      }
    }
    if (sensor_data != NULL) {
      memcpy(&(static_cast<rbr_coda_aggregations_t *>(
                 element->sensor_data))[element->sample_counter],
             sensor_data, sizeof(rbr_coda_aggregations_t));
    } else {
      memcpy(&(static_cast<rbr_coda_aggregations_t *>(
                 element->sensor_data))[element->sample_counter],
             &RBR_CODA_NAN_AGG, sizeof(rbr_coda_aggregations_t));
    }
    element->sample_counter++;
    break;
  }
  case SENSOR_TYPE_SEAPOINT_TURBIDITY: {
    if (element->sample_counter < sample_counter) {
      // Back fill the sensor_data with NANs if we are not on the right sample counter
      // We use the element->sample_counter to track within each element how many samples
      // the element has received.
      for (; element->sample_counter < sample_counter; element->sample_counter++) {
        memcpy(&(static_cast<seapoint_turbidity_aggregations_t *>(
                   element->sensor_data))[element->sample_counter],
               &seapoint_turbidity_NAN_AGG, sizeof(seapoint_turbidity_aggregations_t));
      }
    }
    if (sensor_data != NULL) {
      memcpy(&(static_cast<seapoint_turbidity_aggregations_t *>(
                 element->sensor_data))[element->sample_counter],
             sensor_data, sizeof(seapoint_turbidity_aggregations_t));
    } else {
      memcpy(&(static_cast<seapoint_turbidity_aggregations_t *>(
                 element->sensor_data))[element->sample_counter],
             &seapoint_turbidity_NAN_AGG, sizeof(seapoint_turbidity_aggregations_t));
    }
    element->sample_counter++;
    break;
  }
  default: {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Unknown sensor type in addSampleToElement\n");
    configASSERT(0);
    break;
  }
  }
}

/**
 * @brief Adds a sample to a specific element in the report builder linked list.
 *
 * This function finds the element associated with the given node_id in the linked list. If the element is found, it adds the sensor data to the element. If the element is not found, it creates a new element and appends it to the linked list.
 *
 * @param node_id The ID of the node for the report.
 * @param sensor_type The type of the sensor for the report.
 * @param sensor_data Pointer to the sensor data for the report.
 * @param sensor_data_size The size of the sensor data.
 * @param samples_per_report The number of samples per report.
 * @param sample_counter The current sample counter.
 * @return A boolean value indicating whether the operation was successful. Returns true if the sample was added successfully, false otherwise.
 */
void ReportBuilderLinkedList::findElementAndAddSampleToElement(
    uint64_t node_id, uint8_t sensor_type, void *sensor_data, uint32_t sensor_data_size,
    uint32_t samples_per_report, uint32_t sample_counter) {
  report_builder_element_t *element = findElement(node_id);
  if (element != NULL) {
    addSampleToElement(element, sensor_type, sensor_data, sample_counter);
  } else {
    if (head == NULL && size == 0) {
      append(NULL, node_id, sensor_type, sensor_data, sensor_data_size, samples_per_report,
             sample_counter);
    } else {
      report_builder_element_t *element = head;
      while (element->next != NULL) {
        element = element->next;
      }
      append(element, node_id, sensor_type, sensor_data, sensor_data_size, samples_per_report,
             sample_counter);
    }
  }
}

void ReportBuilderLinkedList::append(report_builder_element_t *curr, uint64_t node_id,
                                     uint8_t sensor_type, void *sensor_data,
                                     uint32_t sensor_data_size, uint32_t samples_per_report,
                                     uint32_t sample_counter) {
  report_builder_element_s *element = newElement(
      node_id, sensor_type, sensor_data, sensor_data_size, samples_per_report, sample_counter);
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

report_builder_element_t *ReportBuilderLinkedList::findElement(uint64_t node_id) {
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

CborError encode_double_sample_member(CborEncoder &sample_array, void *sample_member) {
  double local_sample_member = *(double *)sample_member;
  CborError err = CborNoError;
  err = cbor_encode_double(&sample_array, local_sample_member);
  return err;
}

CborError encode_uint_sample_member(CborEncoder &sample_array, void *sample_member) {
  uint32_t local_sample_member = *(uint32_t *)sample_member;
  CborError err = CborNoError;
  err = cbor_encode_uint(&sample_array, local_sample_member);
  return err;
}

/**
 * @brief Adds a new item to the report builder queue.
 *
 * This function creates a new report builder queue item and adds it to the queue. The item's properties are set based on the message type.
 *
 * @param node_id The ID of the node for the report.
 * @param sensor_type The type of the sensor for the report.
 * @param sensor_data Pointer to the sensor data for the report.
 * @param sensor_data_size The size of the sensor data.
 * @param msg_type The type of the message to be added to the queue.
 *
 * @note If the message type is REPORT_BUILDER_SAMPLE_MESSAGE, the node_id, sensor_type, sensor_data, and sensor_data_size parameters are used.
 * If the message type is REPORT_BUILDER_INCREMENT_SAMPLE_COUNT or REPORT_BUILDER_CHECK_CRC, these parameters are ignored and default values are used instead
 * since they are not needed for these message types.
 */
void reportBuilderAddToQueue(uint64_t node_id, uint8_t sensor_type, void *sensor_data,
                             uint32_t sensor_data_size, report_builder_message_e msg_type) {
  report_builder_queue_item_t item;
  item.message_type = msg_type;
  if (msg_type == REPORT_BUILDER_SAMPLE_MESSAGE) {
    item.node_id = node_id;
    item.sensor_type = sensor_type;
    // This will be freed by the task after the task is done using the sensor_data
    item.sensor_data = static_cast<void *>(pvPortMalloc(sensor_data_size));
    configASSERT(item.sensor_data != NULL);
    memcpy(item.sensor_data, sensor_data, sensor_data_size);
    item.sensor_data_size = sensor_data_size;
  } else if (msg_type == REPORT_BUILDER_INCREMENT_SAMPLE_COUNT ||
             msg_type == REPORT_BUILDER_CHECK_CRC) {
    item.node_id = 0;
    item.sensor_type = 0;
    item.sensor_data = NULL;
    item.sensor_data_size = 0;
  } else {
    configASSERT(0);
  }
  xQueueSend(_report_builder_queue, &item, portMAX_DELAY);
}

/**
 * @brief Adds samples to the sensor report.
 *
 * This function takes the sensor data and adds the samples to the sensor report. The samples are added based on the sensor type and the number of samples.
 *
 * @param context The context for the sensor report encoder.
 * @param sensor_type The type of the sensor for the report.
 * @param sensor_data Pointer to the sensor data for the report.
 * @param sample_index The index of the sensor data to be copied into the sensor report.
 */
static bool addSamplesToReport(sensor_report_encoder_context_t &context, uint8_t sensor_type,
                               void *sensor_data, uint32_t sample_index) {
  bool rval = false;
  switch (sensor_type) {
  case SENSOR_TYPE_AANDERAA: {
    aanderaa_aggregations_t aanderaa_sample =
        (static_cast<aanderaa_aggregations_t *>(sensor_data))[sample_index];
    if (sensor_report_encoder_open_sample(context, AANDERAA_NUM_SAMPLE_MEMBERS,
                                          "aanderaa_current_v0") != CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to open sample in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(context, encode_double_sample_member,
                                                &aanderaa_sample.abs_speed_mean_cm_s) !=
        CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(context, encode_double_sample_member,
                                                &aanderaa_sample.abs_speed_std_cm_s) !=
        CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(context, encode_double_sample_member,
                                                &aanderaa_sample.direction_circ_mean_rad) !=
        CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(context, encode_double_sample_member,
                                                &aanderaa_sample.direction_circ_std_rad) !=
        CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(context, encode_double_sample_member,
                                                &aanderaa_sample.temp_mean_deg_c) !=
        CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(context, encode_double_sample_member,
                                                &aanderaa_sample.abs_tilt_mean_rad) !=
        CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(context, encode_double_sample_member,
                                                &aanderaa_sample.std_tilt_mean_rad) !=
        CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(context, encode_uint_sample_member,
                                                &aanderaa_sample.reading_count) !=
        CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_close_sample(context) != CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to close sample in addSamplesToReport\n");
      break;
    }
    rval = true;
    break;
  }
  case SENSOR_TYPE_SOFT: {
    soft_aggregations_t soft_sample =
        (static_cast<soft_aggregations_t *>(sensor_data))[sample_index];
    if (sensor_report_encoder_open_sample(context, SOFT_NUM_SAMPLE_MEMBERS,
                                          "bm_soft_temp_v0") != CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to open sample in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(context, encode_double_sample_member,
                                                &soft_sample.temp_mean_deg_c) != CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_close_sample(context) != CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to close sample in addSamplesToReport\n");
      break;
    }
    rval = true;
    break;
  }
  case SENSOR_TYPE_RBR_CODA: {
    rbr_coda_aggregations_t rbr_coda_sample =
        (static_cast<rbr_coda_aggregations_t *>(sensor_data))[sample_index];
    bool rbr_coda_sample_valid = false;
    switch (rbr_coda_sample.sensor_type) {
    case BmRbrDataMsg::SensorType::TEMPERATURE: {
      if (sensor_report_encoder_open_sample(context, RBR_CODA_NUM_SAMPLE_MEMBERS,
                                            "bm_rbr_t_v0") != CborNoError) {
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Failed to open rbr_coda sample in addSamplesToReport\n");
      }
      rbr_coda_sample_valid = true;
      break;
    }
    case BmRbrDataMsg::SensorType::PRESSURE: {
      if (sensor_report_encoder_open_sample(context, RBR_CODA_NUM_SAMPLE_MEMBERS,
                                            "bm_rbr_d_v0") != CborNoError) {
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Failed to open rbr_coda sample in addSamplesToReport\n");
      }
      rbr_coda_sample_valid = true;
      break;
    }
    case BmRbrDataMsg::SensorType::PRESSURE_AND_TEMPERATURE: {
      if (sensor_report_encoder_open_sample(context, RBR_CODA_NUM_SAMPLE_MEMBERS,
                                            "bm_rbr_td_v0") != CborNoError) {
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Failed to open rbr_coda sample in addSamplesToReport\n");
      }
      rbr_coda_sample_valid = true;
      break;
    }
    case BmRbrDataMsg::SensorType::UNKNOWN: {
      if (sensor_report_encoder_open_sample(context, RBR_CODA_NUM_SAMPLE_MEMBERS,
                                            "bm_rbr_unknown") != CborNoError) {
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Failed to open rbr_coda sample in addSamplesToReport\n");
      }
      rbr_coda_sample_valid = true;
      break;
    }
    default: {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Received invalid rbr type in addSamplesToReport\n");
      break;
    }
    }
    if (!rbr_coda_sample_valid) {
      break;
    }
    if (sensor_report_encoder_add_sample_member(context, encode_double_sample_member,
                                                &rbr_coda_sample.temp_mean_deg_c) !=
        CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add rbr_coda sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(context, encode_double_sample_member,
                                                &rbr_coda_sample.pressure_mean_deci_bar) !=
        CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add rbr_coda sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(context, encode_double_sample_member,
                                                &rbr_coda_sample.pressure_stdev_deci_bar) !=
        CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add rbr_coda sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_close_sample(context) != CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to close rbr_coda sample in addSamplesToReport\n");
      break;
    }
    rval = true;
    break;
  }
  case SENSOR_TYPE_SEAPOINT_TURBIDITY: {
    seapoint_turbidity_aggregations_t seapoint_turbidity_sample =
        (static_cast<seapoint_turbidity_aggregations_t *>(sensor_data))[sample_index];
    if (sensor_report_encoder_open_sample(context, SEAPOINT_TURBIDITY_NUM_SAMPLE_MEMBERS,
                                          "bm_seapoint_turbidity_v0") != CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to open seapoint_turbidity sample in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(
            context, encode_double_sample_member,
            &seapoint_turbidity_sample.turbidity_s_mean_ftu) != CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add seapoint_turbidity sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_add_sample_member(
            context, encode_double_sample_member,
            &seapoint_turbidity_sample.turbidity_r_mean_ftu) != CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to add seapoint_turbidity sample member in addSamplesToReport\n");
      break;
    }
    if (sensor_report_encoder_close_sample(context) != CborNoError) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                     "Failed to close sample in addSamplesToReport\n");
      break;
    }
    rval = true;
    break;
  }
  default: {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                   "Received invalid sensor type in addSamplesToReport\n");
    break;
  }
  }
  return rval;
}

// Task init
void reportBuilderInit(void) {
  bool save = false;
  _ctx._samplesPerReport = DEFAULT_SAMPLES_PER_REPORT;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SAMPLES_PER_REPORT,
                       strlen(AppConfig::SAMPLES_PER_REPORT), &_ctx._samplesPerReport)) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get samples per report from config, using default %" PRIu32
                   " and saving to config\n",
                   _ctx._samplesPerReport);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::SAMPLES_PER_REPORT,
                    strlen(AppConfig::SAMPLES_PER_REPORT), _ctx._samplesPerReport);
    save = true;
  }
  _ctx._transmitAggregations = DEFAULT_TRANSMIT_AGGREGATIONS;
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::TRANSMIT_AGGREGATIONS,
                       strlen(AppConfig::TRANSMIT_AGGREGATIONS), &_ctx._transmitAggregations)) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Failed to get transmit aggregations from config, using default %" PRIu32
                   " and saving to config\n",
                   _ctx._transmitAggregations);
    set_config_uint(BM_CFG_PARTITION_SYSTEM, AppConfig::TRANSMIT_AGGREGATIONS,
                    strlen(AppConfig::TRANSMIT_AGGREGATIONS), _ctx._transmitAggregations);
    save = true;
  }
  if (save) {
    save_config(BM_CFG_PARTITION_SYSTEM, false);
  }
  _ctx._sample_counter = 0;
  _ctx._config_mutex = xSemaphoreCreateMutex();
  configASSERT(_ctx._config_mutex);
  _ctx.report_period_max_network_config_cbor = NULL;
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
  (void)parameters;

  report_builder_queue_item_t item;

  while (1) {
    if (xQueueReceive(_report_builder_queue, &item, portMAX_DELAY) == pdPASS) {
      switch (item.message_type) {
      case REPORT_BUILDER_INCREMENT_SAMPLE_COUNT: {
        _ctx._sample_counter++;
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                       "Incrementing sample counter to %d\n", _ctx._sample_counter);
        if (_ctx._sample_counter >= _ctx._samplesPerReport) {
          if (_ctx._samplesPerReport > 0 && _ctx._transmitAggregations) {
            // This num_sensors is for the cbor encoder to know how many sensors will be in the report.
            // We will loop through the sensors type list and only count the number of sensors that are
            // not UNKNOWN_SENSOR_TYPE.
            uint32_t num_sensors = 0;
            for (size_t i = 0; i < _ctx._report_period_num_nodes; i++) {
              if (_ctx._report_period_sensor_type_list[i] > SENSOR_TYPE_UNKNOWN) {
                num_sensors++;
              }
            }
            // Need to have more than one node to report anything (1 node would be just the bridge)
            // We also need more than one managed sensor to report anything.
            if (_ctx._report_period_num_nodes > 1 && num_sensors > 0) {
              app_pub_sub_bm_bridge_sensor_report_data_t *message_buff = NULL;
              uint8_t *cbor_buffer = NULL;
              do {
                sensor_report_encoder_context_t context;
                cbor_buffer = static_cast<uint8_t *>(pvPortMalloc(MAX_SENSOR_REPORT_CBOR_LEN));
                configASSERT(cbor_buffer != NULL);
                if (sensor_report_encoder_open_report(cbor_buffer, MAX_SENSOR_REPORT_CBOR_LEN,
                                                      num_sensors, context) != CborNoError) {
                  bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                                 "Failed to open report in report_builder_task\n");
                  break;
                }
                bool abort_cbor_encoding = false;
                // Start at index 1 to skip the bridge
                for (size_t i = 1; i < _ctx._report_period_num_nodes; i++) {
                  report_builder_element_t *element = _ctx._reportBuilderLinkedList.findElement(
                      _ctx._report_period_node_list[i]);
                  if (element == NULL) {
                    if (_ctx._report_period_sensor_type_list[i] > SENSOR_TYPE_UNKNOWN) {
                      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                                     "No data for node %016" PRIx64
                                     " in report period, adding it to the list\n",
                                     _ctx._report_period_node_list[i]);
                      switch (_ctx._report_period_sensor_type_list[i]) {
                      case SENSOR_TYPE_AANDERAA: {
                        _ctx._reportBuilderLinkedList.findElementAndAddSampleToElement(
                            _ctx._report_period_node_list[i],
                            _ctx._report_period_sensor_type_list[i], NULL,
                            sizeof(aanderaa_aggregations_t), _ctx._samplesPerReport,
                            (_ctx._sample_counter - 1));
                        break;
                      }
                      case SENSOR_TYPE_SOFT: {
                        _ctx._reportBuilderLinkedList.findElementAndAddSampleToElement(
                            _ctx._report_period_node_list[i],
                            _ctx._report_period_sensor_type_list[i], NULL,
                            sizeof(soft_aggregations_t), _ctx._samplesPerReport,
                            (_ctx._sample_counter - 1));
                        break;
                      }
                      case SENSOR_TYPE_RBR_CODA: {
                        _ctx._reportBuilderLinkedList.findElementAndAddSampleToElement(
                            _ctx._report_period_node_list[i],
                            _ctx._report_period_sensor_type_list[i], NULL,
                            sizeof(rbr_coda_aggregations_t), _ctx._samplesPerReport,
                            (_ctx._sample_counter - 1));
                        break;
                      }
                      case SENSOR_TYPE_SEAPOINT_TURBIDITY: {
                        _ctx._reportBuilderLinkedList.findElementAndAddSampleToElement(
                            _ctx._report_period_node_list[i],
                            _ctx._report_period_sensor_type_list[i], NULL,
                            sizeof(seapoint_turbidity_aggregations_t), _ctx._samplesPerReport,
                            (_ctx._sample_counter - 1));
                        break;
                      }
                      default: {
                        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                                       "Invalid sensor type in report_builder_task\n");
                        break;
                      }
                      }
                    } else {
                      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                                     "Unknown sensor type in report_builder_task\n");
                      // This is an unkown sensor type (i.e. hello world or something else) so we won't add it to the report.
                      // So instead, lets continue the for loop to the next node!
                      continue;
                    }
                    element = _ctx._reportBuilderLinkedList.findElement(
                        _ctx._report_period_node_list[i]);
                  } else {
                    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                                   "Found data for node %016" PRIx64
                                   " adding it to the the report\n",
                                   _ctx._report_period_node_list[i]);
                  }
                  if (sensor_report_encoder_open_sensor(context, _ctx._samplesPerReport) !=
                      CborNoError) {
                    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                                   "Failed to open sensor in report_builder_task\n");
                    abort_cbor_encoding = true;
                    break;
                  }
                  for (uint32_t j = 0; j < _ctx._samplesPerReport; j++) {
                    if (!addSamplesToReport(context, element->sensor_type, element->sensor_data,
                                            j)) {
                      bridgeLogPrint(
                          BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                          "Failed to add samples to report in report_builder_task\n");
                      abort_cbor_encoding = true;
                      break;
                    }
                  }
                  if (sensor_report_encoder_close_sensor(context) != CborNoError ||
                      abort_cbor_encoding) {
                    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                                   "Failed to close sensor in report_builder_task\n");
                    abort_cbor_encoding = true;
                    break;
                  }
                }
                if (abort_cbor_encoding) {
                  break;
                }
                if (sensor_report_encoder_close_report(context) != CborNoError) {
                  bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                                 "Failed to close report in report_builder_task\n");
                  break;
                }
                size_t cbor_buffer_len = sensor_report_encoder_get_report_size_bytes(context);
                message_buff =
                    static_cast<app_pub_sub_bm_bridge_sensor_report_data_t *>(pvPortMalloc(
                        sizeof(app_pub_sub_bm_bridge_sensor_report_data_t) + cbor_buffer_len));
                configASSERT(message_buff != NULL);
                message_buff->bm_config_crc32 = _ctx._report_period_max_network_crc32;
                message_buff->cbor_buffer_len = cbor_buffer_len;
                memcpy(&message_buff->cbor_buffer, cbor_buffer, cbor_buffer_len);
                // Send cbor_buffer to the other side.
                bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_TOPIC,
                              sizeof(APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_TOPIC),
                              reinterpret_cast<uint8_t *>(message_buff),
                              sizeof(uint32_t) + sizeof(size_t) + cbor_buffer_len,
                              APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_TYPE,
                              APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_VERSION);
              } while (0);
              if (message_buff != NULL) {
                vPortFree(message_buff);
              }
              if (cbor_buffer != NULL) {
                vPortFree(cbor_buffer);
              }
            } else {
              bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                             "No nodes to send data for\n");
            }
          }
          // Always clear the list and reset the sample counter when _ctx._sample_counter >= _ctx._samplesPerReport
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                         "Clearing the list\n");
          _ctx._reportBuilderLinkedList.clear();
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                         "Clearing the sample counter\n");
          _ctx._sample_counter = 0;
          // Clear the report periods network associated data to rebuild it for the next report period
          _ctx._report_period_num_nodes = 0;
          _ctx._report_period_max_network_crc32 = 0;
          memset(_ctx._report_period_node_list, 0, sizeof(_ctx._report_period_node_list));
          memset(_ctx._report_period_sensor_type_list, 0,
                 sizeof(_ctx._report_period_sensor_type_list));
        }
        break;
      }

      case REPORT_BUILDER_SAMPLE_MESSAGE: {
        if (_ctx._samplesPerReport > 0) {
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                         "Adding sample for %016" PRIx64 " to list\n", item.node_id);
          _ctx._reportBuilderLinkedList.findElementAndAddSampleToElement(
              item.node_id, item.sensor_type, item.sensor_data, item.sensor_data_size,
              _ctx._samplesPerReport, _ctx._sample_counter);
        } else {
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                         "samplesPerReport is 0, not adding sample to list\n");
        }
        break;
      }

      case REPORT_BUILDER_CHECK_CRC: {

        // IMPORTANT: If the bus is constantly on, the currentAggPeriodMin(or sample period eventually) must be longer than the topology sampler period (1 min)
        // otherwise the CRC for the report builder may not update before a message needs to be sent

        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                       "Checking CRC in report builder!\n");
        // Get the latest crc - if it doens't match our saved one, pull the latest topology and compare the topology
        // to our current topology - if it doesn't match our current one (and mainly if it is bigger or the same size) then
        // update the crc and the topology list
        uint32_t temp_network_crc32 = 0;
        uint32_t temp_cbor_config_size = 0;
        uint8_t *last_network_config = topology_sampler_alloc_last_network_config(
            temp_network_crc32, temp_cbor_config_size);
        if (last_network_config != NULL) {
          bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                         "Got CRC %" PRIx32 " OLD CRC %" PRIx32 "\n", temp_network_crc32,
                         _ctx._report_period_max_network_crc32);
          if (temp_network_crc32 != _ctx._report_period_max_network_crc32) {
            bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                           "Getting topology in report builder!\n");
            uint64_t temp_node_list[TOPOLOGY_SAMPLER_MAX_NODE_LIST_SIZE];
            size_t temp_node_list_size = sizeof(temp_node_list);
            uint32_t temp_num_nodes;
            if (topology_sampler_get_node_list(temp_node_list, temp_node_list_size,
                                               temp_num_nodes, TOPO_TIMEOUT_MS)) {
              bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                             "Got topology in report builder!\n");
              if (temp_num_nodes >= _ctx._report_period_num_nodes) {
                bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                               "Updating CRC and topology in report builder!\n");
                if (xSemaphoreTake(_ctx._config_mutex,
                                   pdMS_TO_TICKS(NETWORK_CONFIG_MUTEX_TIMEOUT_MS))) {
                  if (_ctx.report_period_max_network_config_cbor != NULL) {
                    vPortFree(_ctx.report_period_max_network_config_cbor);
                  }
                  _ctx.report_period_max_network_config_cbor = last_network_config;
                  last_network_config = NULL;
                  _ctx.report_period_max_network_config_cbor_len = temp_cbor_config_size;
                  bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                                 "Updated reportBuilders max network configuration CBOR map\n");
                  _ctx._report_period_num_nodes = temp_num_nodes;
                  memcpy(_ctx._report_period_node_list, temp_node_list, sizeof(temp_node_list));
                  _ctx._report_period_max_network_crc32 = temp_network_crc32;
                  size_t report_period_sensor_type_list_size =
                      sizeof(_ctx._report_period_sensor_type_list);
                  if (!topology_sampler_get_sensor_type_list(
                          _ctx._report_period_sensor_type_list,
                          report_period_sensor_type_list_size, _ctx._report_period_num_nodes,
                          TOPO_TIMEOUT_MS)) {
                    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                                   "Failed to get the sensor type list in report builder!\n");
                  } else {
                    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                                   "Updated reportBuilders max network sensor type list\n");
                  }
                  xSemaphoreGive(_ctx._config_mutex);
                } else {
                  bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                                 "Failed to take the config mutex in report builder!\n");
                }
              } else {
                bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                               "Not updating CRC and topology in report builder, new topology "
                               "is smaller than the old one!\n");
              }
            } else {
              bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                             "Failed to get the node list in report builder!\n");
            }
          } else {
            bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                           "CRCs match, not updating\n");
          }
          // The last network config is not used here, but in the future it can be used to determine if varying sensor types have been
          // connected/disconnected to the network.
          if (last_network_config != NULL) {
            vPortFree(last_network_config);
          }
        }
        break;
      }

      default: {
        bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                       "Unknown message type received in report_builder_task\n");
        break;
      }
      }
      // Free the sensor data if it is not NULL, it is allocated in reportBuilderAddToQueue
      if (item.sensor_data != NULL) {
        vPortFree(item.sensor_data);
      }
    }
  }
}

/*!
 * @brief Get the last network configuration from the report builder. This will return the last
 * @brief network configuration that may be used to stamp an outbound report.
 * @note This function will allocate memory for the cbor config map, the caller is responsible for freeing it.
 * @param[out] network_crc32 The network crc32
 * @param[out] cbor_config_size The size of the cbor config map in bytes.
 * @return The cbor config map or NULL if unable to retreive.
 */
uint8_t *report_builder_alloc_last_network_config(uint32_t &network_crc32,
                                                  uint32_t &cbor_config_size) {
  uint8_t *rval = NULL;
  if (xSemaphoreTake(_ctx._config_mutex, pdMS_TO_TICKS(NETWORK_CONFIG_MUTEX_TIMEOUT_MS))) {
    do {
      if (!_ctx.report_period_max_network_config_cbor) {
        break;
      }
      network_crc32 = _ctx._report_period_max_network_crc32;
      cbor_config_size = _ctx.report_period_max_network_config_cbor_len;
      rval = static_cast<uint8_t *>(pvPortMalloc(cbor_config_size));
      configASSERT(rval);
      memcpy(rval, _ctx.report_period_max_network_config_cbor, cbor_config_size);
    } while (0);
    xSemaphoreGive(_ctx._config_mutex);
  }
  return rval;
}
