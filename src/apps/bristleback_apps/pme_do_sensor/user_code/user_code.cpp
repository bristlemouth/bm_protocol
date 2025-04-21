#include "user_code.h"
#include "FreeRTOS.h"
#include "OrderedSeparatorLineParser.h"
#include "app_pub_sub.h"
#include "array_utils.h"
#include "avgSampler.h"
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
- Pull barometric pressure from spotter to use for DOsat% calc
*/

uint32_t pmeLogEnable = 1;
bool pmeLogEnableCfg = false;
uint32_t pmeDOTIntervalS = 600;
bool pmeDOTIntervalSCfg = false;
uint32_t pmeWipeIntervalS = 14400;
bool pmeWipeIntervalSCfg = false;

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

void debugTx(void) {}

void setup(void) {
  // Load system configuration & initialize if not configured by user
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_BM_LOG_ENABLE, strlen(SENSOR_BM_LOG_ENABLE), &pmeLogEnable)) {
    pmeLogEnableCfg = true;
  }
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_BM_DOT_INTERVAL_S, strlen(SENSOR_BM_DOT_INTERVAL_S), &pmeDOTIntervalS)) {
    pmeDOTIntervalSCfg = true;
  }
  if (get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_BM_WIPE_INTERVAL_S, strlen(SENSOR_BM_WIPE_INTERVAL_S), &pmeWipeIntervalS)) {
    pmeWipeIntervalSCfg = true;
  }

  pmeSensor.init();
  pmeDoTopicStrLen = createPmeDoMeasurementDataTopic();
  pmeWipeTopicStrLen = createPmeWipeDataTopic();
  lastWipeEpochS = loadLastWipeEpoch();
  printf("Last wipe time: %lu\n", lastWipeEpochS);
  if (pmeLogEnableCfg == true) {
    printf("User-defined pmeLogEnable found: %" PRIu32 "\n", pmeLogEnable);
  }
  else {
    printf("No user-defined pmeLogEnable - defaulting to: %" PRIu32 "\n", pmeLogEnable);
  }

  if (pmeDOTIntervalSCfg == true) {
    printf("User-defined pmeDOTIntervalS found: %" PRIu32 " seconds.\n", pmeDOTIntervalS);
  }
  else {
    printf("No user-defined pmeDOTIntervalS - defaulting to: %" PRIu32 " seconds.\n", pmeDOTIntervalS);
  }

  if (pmeWipeIntervalSCfg == true) {
    printf("User-defined pmeWipeIntervalS found: %" PRIu32 " seconds.\n", pmeWipeIntervalS);
  }
  else {
    printf("No user-defined pmeWipeIntervalS - defaulting to: %" PRIu32 " seconds.\n\n", pmeWipeIntervalS);
  }
  // Ensure Vbus stable before enabling Vout
  IOWrite(&BB_VBUS_EN, 0);
  bm_delay(50);
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
          printf("#  WIPE Encoding success! | Topic: %s, cborBuf: %d, \n", pmeWipeTopic,
                 cborBuf); 
        } else {
          printf("!  Failed to encode WIPE data message\n");
        }
      }
    } else {
      if (elapsedWipe % 10 == 0) {
        printf("Wipe timer: %llu of %i seconds\n", elapsedWipe, pmeWipeIntervalS);
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

    if (pmeSensor.getDoData(d)) {
      static uint8_t cborBuf[pmeSensorDataMsgMaxSize];
      size_t encodedLen = 0;
      if (PmeDissolvedOxygenMsg::encode(d, cborBuf, sizeof(cborBuf), &encodedLen) ==
          CborNoError) {
        printf("#  DOT Encoding success! | Topic: %s, cborBuf: %d, \n", pmeDoTopic,
               cborBuf);
        bm_pub_wl(pmeDoTopic, pmeDoTopicStrLen, cborBuf, encodedLen, 0,
                  BM_COMMON_PUB_SUB_VERSION);
        if (true) {
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
    printf("DOT timer: %llu of %i seconds\n", elapsedDoSec, pmeDOTIntervalS);
    }
  }
  bm_delay(1000);
}
