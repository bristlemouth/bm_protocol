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

// app_main passes a handle to the user config partition in NVM.
extern cfg::Configuration *userConfigurationPartition;

TSYS01 sst(&spi1, &BM_CS);

uint32_t sst_delay = 5000;

void setup(void) {
  // 5 second delay so we can usb-serial in and see the output
  vTaskDelay(5000);
  sst.begin();
  if (sst.checkPROM()) {
    printf("SOFT PROM Valid\n");
  } else {
    printf("SOFT PROM Invalid\n");
  }
  uint32_t serial_number;

  if (sst.getSerialNumber(serial_number)) {
    printf("SOFT Serial Number: %" PRIu32 "\n", serial_number);
  } else {
    printf("SOFT Serial Number: Get SN Failed\n");
  }
  uint32_t sst_delay_s = 0;
  if (userConfigurationPartition->getConfig("softReadingPeriodS", strlen("softReadingPeriodS"), sst_delay_s)) {
    sst_delay = sst_delay_s * 1000;
    printf("SOFT Delay: %" PRIu32 "ms\n", sst_delay);
  } else {
    printf("SOFT Delay: Using default % " PRIu32 "ms\n", sst_delay);
  }

}

void loop(void) {
  float temperature = 0.0f;

  RTCTimeAndDate_t time_and_date = {};
  rtcGet(&time_and_date);
  char rtcTimeBuffer[32];
  rtcPrint(rtcTimeBuffer, &time_and_date);

  if (sst.getTemperature(temperature)) {
      printf("soft | tick: %llu, rtc: %s, temp: %f\n", uptimeGetMs(),
             rtcTimeBuffer, temperature);
      bm_fprintf(0, "soft.log", "soft | tick: %llu, rtc: %s, temp: %f\n", uptimeGetMs(),
             rtcTimeBuffer, temperature);
      bm_printf(0, "soft | tick: %llu, rtc: %s, temp: %f", uptimeGetMs(),
             rtcTimeBuffer, temperature);
  } else {
    printf("soft | tick: %llu, rtc: %s, temp: %f\n", uptimeGetMs(),
             rtcTimeBuffer, NAN);
      bm_fprintf(0, "soft.log", "soft | tick: %llu, rtc: %s, temp: %f\n", uptimeGetMs(),
             rtcTimeBuffer, NAN);
      bm_printf(0, "soft | tick: %llu, rtc: %s, temp: %f", uptimeGetMs(),
             rtcTimeBuffer, NAN);
  }
  // Delay between readings
  vTaskDelay(pdMS_TO_TICKS(sst_delay));
}
