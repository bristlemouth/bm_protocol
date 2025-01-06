#include "borealisSensor.h"
#include "FreeRTOS.h"
#include "bm_config.h"
#include "bm_os.h"
#include "bristlemouth.h"
#include "messages/config.h"
#include "pubsub.h"
#include "semphr.h"
#include "sensorController.h"
#include <inttypes.h>
#include <new>
#include <string.h>

#define MAX_BOREALIS_READING_PERIOD_MS(period) (period + 1000U)

static constexpr char READING_PERIOD_KEY[] = "bandsSampleTimeMs";
static struct BorealisSensor *CURRENT_SUB;

//TODO: remove this once codec is in place
CborError BorealisDataMsg::decode(Data &d, const uint8_t *cbor_buffer, size_t size) {
  (void)cbor_buffer;
  (void)size;
  memcpy(&d, cbor_buffer, sizeof(Data));
  return CborNoError;
}

/*!
 @brief Subscribe To Borealis Topic

 @details Subscribes to a specific nodes borealis topic

 @return true on success
 @return false on failure
 */
bool BorealisSensor::subscribe() {
  bool ret = false;
  char *sub = static_cast<char *>(bm_malloc(BM_TOPIC_MAX_LEN));
  if (sub) {
    uint32_t topic_strlen =
        snprintf(sub, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "%s", node_id, subtag);
    if (topic_strlen > 0) {
      ret = bm_sub_wl(sub, topic_strlen, borealisSubCallback) == BmOK;
    }
    bm_free(sub);
  }
  return ret;
}

/*!
 @brief Subscription Callback For Borealis Topic

 @details This function decodes the subscribed borealis message and sends
          formmated data to spotter as an individual reading.

 @param node_id node ID from subscription
 @param topic topic from subscription
 @param topic_len length of topic
 @param data data from subscribed message
 @param data_len length of data
 @param type unused
 @param version unused
 */
void BorealisSensor::borealisSubCallback(uint64_t node_id, const char *topic,
                                         uint16_t topic_len, const uint8_t *data,
                                         uint16_t data_len, uint8_t type, uint8_t version) {
  (void)type;
  (void)version;
  BmErr err = BmOK;
  BorealisDataMsg::Data d;
  Borealis_t *borealis = static_cast<Borealis_t *>(sensorControllerFindSensorById(node_id));

  bm_debug("Borealis data received from node %016" PRIx64 ", on topic: %.*s\n", node_id,
           topic_len, topic);
  if (borealis && borealis->type == SENSOR_TYPE_BOREALIS && borealis->_mutex) {
    if (xSemaphoreTake(borealis->_mutex, portMAX_DELAY) == pdTRUE &&
        BorealisDataMsg::decode(d, data, data_len) == CborNoError) {
      //TODO: Get borealis specific items here if necessary

      err = borealis->send_spotter_log_individual(
          "borealis", d.header, MAX_BOREALIS_READING_PERIOD_MS(borealis->m_reading_period_ms),
          //TODO: Replace with borealis specific data here
          "%.3f\n", 0.0f);
      if (err != BmOK) {
        bm_debug("Failed to send borealis individual log to spotter, reason: %d\n", err);
      }
      xSemaphoreGive(borealis->_mutex);
    } else {
      bm_debug("Failed to take the subbed Borealis mutex after getting a new reading\n");
    }
  }
}

static BmErr borealisConfigCb(uint8_t *payload) {
  BmErr err = BmENODATA;

  if (payload && CURRENT_SUB) {
    BmConfigValue *msg = reinterpret_cast<BmConfigValue *>(payload);
    size_t size = sizeof(AbstractSensor::m_reading_period_ms);
    err = bcmp_config_decode_value(UINT32, msg->data, msg->data_length,
                                   &CURRENT_SUB->m_reading_period_ms, &size);
    CURRENT_SUB = NULL;
  }

  return err;
}

/*!
 @brief Creates A Borealis Sensor Subscriber

 @param node_id node ID of the subscriber
 @param reading_period_ms reading period of 

 @return pointer to new borealis subscriber
 @return nullptr on failure
 */
Borealis_t *createBorealisSensorSub(uint64_t node_id) {
  Borealis_t *sub = static_cast<Borealis_t *>(bm_malloc(sizeof(Borealis_t)));
  Borealis_t *ret = nullptr;
  BmErr err = BmOK;

  if (sub) {
    ret = new (sub) Borealis_t();

    ret->_mutex = xSemaphoreCreateMutex();

    if (ret->_mutex) {
      ret->node_id = node_id;
      ret->type = SENSOR_TYPE_BOREALIS;
      ret->next = nullptr;
      ret->m_reading_period_ms = BorealisSensor::DEFAULT_BOREALIS_READING_PERIOD_MS;
      CURRENT_SUB = ret;
      bcmp_config_get(node_id, BM_CFG_PARTITION_SYSTEM, strlen(READING_PERIOD_KEY),
                      READING_PERIOD_KEY, &err, borealisConfigCb);
    }

    if (!ret->_mutex || err != BmOK) {
      bm_debug("Failed to create borealis sensor err: %d\n", err);
      bm_free(sub);
      ret = nullptr;
    }
  } else {
    bm_debug("Failed to allocate memory for borealis sensor\n");
  }

  return ret;
}
