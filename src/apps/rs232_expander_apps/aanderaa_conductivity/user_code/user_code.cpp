#include "user_code.h"
#include "FreeRTOS.h"
#include "aanderaa_conductivity_msg.h"
#include "spotter.h"
#include "pubsub.h"
#include "bsp.h"
#include "debug.h"
#include "device_info.h"
#include "uptime.h"
#include "app_util.h"
#include "aanderaa_conductivity_sensor.h"

static constexpr uint32_t AANDERAA_CONDUCTIVITY_DATA_MSG_MAX_SIZE = 256;
static AanderaaConductivitySensor aanderaa_conductivity_sensor;
static char aanderaa_conductivity_topic[BM_TOPIC_MAX_LEN];
static int aanderaa_conductivity_topic_str_len;

static int createAanderaaConductivityDataTopic(void) {
  int topic_str_len = snprintf(aanderaa_conductivity_topic, BM_TOPIC_MAX_LEN,
                               "sensor/%016" PRIx64 "/sofar/aanderaa_conductivity_data", getNodeId());
  configASSERT(topic_str_len > 0 && topic_str_len < BM_TOPIC_MAX_LEN);
  return topic_str_len;
}

void setup(void) {
  aanderaa_conductivity_sensor.init();
  aanderaa_conductivity_topic_str_len = createAanderaaConductivityDataTopic();
  IOWrite(&VBUS_EN, 0);
  vTaskDelay(pdMS_TO_TICKS(500)); // Wait for Vbus to stabilize
  IOWrite(&PL_BUCK_EN, 0);
  aanderaa_conductivity_sensor.configureSensor();
}
void loop(void) {
  // Read and handle line from sensor
/*
  static AanderaaConductivityMsg::Data d;
  if (aanderaa_conductivity_sensor.getData(d)) {
    static uint8_t cbor_buf[AANDERAA_CONDUCTIVITY_DATA_MSG_MAX_SIZE];
    size_t encoded_len = 0;
    if (AanderaaConductivityMsg::encode(d, cbor_buf, sizeof(cbor_buf), &encoded_len) == CborNoError) {
      bm_pub_wl(aanderaa_conductivity_topic, aanderaa_conductivity_topic_str_len, cbor_buf, encoded_len, 0, BM_COMMON_PUB_SUB_VERSION);
    } else {
      printf("Failed to encode conductivity data message\n");
    }
  }
*/
}