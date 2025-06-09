#include "user_code.h"
#include "FreeRTOS.h"
#include "OrderedSeparatorLineParser.h"
#include "app_pub_sub.h"
#include "array_utils.h"
#include "avgSampler.h"
#include "barometric_pressure_data_msg.h"
#include "bm_os.h"
#include "bm_printf.h"
#include "bsp.h"
#include "configuration.h"
#include "debug.h"
#include "device_info.h"
#include "htuSampler.h"
#include "io.h"
#include "lwip/inet.h"
#include "payload_uart.h"
#include "pme_dissolved_oxygen_msg.h"
#include "pme_do_sensor.h"
#include "sensorSampler.h"
#include "serial.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"
#include "util.h"
#include <ctime>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#define LAST_WIPE_EPOCH_KEY "lastWipeEpochS";

/* TODO
- Add power switching to microDOT when sensing is not ready
- Use cbor buffers to upload data rather than raw payloads (cbor works but not implemented yet)
- Comment code
- Remove or comment out debugging printf's
- Follow as bm_pub_wl is developed
*/

uint32_t pmeLogEnable = 1;
bool pmeLogEnableCfg = false;
uint32_t pmeDOTIntervalS = 600;
bool pmeDOTIntervalSCfg = false;
uint32_t pmeWipeIntervalS = 14400;
bool pmeWipeIntervalSCfg = false;
uint32_t sensorDebugTxEnable = 0;
bool sensorDebugTxEnableCfg = false;

static float salinityPpt = 35.0;
bool salinityPptCfg = false;

static PmeSensor pmeSensor;

static char pmeDoTopic[BM_TOPIC_MAX_LEN]; // DO
static int pmeDoTopicStrLen;              // DO

static char pmeWipeTopic[BM_TOPIC_MAX_LEN]; // Wipe
static int pmeWipeTopicStrLen;              // Wipe

bool firstDo;

// Defines the max buffer size for the pme sensor message
static constexpr uint32_t pmeSensorDataMsgMaxSize = 256;
static constexpr char SENSOR_BM_LOG_ENABLE[] = "pmeLogEnable";
static constexpr char SENSOR_BM_DOT_INTERVAL_S[] = "pmeDOTIntervalS";
static constexpr char SENSOR_BM_WIPE_INTERVAL_S[] = "pmeWipeIntervalS";
static constexpr char SENSOR_DEBUG_TX_ENABLE[] = "sensorDebugTxEnable";
static constexpr char SALINITY_PPT[] = "salinityPpt";

// Make this sys cfg available when supported by backend
// static constexpr char fwVersion = "0.13.0-rc.7";

// Variables for measurements and timing
static uint32_t lastWipeEpochS = 0;
static uint64_t lastDoMeasurementUptimeSec = 0;

// Function to create the topic string for pme DO measurement data
static int createPmeDoMeasurementDataTopic(void) { // DO measurement
  int topicStrLen =
      snprintf(pmeDoTopic, BM_TOPIC_MAX_LEN, "sensor/%016" PRIx64 "/pme/do_data", getNodeId());
  configASSERT(topicStrLen > 0 && topicStrLen < BM_TOPIC_MAX_LEN);
  return topicStrLen;
}

static int createPmeWipeDataTopic(void) { // Wipe
  int topicStrLen = snprintf(pmeWipeTopic, BM_TOPIC_MAX_LEN,
                             "sensor/%016" PRIx64 "/pme/wipe_data", getNodeId());
  configASSERT(topicStrLen > 0 && topicStrLen < BM_TOPIC_MAX_LEN);
  return topicStrLen;
}

static void handle_barometric_pressure(uint64_t node_id, const char *topic, uint16_t topic_len,
                                       const uint8_t *data, uint16_t data_len, uint8_t type,
                                       uint8_t version) {
  (void)node_id;
  (void)topic;
  (void)topic_len;
  (void)type;
  (void)version;

  BarometricPressureDataMsg::Data d;
  CborError err = BarometricPressureDataMsg::decode(d, data, data_len);
  if (err == CborNoError) {
    pmeSensor.setBarometricPressure(d.barometric_pressure_mbar);
  } else {
    printf("Failed to decode barometric pressure data message. err=%d\n", err);
  }
}

void debugTx(void) {}

void setup(void) {
  // Load system configuration & initialize if not configured by user
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_BM_LOG_ENABLE,
                      strlen(SENSOR_BM_LOG_ENABLE), &pmeLogEnable)) {
    pmeLogEnableCfg = true;
  }
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_BM_DOT_INTERVAL_S,
                      strlen(SENSOR_BM_DOT_INTERVAL_S), &pmeDOTIntervalS)) {
    pmeDOTIntervalSCfg = true;
  }
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_BM_WIPE_INTERVAL_S,
                      strlen(SENSOR_BM_WIPE_INTERVAL_S), &pmeWipeIntervalS)) {
    pmeWipeIntervalSCfg = true;
  }
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_DEBUG_TX_ENABLE,
                      strlen(SENSOR_DEBUG_TX_ENABLE), &sensorDebugTxEnable)) {
    sensorDebugTxEnableCfg = true;
  }
  if (get_config_float(BM_CFG_PARTITION_SYSTEM, SALINITY_PPT,
                   strlen(SALINITY_PPT), &salinityPpt)) {
    salinityPptCfg = true;
  }

  // Subscribe to barometric pressure
  bm_sub("sensor/*/barometric_pressure", handle_barometric_pressure);
  // Possibly longer term, subscribe to salinity
  // bm_sub("sensor/*/salinity", handle_salinity);

  pmeSensor.init();
  pmeDoTopicStrLen = createPmeDoMeasurementDataTopic();
  pmeWipeTopicStrLen = createPmeWipeDataTopic();
  lastWipeEpochS = loadLastWipeEpoch();
  printf("Last wipe time: %lu\n", lastWipeEpochS);
  if (pmeLogEnableCfg == true) {
    printf("User-defined pmeLogEnable found: %" PRIu32 "\n", pmeLogEnable);
  } else {
    printf("No user-defined pmeLogEnable - defaulting to: %" PRIu32 "\n", pmeLogEnable);
  }

  if (pmeDOTIntervalSCfg == true) {
    printf("User-defined pmeDOTIntervalS found: %" PRIu32 " seconds.\n", pmeDOTIntervalS);
  } else {
    printf("No user-defined pmeDOTIntervalS - defaulting to: %" PRIu32 " seconds.\n",
           pmeDOTIntervalS);
  }

  if (pmeWipeIntervalSCfg == true) {
    printf("User-defined pmeWipeIntervalS found: %" PRIu32 " seconds.\n", pmeWipeIntervalS);
  } else {
    printf("No user-defined pmeWipeIntervalS - defaulting to: %" PRIu32 " seconds.\n",
           pmeWipeIntervalS);
  }

  if (sensorDebugTxEnableCfg == true) {
    printf("User-defined sensorDebugTxEnable found: %" PRIu32 "\n", sensorDebugTxEnable);
  } else {
    printf("No user-defined sensorDebugTxEnable - defaulting to: %" PRIu32 "\n",
           sensorDebugTxEnable);
  }

  if (salinityPptCfg == true) {
    printf("User-defined salinityPpt found: %f\n", salinityPpt);
  } else {
    printf("No user-defined salinityPpt - defaulting to: %f\n", salinityPpt);
  }

  printf("\n");

  // VBUS and 9V are not used in microDOT so we can turn these both off (they are active when LOW)
  IOWrite(&BB_PL_BUCK_EN, 1);
  IOWrite(&BB_VBUS_EN, 1);
  ledAllOff();
  //Set first DO measurement flag to true to trigger startup DO measurement
  firstDo = true;
}

void loop(void) {
  RTCTimeAndDate_t time_and_date = {};
  bool rtcIsSet = rtcGet(&time_and_date);
  uint64_t epochTimeSec = (rtcGetMicroSeconds(&time_and_date) / 1000000);
  uint64_t currentUptimeSec = uptimeGetMs() / 1000;

  //////////
  // Wipe //
  //////////

  if (rtcIsSet) {
    // Check if enough time has passed since the last wipe
    uint64_t elapsedWipe = epochTimeSec - lastWipeEpochS;
    if (elapsedWipe >= pmeWipeIntervalS) {
      // Perform a Wipe
      static PmeWipeMsg::Data w;
      if (pmeSensor.getWipeData(w)) {
        saveLastWipeEpoch(epochTimeSec);
        lastWipeEpochS = epochTimeSec;
        static uint8_t cborBuf[pmeSensorDataMsgMaxSize];
        size_t encodedLen = 0;
        if (true) {
          debugTx();
        }
        if (PmeWipeMsg::encode(w, cborBuf, sizeof(cborBuf), &encodedLen) == CborNoError) {
          bm_pub_wl(pmeWipeTopic, pmeWipeTopicStrLen, cborBuf, encodedLen, 0,
                    BM_COMMON_PUB_SUB_VERSION);
          //printf("#  WIPE Encoding success! | Topic: %s, cborBuf: %" PRIx8 "%" PRIx8 "%" PRIx8
          //       "...\n", pmeWipeTopic, cborBuf[0], cborBuf[1], cborBuf[2]);
        } else {
          printf("!  Failed to encode WIPE data message\n");
        }
      }
    } else {
      if (elapsedWipe % 10 == 0) {
        printf("Wipe timer: %llu of %lu seconds\n", elapsedWipe, pmeWipeIntervalS);
      }
    }
  } else {
    if (currentUptimeSec % 10 == 0) {
      printf("Wipe timer: !!! Not wiping - RTC is not set!\n");
    }
  }

  /////////
  // DOT //
  /////////
  static PmeDissolvedOxygenMsg::Data d;
  uint64_t elapsedDoSec = currentUptimeSec - lastDoMeasurementUptimeSec;
  if (elapsedDoSec >= pmeDOTIntervalS || firstDo) {

    if (firstDo) {
      firstDo = false; //Turn off initial DO flag
    }

    if (pmeSensor.getDoData(d, salinityPpt)) {
      static uint8_t cborBuf[pmeSensorDataMsgMaxSize];
      size_t encodedLen = 0;
      if (PmeDissolvedOxygenMsg::encode(d, cborBuf, sizeof(cborBuf), &encodedLen) ==
          CborNoError) {
        //printf("#  DOT Encoding success! | Topic: %s, cborBuf: %" PRIx8 "%" PRIx8 "%" PRIx8
        //       "...\n", pmeDoTopic, cborBuf[0], cborBuf[1], cborBuf[2]);
        bm_pub_wl(pmeDoTopic, pmeDoTopicStrLen, cborBuf, encodedLen, 0,
                  BM_COMMON_PUB_SUB_VERSION);
        if (false) {
          debugTx();
        }
      } else {
        printf("!  Failed to encode DOT measurement data message\n");
      }
    } else {
      printf("!  Failed to perform DOT measurement\n");
    }
    lastDoMeasurementUptimeSec = currentUptimeSec;
  } else {
    if (elapsedDoSec % 10 == 0) {
      printf("DOT timer: %llu of %lu seconds\n", elapsedDoSec, pmeDOTIntervalS);
    }
  }
  bm_delay(1000);
}
