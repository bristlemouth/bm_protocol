#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct report_builder_element_s {
  uint64_t node_id;
  uint8_t sensor_type;
  void *sensor_data;
  uint32_t sample_counter;
  report_builder_element_s *next;
  report_builder_element_s *prev;
} report_builder_element_t;

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
                          void *sensor_data, uint32_t sample_counter, uint32_t size = 0);

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
