#include "user_code.h"
#include "FreeRTOS.h"
#include "aanderaa_adcp_sensor.h"
#include "app_util.h"
#include "bsp.h"
#include "debug.h"
#include "device_info.h"
#include "pubsub.h"
#include "spotter.h"
#include "uptime.h"

static constexpr uint32_t AANDERAA_CONDUCTIVITY_DATA_MSG_MAX_SIZE = 256;
static AanderaaAdcpSensor aanderaa_sensor;

// Configurable VOUT off duration when we want to reboot the Aanderaa sensor.
// We do this to recover from FTL: Failure To Launch.
static constexpr char CFG_FTL_RECOVERY_MS[] = "ftlRecoveryMs";
static uint32_t ftl_recovery_ms = 1000;

void setup(void) {
  get_config_uint(BM_CFG_PARTITION_SYSTEM, CFG_FTL_RECOVERY_MS, strlen(CFG_FTL_RECOVERY_MS),
                  &ftl_recovery_ms);

  aanderaa_sensor.init();

  // power bleeding
  IOWrite(&DISCHARGE_ON, 1);
  IOWrite(&PL_BUCK_EN, 1);
  vTaskDelay(pdMS_TO_TICKS(ftl_recovery_ms));
  IOWrite(&VBUS_EN, 0);
  vTaskDelay(pdMS_TO_TICKS(100)); // Wait for Vbus to stabilize
  IOWrite(&DISCHARGE_ON, 0);
  IOWrite(&PL_BUCK_EN, 0);

  aanderaa_sensor.configureSensor();
  aanderaa_sensor.startStreaming(10000);
}

void loop(void) {
  // Read and handle line from sensor
  if (aanderaa_sensor.getData()) {
    debug_printf("Obtained data from aanderaa sensor\n");
  }
}
