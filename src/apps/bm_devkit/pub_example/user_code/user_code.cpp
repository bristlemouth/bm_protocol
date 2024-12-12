#include "user_code.h"
#include "pubsub.h"
#include "uptime.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
This application demonstrates how to publish data over bristlemouth via the bm_pub function.
The application publishes a message to the topic "pubsub_example" every second.
The message is a simple string that includes the uptime in milliseconds.
To test this application, you will need to run the sub_example application on another node.
*/

// Publish at a rate of 1 Hz
static constexpr uint32_t PUBLISH_PERIOD_MS = (1000);
// The topic type is a identifier for the topic to encode different data types, we don't need to worry about this value for the tutorial,
// other than that it must be the same for the publisher and subscriber.
static constexpr uint32_t EXAMPLE_PUBLISH_TOPIC_TYPE = (1);
// The topic version is a version number for the topic, in case things change we don't need to worry about this value for the tutorial,
// other than that it must be the same for the publisher and subscriber.
static constexpr uint32_t EXAMPLE_PUBLISH_TOPIC_VERSION = (1);
// This is the topic to publish to, the subscriber will need to know this topic to receive the data (see the application sub_example)
static const char *const EXAMPLE_PUBLISH_TOPIC = "pubsub_example";

void setup(void) { /* USER ONE-TIME SETUP CODE GOES HERE */ }

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  /// This section demonstrates a simple application to publish data over bristlemouth every second.
  static uint64_t publishTimer = uptimeGetMs();
  // Publish data every PUBLISH_PERIOD_MS milliseconds.
  if (uptimeGetMs() - publishTimer >= PUBLISH_PERIOD_MS) {
    // This is a data buffer to hold the data to be published.
    // It is larger than necessary to demonstrate the use of the bm_pub function.
    static uint8_t data_buffer[128];
    // Clear the data buffer
    memset(data_buffer, 0, sizeof(data_buffer));
    // Print a simple message into the data buffer to publish to the topic.
    sprintf((char *)data_buffer, "[%" PRId64 " ms] Hello World!", uptimeGetMs());
    // Publish the data buffer to the topic.
    if (bm_pub(EXAMPLE_PUBLISH_TOPIC, data_buffer, sizeof(data_buffer),
               EXAMPLE_PUBLISH_TOPIC_TYPE, EXAMPLE_PUBLISH_TOPIC_VERSION) == BmOK) {
      printf("Failed to publish message\n");
    } else {
      printf("Published message to network: %s\n", data_buffer);
    }
    // Increment the publish timer
    publishTimer += PUBLISH_PERIOD_MS;
  }
}
