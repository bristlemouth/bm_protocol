#include "user_code.h"
#include "bm_network.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "bristlefin.h"
#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "sensors.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "util.h"
#include <stdint.h>

// The topic type is a identifier for the topic to encode different data types, we don't need to worry about this value for the tutorial.
static constexpr uint32_t EXAMPLE_SUBSCRIBE_TOPIC_TYPE = (1);
// The topic version is a version number for the topic, in case things change we don't need to worry about this value for the tutorial.
static constexpr uint32_t  EXAMPLE_SUBSCRIBE_TOPIC_VERSION =(1);
// This is the topic to publish to, the subscriber will need to know this topic to receive the data (see the application sub_example)
static const char *const EXAMPLE_SUBSCRIBE_TOPIC = "pubsub_example";

static void subscribe_callback(uint64_t node_id, const char* topic, uint16_t topic_len, const uint8_t* data, uint16_t data_len, uint8_t type, uint8_t version) {
  // These parameters are unused for this example, but in a real application you would use them to process the message.
  (void)topic_len;
  (void)data_len;
  // Check if the message is of the correct type and version
  if(type != EXAMPLE_SUBSCRIBE_TOPIC_TYPE || version != EXAMPLE_SUBSCRIBE_TOPIC_VERSION) {
    printf("Received message with incorrect type or version\n");
    return;
  }
  if(strncmp(topic, EXAMPLE_SUBSCRIBE_TOPIC, strlen(EXAMPLE_SUBSCRIBE_TOPIC)) == 0) {
    // Print the received message
    printf("Received message from node %016" PRIx64 " on topic %s:\"%s\"\n", node_id, EXAMPLE_SUBSCRIBE_TOPIC, data);
  }

}

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  bm_sub(EXAMPLE_SUBSCRIBE_TOPIC, subscribe_callback);
}

void loop(void) {
  // Nothing to do!
  // The callback function will be called independently from this loop
  // when a message is received.
}
