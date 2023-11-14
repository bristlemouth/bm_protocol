#include "user_code.h"
#include "bm_network.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "bristlefin.h"
#include "bsp.h"
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

void setup(void) {
  // 5 second delay so we can serial in first
  vTaskDelay(5000);
  sst.begin();
  if (sst.checkPROM()) {
    printf("SST PROM Valid\n");
  } else {
    printf("SST PROM Invalid\n");
  }
  uint32_t serial_number;

  if (sst.getSerialNumber(serial_number)) {
    printf("SST Serial Number: %" PRIu32 "\n", serial_number);
  } else {
    printf("SST Serial Number: Get SN Failed\n");
  }
}

void loop(void) {
  float temperature = 0.0f;

  while(1) {
    vTaskDelay(5000);
    if (sst.getTemperature(temperature)) {
      printf("SST Temperature: %f\n", temperature);
    } else {
      printf("SST Temperature: Get Temp Failed\n");
    }
  }
}
