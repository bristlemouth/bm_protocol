#include "user_code.h"
#include "FreeRTOS.h"
#include "bm_seapoint_turbidity_data_msg.h"
#include "spotter.h"
#include "pubsub.h"
#include "bsp.h"
#include "debug.h"
#include "device_info.h"
#include "seapoint_turbidity_sensor.h"
#include "uptime.h"
#include "app_util.h"

static constexpr uint32_t BM_SEAPOINT_TURBIDITY_DATA_MSG_MAX_SIZE = 256;

static SeapointTurbiditySensor seapoint_turbidity_sensor;
static char seapoint_turbidity_topic[BM_TOPIC_MAX_LEN];
static int seapoint_turbidity_topic_str_len;

static int createSeapointTurbidityDataTopic(void) {
  int topic_str_len = snprintf(seapoint_turbidity_topic, BM_TOPIC_MAX_LEN,
                               "sensor/%016" PRIx64 "/sofar/seapoint_turbidity_data", getNodeId());
  configASSERT(topic_str_len > 0 && topic_str_len < BM_TOPIC_MAX_LEN);
  return topic_str_len;
}

void setup(void) {
  seapoint_turbidity_sensor.init();
  seapoint_turbidity_topic_str_len = createSeapointTurbidityDataTopic();
  IOWrite(&VBUS_EN, 0);
  vTaskDelay(pdMS_TO_TICKS(500)); // Wait for Vbus to stabilize
  IOWrite(&PL_BUCK_EN, 0);
}

void loop(void) {
  // Read and handle line from sensor
  static BmSeapointTurbidityDataMsg::Data d;
  if (seapoint_turbidity_sensor.getData(d)) {
    static uint8_t cbor_buf[BM_SEAPOINT_TURBIDITY_DATA_MSG_MAX_SIZE];
    size_t encoded_len = 0;
    if (BmSeapointTurbidityDataMsg::encode(d, cbor_buf, sizeof(cbor_buf), &encoded_len) == CborNoError) {
      bm_pub_wl(seapoint_turbidity_topic, seapoint_turbidity_topic_str_len, cbor_buf, encoded_len, 0, BM_COMMON_PUB_SUB_VERSION);
    } else {
      printf("Failed to encode turbidity data message\n");
    }
  }
}
