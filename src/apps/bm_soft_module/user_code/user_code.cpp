#include "user_code.h"
#include "TSYS01.h"
#include "bm_network.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "bm_soft_data_msg.h"
#include "bristlefin.h"
#include "bsp.h"
#include "configuration.h"
#include "debug.h"
#include "device_info.h"
#include "lwip/inet.h"
#include "sensorWatchdog.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "util.h"

#define PAYLOAD_WATCHDOG_TIMEOUT_MS (60 * 1000)
#define NO_MAX_TRIGGER (0)
#define BM_SOFT_DATA_MSG_MAX_SIZE (128)
#define BM_SOFT_PERIODIC_INIT_DELAY_MS (500)

static constexpr char SOFTMODULE_WATCHDOG_ID[] = "softmodule";
static constexpr char soft_log[] = "soft.log";
static constexpr char soft_cfg_tsys_id[] = "tsysId";
static constexpr char soft_cfg_cal_temp_c[] = "calTempC";
static constexpr char soft_cfg_cal_time_epoch[] = "calTimeEpoch";
static constexpr char soft_cfg_cal_offset_milli_c[] = "calOffsetMilliC";
static constexpr char soft_cfg_reading_period_ms[] = "softReadingPeriodMs";

// app_main passes a handle to the user config partition in NVM.
extern cfg::Configuration *userConfigurationPartition;
extern cfg::Configuration *sysConfigurationPartition;

static TSYS01 soft(&spi1, &BM_CS);
static uint32_t serial_number = 0;
static uint32_t soft_delay_ms = 500;
static uint32_t cal_time_epoch = 0;
static char bmSoftTopic[BM_TOPIC_MAX_LEN];
static int bmSoftTopicStrLen;
static float calibrationOffsetDegC = 0.0;

static bool BmSoftWatchdogHandler(void *arg);
static bool BmSoftStartAndValidate(void);
static int createBmSoftDataTopic(void);
static void BmSoftInitalize(void);

static void getConfigs() {
  if (sysConfigurationPartition->getConfig(soft_cfg_tsys_id, strlen(soft_cfg_tsys_id),
                                           serial_number)) {
    printf("TSYS serial number: %" PRIu32 "\n", serial_number);
  } else {
    printf("No TSYS serial number found\n");
    bm_fprintf(0, soft_log, "No TSYS serial number found\n");
    bm_printf(0, "No TSYS serial number found");
  }

  int32_t calTempC = 0;
  if (sysConfigurationPartition->getConfig(soft_cfg_cal_temp_c, strlen(soft_cfg_cal_temp_c),
                                           calTempC)) {
    printf("Calibration temperature: %" PRId32 "\n", calTempC);
  } else {
    printf("No calibration temperature found\n");
    bm_fprintf(0, soft_log, "No calibration temperature found\n");
    bm_printf(0, "No calibration temperature found");
  }

  if (sysConfigurationPartition->getConfig(soft_cfg_cal_time_epoch,
                                           strlen(soft_cfg_cal_time_epoch), cal_time_epoch)) {
    printf("Calibration time: %" PRIu32 "\n", cal_time_epoch);
  } else {
    printf("No calibration time found\n");
    bm_fprintf(0, soft_log, "No calibration time found\n");
    bm_printf(0, "No calibration time found");
  }

  int32_t calOffsetMilliC = 0;
  if (sysConfigurationPartition->getConfig(
          soft_cfg_cal_offset_milli_c, strlen(soft_cfg_cal_offset_milli_c), calOffsetMilliC)) {
    printf("Calibration offset (milliDegC): %" PRId32 "\n", calOffsetMilliC);
  } else {
    printf("No calibration offset (milliDegC) found\n");
    bm_fprintf(0, soft_log, "No calibration offset (milliDegC) found\n");
    bm_printf(0, "No calibration offset (milliDegC) found");
  }
  calibrationOffsetDegC = static_cast<float>(calOffsetMilliC) * 1e-3f;

  if (userConfigurationPartition->getConfig(
          soft_cfg_reading_period_ms, strlen(soft_cfg_reading_period_ms), soft_delay_ms)) {
    printf("SOFT Delay: %" PRIu32 "ms\n", soft_delay_ms);
  } else {
    printf("SOFT Delay: Using default % " PRIu32 "ms\n", soft_delay_ms);
  }
}

void setup(void) {
  configASSERT(userConfigurationPartition);
  configASSERT(sysConfigurationPartition);
  getConfigs();
  BmSoftInitalize();
  bmSoftTopicStrLen = createBmSoftDataTopic();
  SensorWatchdog::SensorWatchdogAdd(SOFTMODULE_WATCHDOG_ID, PAYLOAD_WATCHDOG_TIMEOUT_MS,
                                    BmSoftWatchdogHandler, NO_MAX_TRIGGER, soft_log);
}

void loop(void) {
  float temperature = 0.0f;

  RTCTimeAndDate_t time_and_date = {};
  char rtcTimeBuffer[32];
  bool rtcValid = rtcGet(&time_and_date);
  if (rtcValid == pdPASS) {
    rtcPrint(rtcTimeBuffer, &time_and_date);
  };

  TSYS01::tsys01_result_e result = soft.getTemperature(temperature);
  switch (result) {
  case TSYS01::TSYS01_RESULT_OK: {
    SensorWatchdog::SensorWatchdogPet(SOFTMODULE_WATCHDOG_ID);

    printf("soft | tick: %llu, rtc: %s, temp: %f\n", uptimeGetMs(), rtcTimeBuffer, temperature);
    bm_printf(0, "soft | tick: %llu, rtc: %s, temp: %f", uptimeGetMs(), rtcTimeBuffer,
              temperature);
    static BmSoftDataMsg::Data d;
    d.header.version = 1;
    if (rtcValid) {
      d.header.reading_time_utc_ms = rtcGetMicroSeconds(&time_and_date) * 1e-3;
    }
    d.header.reading_uptime_millis = uptimeGetMs();
    d.temperature_deg_c = temperature;
    static uint8_t cbor_buf[BM_SOFT_DATA_MSG_MAX_SIZE];
    memset(cbor_buf, 0, sizeof(cbor_buf));
    size_t encoded_len = 0;
    if (BmSoftDataMsg::encode(d, cbor_buf, sizeof(cbor_buf), &encoded_len) == CborNoError) {
      bm_pub_wl(bmSoftTopic, bmSoftTopicStrLen, cbor_buf, encoded_len, 0);
    } else {
      printf("Failed to encode data message\n");
    }
    break;
  }
  case TSYS01::TSYS01_RESULT_COMMS: {
    printf("soft | tick: %llu, rtc: %s, COMMS ERROR\n", uptimeGetMs(), rtcTimeBuffer);
    bm_fprintf(0, soft_log, "soft | tick: %llu, rtc: %s,  COMMS ERROR\n", uptimeGetMs(),
               rtcTimeBuffer);
    bm_printf(0, "soft | tick: %llu, rtc: %s,  COMMS ERROR", uptimeGetMs(), rtcTimeBuffer);
    break;
  }
  case TSYS01::TSYS01_RESULT_INVALID: {
    printf("soft | tick: %llu, rtc: %s, INVALID temp: %f\n", uptimeGetMs(), rtcTimeBuffer,
           temperature);
    bm_fprintf(0, soft_log, "soft | tick: %llu, rtc: %s, INVALID temp: %f\n", uptimeGetMs(),
               rtcTimeBuffer, temperature);
    bm_printf(0, "soft | tick: %llu, rtc: %s, INVALID temp: %f", uptimeGetMs(), rtcTimeBuffer,
              temperature);
    break;
  }
  default: {
    break;
  }
  }

  // Delay between readings
  vTaskDelay(pdMS_TO_TICKS(soft_delay_ms));
}

static bool BmSoftStartAndValidate(void) {
  bool success = false;
  do {
    if (!soft.begin(calibrationOffsetDegC, serial_number, cal_time_epoch)) {
      printf("SOFT Init Failed\n");
      break;
    }
    if (!soft.getSerialNumber(serial_number)) {
      printf("SOFT Serial Number: Get SN Failed\n");
      break;
    }
    if (!sysConfigurationPartition->setConfig("tsysId", strlen("tsysId"), serial_number)) {
      printf("SOFT Serial Number: Set SN Failed\n");
      break;
    }
    success = true;
  } while (0);
  return success;
}

static bool BmSoftWatchdogHandler(void *arg) {
  (void)arg;
  printf("Resetting BM soft\n");
  if (BmSoftStartAndValidate()) {
    bm_fprintf(0, soft_log, "soft | Watchdog: Reset Bm Soft\n");
    bm_printf(0, "soft | Watchdog: Reset Bm Soft");
    printf("soft | Watchdog: Reset Bm Soft\n");
  } else {
    bm_fprintf(0, soft_log, "soft | Watchdog: Failed to reset Bm Soft\n");
    bm_printf(0, "soft | Watchdog: Failed to reset Bm Soft");
    printf("soft | Watchdog: Failed to reset Bm Soft\n");
  }
  return true;
}

static int createBmSoftDataTopic(void) {
  int topiclen = snprintf(bmSoftTopic, BM_TOPIC_MAX_LEN,
                          "sensor/%" PRIx64 "/sofar/bm_soft_temp", getNodeId());
  configASSERT(topiclen > 0);
  return topiclen;
}

static void BmSoftInitalize(void) {
  bool failed_init = false;
  while (!BmSoftStartAndValidate()) {
    if (!failed_init) {
      printf("Failed to start BM soft\n");
      bm_fprintf(0, soft_log, "Failed to start BM soft\n");
      bm_printf(0, "Failed to start BM soft");
      failed_init = true;
    }
    vTaskDelay(pdMS_TO_TICKS(BM_SOFT_PERIODIC_INIT_DELAY_MS));
  }
  if (failed_init) {
    printf("Successfully recovered & started BM soft\n");
    bm_fprintf(0, soft_log, "Successfully recovered & started BM soft\n");
    bm_printf(0, "Successfully recovered & started BM soft");
  }
}
