#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "task_priorities.h"
#include "timer_callback_handler.h"
#include "timers.h"
#include <string.h>
#include "app_config.h"
#include "reportBuilder.h"

#define REPORT_BUILDER_QUEUE_SIZE (16)

typedef struct {
  uint32_t _sample_counter;
  uint32_t _samplesPerReport;
  uint32_t _transmitAggregations;
  // TODO add a linked list of samples from nodes
} ReportBuilderContext_t;

static ReportBuilderContext_t _ctx;
static QueueHandle_t _report_builder_message_queue = NULL;

static void report_builder_task(void *parameters);

void reportBuilderInit(cfg::Configuration* sys_cfg) {
  configASSERT(sys_cfg);
  _ctx._samplesPerReport = 0;
  sys_cfg->getConfig(AppConfig::SAMPLES_PER_REPORT, strlen(AppConfig::SAMPLES_PER_REPORT), _ctx._samplesPerReport);
  _ctx._sample_counter = 0;
  // TODO define the messages so we now how big to make the queue
  _report_builder_message_queue =
      xQueueCreate(REPORT_BUILDER_QUEUE_SIZE, sizeof(REPORT_BUILDER_MESSAGES));
  configASSERT(_report_builder_message_queue);
  // create task
  BaseType_t rval = xTaskCreate(report_builder_task, "REPORT_BUILDER", 1024, NULL,
                                REPORT_BUILDER_TASK_PRIORITY, NULL);
  configASSERT(rval == pdPASS);
}

static void report_builder_task(void *parameters) {

  // TODO - define the messages so we can use them here
  // REPORT_BUILDER_MESSAGES message;
  while (1) {
    if (xQueueReceive(_report_builder_message_queue, &message,
                      portMAX_DELAY) == pdPASS) {
      switch (message) {
      case REPORT_BUILDER_INCREMENT_SAMPLE_COUNT:
        _ctx._sample_counter++;
        if (_ctx._sample_counter >= _ctx._samplesPerReport) {
          if (_ctx._transmitAggregations) {
            // TODO - compile report into Cbor and send it to spotter
          } else {
            // TODO - is there something we should do with the report if we don't want to send it?
          }
          // empty the linked list
        }
        break;
      case REPORT_BUILDER_SAMPLE_MESSAGE:
        // TODO - add sample to the linked list
        break;
      }
    }
}

