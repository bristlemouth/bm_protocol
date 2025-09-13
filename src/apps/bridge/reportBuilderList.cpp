#include "reportBuilderList.h"
#include "FreeRTOS.h"
#include "aanderaaSensor.h"
#include "aanderaaConductivitySensor.h"
#include "borealisSensor.h"
#include "bridgeLog.h"
#include "pmeDissolvedOxygenSensor.h"
#include "rbrCodaSensor.h"
#include "seapointTurbiditySensor.h"
#include "softSensor.h"
#include <cmath>

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
  element->size = sensor_data_size;
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
 * @brief Helper function to set up conductivity sensor data pointers.
 * @param element Pointer to the element in the linked list.
 * @param nan_sample Pointer to store the NAN sample reference.
 * @param dst Pointer to store the destination data reference.
 */
void ReportBuilderLinkedList::setupConductivitySensorPointers(report_builder_element_t *element,
                                                              const void **nan_sample,
                                                              void **dst) {
  *nan_sample = &AanderaaConductivitySensor::aanderaa_conductivity_NAN_AGG;
  *dst = &(static_cast<aanderaa_conductivity_aggregations_t *>(
      element->sensor_data))[element->sample_counter];
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
  void *dst = NULL;
  const void *nan_sample = NULL;

  switch (sensor_type) {
  case SENSOR_TYPE_AANDERAA: {
    nan_sample = &AanderaaSensor::NAN_AGG;
    dst = &(
        static_cast<aanderaa_aggregations_t *>(element->sensor_data))[element->sample_counter];
    break;
  }
  case SENSOR_TYPE_SOFT: {
    nan_sample = &SoftSensor::SOFT_NAN_AGG;
    dst = &(static_cast<soft_aggregations_t *>(element->sensor_data))[element->sample_counter];
    break;
  }
  case SENSOR_TYPE_RBR_CODA: {
    nan_sample = &RbrCodaSensor::RBR_CODA_NAN_AGG;
    dst = &(
        static_cast<rbr_coda_aggregations_t *>(element->sensor_data))[element->sample_counter];
    break;
  }
  case SENSOR_TYPE_SEAPOINT_TURBIDITY: {
    nan_sample = &SeapointTurbiditySensor::seapoint_turbidity_NAN_AGG;
    dst = &(static_cast<seapoint_turbidity_aggregations_t *>(
        element->sensor_data))[element->sample_counter];
    break;
  }
  case SENSOR_TYPE_AANDERAA_CONDUCTIVITY: {
    setupConductivitySensorPointers(element, &nan_sample, &dst);
    break;
  }
  case SENSOR_TYPE_PME_DO: {
    nan_sample = &PmeDissolvedOxygenSensor::PME_DO_NAN_AGG;
    dst = &(static_cast<pme_dissolved_oxygen_aggregations_t *>(
        element->sensor_data))[element->sample_counter];
    break;
  }
  case SENSOR_TYPE_BOREALIS: {
    nan_sample = &BorealisSensor::AOS_BOREALIS_NAN_AGG;
    dst = static_cast<BorealisAggregationData_t *>(element->sensor_data);
    dst = static_cast<uint8_t *>(dst) + element->size * element->sample_counter;
    break;
  }
  default: {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Unknown sensor type in addSampleToElement\n");
    configASSERT(0);
    break;
  }
  }

  if (dst && nan_sample && element->size) {
    if (element->sample_counter < sample_counter) {
      // Back fill the sensor_data with NANs if we are not on the right sample counter
      // We use the element->sample_counter to track within each element how many samples
      // the element has received.
      for (; element->sample_counter < sample_counter; element->sample_counter++) {
        memcpy(dst, nan_sample, element->size);
        dst = static_cast<uint8_t *>(dst) + element->size;
      }
    }
    // Copy the sensor data into the elements array in the correct location within the buffer
    // If it is NULL then just fill it with NAN again
    if (sensor_data != NULL) {
      memcpy(dst, sensor_data, element->size);
    } else {
      memcpy(dst, nan_sample, element->size);
    }
    element->sample_counter++;
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
