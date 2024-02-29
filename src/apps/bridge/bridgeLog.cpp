#include "bridgeLog.h"
#include "app_pub_sub.h"
#include "bm_serial.h"
#include "device_info.h"
#include <stdio.h>
#include <string.h>

void bridgeLogPrintf(const char *str, size_t len, bridgeLogType_e type) {
  printf("%.*s", len, str);
  switch (type) {
  case BRIDGE_SYS:
    bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_BRIDGE_PRINTF_TOPIC,
                  sizeof(APP_PUB_SUB_BM_BRIDGE_PRINTF_TOPIC) - 1,
                  reinterpret_cast<const uint8_t *>(str), len,
                  APP_PUB_SUB_BM_BRIDGE_PRINTF_TYPE, APP_PUB_SUB_BM_BRIDGE_PRINTF_VERSION);
    break;
  case BRIDGE_CFG:
    bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_BRIDGE_CFG_PRINTF_TOPIC,
                  sizeof(APP_PUB_SUB_BM_BRIDGE_CFG_PRINTF_TOPIC) - 1,
                  reinterpret_cast<const uint8_t *>(str), len,
                  APP_PUB_SUB_BM_BRIDGE_CFG_PRINTF_TYPE,
                  APP_PUB_SUB_BM_BRIDGE_CFG_PRINTF_VERSION);
    break;
  default:
    printf("ERROR: Unknown log type in bridgeLogPrintf\n");
    break;
  }
}

void bridgeSensorLogPrintf(bridgeSensorLogType_e type, const char *str, size_t len) {
  if (len > 0) {
    switch (type) {
    case BM_COMMON_IND:
      printf("[%s] %.*s", "BM_COMMON_IND", len, str);
      bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_BRIDGE_SENSOR_IND_TOPIC,
                    sizeof(APP_PUB_SUB_BM_BRIDGE_SENSOR_IND_TOPIC) - 1,
                    reinterpret_cast<const uint8_t *>(str), len,
                    APP_PUB_SUB_BM_BRIDGE_SENSOR_IND_TYPE,
                    APP_PUB_SUB_BM_BRIDGE_SENSOR_IND_VERSION);
      break;
    case BM_COMMON_AGG:
      printf("[%s] %.*s", "BM_COMMON_AGG", len, str);
      bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_BRIDGE_SENSOR_AGG_TOPIC,
                    sizeof(APP_PUB_SUB_BM_BRIDGE_SENSOR_AGG_TOPIC) - 1,
                    reinterpret_cast<const uint8_t *>(str), len,
                    APP_PUB_SUB_BM_BRIDGE_SENSOR_AGG_TYPE,
                    APP_PUB_SUB_BM_BRIDGE_SENSOR_AGG_VERSION);
      break;
    default:
      printf("ERROR: Unknown log type in bridgerSensorLogPrintf\n");
      break;
    }
  }
}
