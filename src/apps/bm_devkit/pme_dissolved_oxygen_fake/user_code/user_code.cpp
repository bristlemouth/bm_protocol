#include "user_code.h"
#include "FreeRTOS.h"
#include "device_info.h"
#include "pubsub.h"
#include "topology.h"
#include "uptime.h"
#include "pme_dissolved_oxygen_msg.h"
#include "barometric_pressure_data_msg.h"
#include "pme_wipe_msg.h"
#include "stm32_rtc.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

extern "C" {
  #include "messages/resource_discovery.h"
}

// Modified Sub + Pub Example to create a fake PME DO sensor for testing the bridge code!

/*
This application demonstrates how to subscribe to a data topic over bristlemouth via the bm_sub function.
The application subscribes to the topic "pubsub_example" and prints the received message to the console.
The callback function subscribe_callback is called when a message is received.
To test this application, you will need to run the pub_example application on another node.
*/

// The topic type is a identifier for the topic to encode different data types, we don't need to worry about this value for the tutorial.
static constexpr uint32_t SPOTTER_BARO_TYPE = (1);
// The topic version is a version number for the topic, in case things change we don't need to worry about this value for the tutorial.
static constexpr uint32_t SPOTTER_BARO_VERSION = (1);
// This is the topic to subscribe to, the publisher will need to publish to this topic (see the application pub_example)
static const char *const SPOTTER_BARO_TOPIC = "spotter/barometric_pressure";

char *pub_topic;
int topic_strlen;

// Publish at a rate of 10 minutes
static constexpr uint32_t PUBLISH_PERIOD_MS = (10 * 60 * 1000);
static constexpr uint32_t DO_PUB_TOPIC_VERSION = (1);
static const char *const DO_SUB_TAG = "/pme/do_reading";
static constexpr uint32_t PME_DO_MSG_MAX_SIZE = 256;

// The wipers sensor data
char *pub_topic_wiper;
int topic_strlen_wiper;
static constexpr uint32_t PME_WIPER_PUB_TOPIC_VERSION = (1);
static const char *const PME_WIPER_SUB_TAG = "/pme/wiper";
static constexpr uint32_t PME_WIPE_MSG_MAX_SIZE = 256;

// used to track if we have a source for our barometric pressure data!
bool baro_data_subscribed = false;
static uint64_t check_for_baro_pubs_timer = 0;

static void subscribe_callback(uint64_t node_id, const char *topic, uint16_t topic_len,
                               const uint8_t *data, uint16_t data_len, uint8_t type,
                               uint8_t version) {
  // These parameters are unused for this example, but in a real application you would use them to process the message.
  (void)topic_len;
  (void)data_len;
  (void)type;
  (void)version;
  (void)topic;
  // Check if the message is of the correct type and version
  // if (type != SPOTTER_BARO_TYPE || version != SPOTTER_BARO_VERSION) {
  //   printf("Received message with incorrect type or version\n");
  //   return;
  // }
  // if (strncmp(topic, SPOTTER_BARO_TOPIC, strlen(SPOTTER_BARO_TOPIC)) == 0) {
  //   // Print the received message
  BarometricPressureDataMsg::Data baro_data;
  if (BarometricPressureDataMsg::decode(baro_data, data, data_len) == CborNoError) {
    printf("Recieved pressure publication from node %016" PRIx64 " on topic %.*s\n\tpressure: %0.02f\n", node_id,
            topic_len, topic, baro_data.barometric_pressure_mbar);
  } else {
    printf("Failed to decode barometric pressure data message\n");
  }
}

static void sub_to_baro_publicating_node(void *bcmp_resource_table_reply) {
  BcmpResourceTableReply *reply =
      reinterpret_cast<BcmpResourceTableReply *>(bcmp_resource_table_reply);

  printf("User code received resource table reply from %016" PRIx64 "\n", reply->node_id);
  uint16_t num_pubs = reply->num_pubs;
  size_t offset = 0;
  while (num_pubs) {
    BcmpResource *cur_resource = (BcmpResource *)&reply->resource_list[offset];
    offset += (sizeof(BcmpResource) + cur_resource->resource_len);
    num_pubs--;
    // check for "sensor/" and "/barometric_pressure" in the string!
    if (strncmp(cur_resource->resource, "sensor/", strlen("sensor/")) == 0 &&
        strncmp(cur_resource->resource + cur_resource->resource_len - strlen("/barometric_pressure"),
                "/barometric_pressure", strlen("/barometric_pressure")) == 0) {
      // we have a source for our barometric pressure data!
      printf("FOUND A BARO SENSOR!\n");
      baro_data_subscribed = true;
      bm_sub_wl(cur_resource->resource, cur_resource->resource_len, subscribe_callback);
    }
  }
}

static void look_for_baro_publications(NetworkTopology *network_topology) {
  if (network_topology) {
    NeighborTableEntry *cursor = NULL;
    for (cursor = network_topology->front; cursor != NULL; cursor = cursor->nextNode) {
      bcmp_resource_discovery_send_request(cursor->neighbor_table_reply->node_id, sub_to_baro_publicating_node);
    }
  } else {
    printf("Failed to get network topology while looking for nodes that publish baro data\n");
  }
}

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  // Subscribe to the topic and provide the callback function to be called when a message is received.
  // bm_sub(SPOTTER_BARO_TOPIC, subscribe_callback);

  pub_topic = static_cast<char *>(pvPortMalloc(BM_TOPIC_MAX_LEN));
  configASSERT(pub_topic);
  topic_strlen = snprintf(pub_topic, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", getNodeId(), DO_SUB_TAG);

  pub_topic_wiper = static_cast<char *>(pvPortMalloc(BM_TOPIC_MAX_LEN));
  configASSERT(pub_topic_wiper);
  topic_strlen_wiper = snprintf(pub_topic_wiper, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", getNodeId(), PME_WIPER_SUB_TAG);
}

void loop(void) {

  if (!baro_data_subscribed && uptimeGetMs() - check_for_baro_pubs_timer >= 10000U) {
    // get the topology list and check if we have a source for our barometric pressure data
    bcmp_topology_start(look_for_baro_publications);
    check_for_baro_pubs_timer = uptimeGetMs();
  }


  static uint64_t publishTimer = uptimeGetMs();
  // Publish data every PUBLISH_PERIOD_MS milliseconds.
  if (uptimeGetMs() - publishTimer >= PUBLISH_PERIOD_MS) {

    PmeDissolvedOxygenMsg::Data pme_do_data;

    RTCTimeAndDate_t datetime;
    if (rtcGet(&datetime) == pdPASS) {
      pme_do_data.header.reading_time_utc_ms = (rtcGetMicroSeconds(&datetime) / 1e3);
      srand(pme_do_data.header.reading_time_utc_ms & 0xFFFFFFFF);
    }

    pme_do_data = {
      .header = {
        .version = PmeDissolvedOxygenMsg::VERSION,
        .reading_time_utc_ms = (rtcGetMicroSeconds(&datetime) / 1e3),
        .reading_uptime_millis = uptimeGetMs(),
        .sensor_reading_time_ms = uptimeGetMs(),
      },
      // TODO - only add the saturation when we get baro data once that is implemented!
      .temperature_deg_c = 25.0 + (rand() % 10) - (rand() % 10),
      .do_mg_per_l = (rand() % 100) + 1,
      .quality = ((double)((rand() % 100) + 1)) / 100,
      .do_saturation_pct = (rand() % 100),
    };
    uint8_t cbor_buf[PME_DO_MSG_MAX_SIZE];
    size_t encoded_len = 0;
    if (PmeDissolvedOxygenMsg::encode(pme_do_data, cbor_buf, sizeof(cbor_buf), &encoded_len) == CborNoError) {
      if (bm_pub_wl(pub_topic, topic_strlen, cbor_buf, encoded_len, 0, DO_PUB_TOPIC_VERSION) != BmOK) {
        printf("Failed to publish PME DO message\n");
      } else {
        printf("Published PME DO message to network:\n\ttemperature: %.2f\n\tdissolved oxygen: %.2f\n\tquality: %.2f\n\tsaturation: %.2f\n",
               pme_do_data.temperature_deg_c, pme_do_data.do_mg_per_l, pme_do_data.quality, pme_do_data.do_saturation_pct);
      }
    } else {
      printf("Failed to encode PME DO data message\n");
    }

    PmeWipeMsg::Data pme_wipe_data = {
      .header = {
        .version = PmeWipeMsg::VERSION,
        .reading_time_utc_ms = (rtcGetMicroSeconds(&datetime) / 1e3),
        .reading_uptime_millis = uptimeGetMs(),
        .sensor_reading_time_ms = uptimeGetMs(),
      },
      .wipe_current_mean_ma = (rand() % 100) + 1,
      .wipe_duration_s = ((rand() % 100) + 1) / 10,
    };

    uint8_t wipe_cbor_buf[PME_WIPE_MSG_MAX_SIZE];
    size_t wipe_encoded_len = 0;
    if (PmeWipeMsg::encode(pme_wipe_data, wipe_cbor_buf, sizeof(wipe_cbor_buf), &wipe_encoded_len) == CborNoError) {
      if (bm_pub_wl(pub_topic_wiper, topic_strlen_wiper, wipe_cbor_buf, wipe_encoded_len, 0, PME_WIPER_PUB_TOPIC_VERSION) != BmOK) {
        printf("Failed to publish PME Wiper message\n");
      } else {
        printf("Published PME Wiper message to network:\n\tcurrent: %.2f\n\tduration: %.2f\n",
               pme_wipe_data.wipe_current_mean_ma, pme_wipe_data.wipe_duration_s);
      }
    } else {
      printf("Failed to encode PME Wiper data message\n");
    }

    // Increment the publish timer
    publishTimer += PUBLISH_PERIOD_MS;
  }
}
