#include "user_code.h"
#include "bm_network.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "bristlefin.h"
#include "bsp.h"
#include "configuration.h"
#include "debug.h"
#include "lwip/inet.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "TSYS01.h"
#include "uptime.h"
#include "util.h"
#include "sensorWatchdog.h"

#define PAYLOAD_WATCHDOG_TIMEOUT_MS (60 * 1000)
#define BM_SOFT_WATCHDOG_MAX_TRIGGERS (3)
#define BM_SOFT_RESET_WAIT_TIME_MS (5) // https://www.digikey.com/en/htmldatasheets/production/1186938/0/0/1/g-nico-018

static constexpr char SOFTMODULE_WATCHDOG_ID[] = "softmodule";
static constexpr char soft_log[] = "soft.log";

// app_main passes a handle to the user config partition in NVM.
extern cfg::Configuration *userConfigurationPartition;

static TSYS01 soft(&spi1, &BM_CS);
static uint32_t serial_number = 0;
static uint32_t soft_delay = 5000;

static bool BmSoftWatchdogHandler(void *arg);
static bool BmSoftStartAndValidate(void);

void setup(void) {
  BmSoftStartAndValidate();
  if (userConfigurationPartition->getConfig("softReadingPeriodMs", strlen("softReadingPeriodMs"), soft_delay)) {
    printf("SOFT Delay: %" PRIu32 "ms\n", soft_delay);
  } else {
    printf("SOFT Delay: Using default % " PRIu32 "ms\n", soft_delay);
  }

  SensorWatchdog::SensorWatchdogAdd(SOFTMODULE_WATCHDOG_ID, PAYLOAD_WATCHDOG_TIMEOUT_MS,
                                    BmSoftWatchdogHandler,
                                    BM_SOFT_WATCHDOG_MAX_TRIGGERS, soft_log);
}

void loop(void) {
  float temperature = 0.0f;

  RTCTimeAndDate_t time_and_date = {};
  rtcGet(&time_and_date);
  char rtcTimeBuffer[32];
  rtcPrint(rtcTimeBuffer, &time_and_date);

  if (soft.getTemperature(temperature)) {
      printf("soft | tick: %llu, rtc: %s, temp: %f\n", uptimeGetMs(),
             rtcTimeBuffer, temperature);
      bm_fprintf(0, soft_log, "soft | tick: %llu, rtc: %s, temp: %f\n", uptimeGetMs(),
             rtcTimeBuffer, temperature);
      bm_printf(0, "soft | tick: %llu, rtc: %s, temp: %f", uptimeGetMs(),
             rtcTimeBuffer, temperature);
  } else {
    printf("soft | tick: %llu, rtc: %s, temp: %f\n", uptimeGetMs(),
             rtcTimeBuffer, NAN);
      bm_fprintf(0, soft_log, "soft | tick: %llu, rtc: %s, temp: %f\n", uptimeGetMs(),
             rtcTimeBuffer, NAN);
      bm_printf(0, "soft | tick: %llu, rtc: %s, temp: %f", uptimeGetMs(),
             rtcTimeBuffer, NAN);
  }
  // Delay between readings
  vTaskDelay(pdMS_TO_TICKS(soft_delay));
}

static bool BmSoftStartAndValidate(void) {
  bool success = false;
  do {
    if(!soft.begin()) {
      break;
    }
    if(!soft.checkPROM()) {
      printf("SOFT PROM Invalid\n");
      break;
    }
    if(!soft.getSerialNumber(serial_number)) {
      printf("SOFT Serial Number: Get SN Failed\n");
    }
    success = true;
  } while(0);
  return success;
}

static bool BmSoftWatchdogHandler(void *arg) {
  SensorWatchdog::sensor_watchdog_t *watchdog =
      reinterpret_cast<SensorWatchdog::sensor_watchdog_t *>(arg);
  printf("Resetting BM soft\n");
  if (watchdog->_triggerCount < watchdog->_max_triggers) {
    soft.reset();
    vTaskDelay(BM_SOFT_RESET_WAIT_TIME_MS);
    BmSoftStartAndValidate();
  } 
  return true;
}