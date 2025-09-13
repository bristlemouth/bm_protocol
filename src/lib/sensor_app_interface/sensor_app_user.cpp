#include "sensor_app_user.h"
#include "FreeRTOS.h"
#include "aanderaa_conductivity_msg.h"
#include "pubsub.h"
#include "debug.h"
#include "device_info.h"
#include "uptime.h"
#include "app_util.h"

static constexpr uint32_t AANDERAA_CONDUCTIVITY_MSG_MAX_SIZE = 256;

int SensorAppUser::createAanderaaConductivityDataTopic(void) {
  aanderaa_conductivity_topic_str_len = snprintf(this->aanderaa_conductivity_topic, TOPIC_MAX_LEN,
                               "sensor/%016" PRIx64 "/sofar/aanderaa_conductivity_data", getNodeId());
  configASSERT(aanderaa_conductivity_topic_str_len > 0 && aanderaa_conductivity_topic_str_len < TOPIC_MAX_LEN);
  return aanderaa_conductivity_topic_str_len;
}

void SensorAppUser::setup_with_pins(IOPinHandle_t* vbus_en, IOPinHandle_t* pl_buck_en, uint32_t settle_time_ms) {
  // Store platform-specific pin handles
  this->vbus_en_pin = vbus_en;
  this->pl_buck_en_pin = pl_buck_en;
  this->vbus_settle_time_ms = settle_time_ms;

  // Initialize sensor and topic
  aanderaa_conductivity_sensor.init();
  createAanderaaConductivityDataTopic();

  // Initialize power using platform-specific pins
  IOWrite(vbus_en_pin, 0);
  vTaskDelay(pdMS_TO_TICKS(vbus_settle_time_ms)); // Wait for Vbus to stabilize
  IOWrite(pl_buck_en_pin, 0);
}

// void setup(void) {
//   // This function should not be called directly - use setup_with_pins() instead
//   // But we provide a default implementation that will fail at compile time
//   // if the platform pins are not defined
// #if defined(BB_VBUS_EN) && defined(BB_PL_BUCK_EN)
//   setup_with_pins(&BB_VBUS_EN, &BB_PL_BUCK_EN, 500);
// #elif defined(VBUS_EN) && defined(PL_BUCK_EN)
//   setup_with_pins(&VBUS_EN, &PL_BUCK_EN, 500);
// #else
//   #error "Platform power management pins not defined. Use setup_with_pins() instead."
// #endif
// }

void SensorAppUser::loop(void) {
  // Read and handle line from sensor
  static AanderaaConductivityMsg::Data d;
  if (aanderaa_conductivity_sensor.getData(d)) {
    static uint8_t cbor_buf[AANDERAA_CONDUCTIVITY_MSG_MAX_SIZE];
    size_t encoded_len = 0;
    if (AanderaaConductivityMsg::encode(d, cbor_buf, sizeof(cbor_buf), &encoded_len) == CborNoError) {
      bm_pub_wl(this->aanderaa_conductivity_topic, this->aanderaa_conductivity_topic_str_len, cbor_buf, encoded_len, 0, BM_COMMON_PUB_SUB_VERSION);
    } else {
      printf("Failed to encode conductivity data message\n");
    }
  }
}
