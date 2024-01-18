#include "bridgeLog.h"
#include <stdio.h>
#include <string.h>
#include "app_pub_sub.h"
#include "device_info.h"
#include "bm_serial.h"

void bridgeLogPrintf(const char *str, size_t len) {
    printf("%.*s", len, str);
    bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_BRIDGE_PRINTF_TOPIC, sizeof(APP_PUB_SUB_BM_BRIDGE_PRINTF_TOPIC)-1, reinterpret_cast<const uint8_t*>(str),len, APP_PUB_SUB_BM_BRIDGE_PRINTF_TYPE, APP_PUB_SUB_BM_BRIDGE_PRINTF_VERSION);
}

void bridgeSensorLogPrintf(bridgeSensorLogType_e type, const char *str, size_t len) {
    if(len > 0){
        switch(type) {
            // TODO - keep as sensor specific logs or switch to the bm_sensor common log
            case BM_COMMON_IND:
                printf("[%s] %.*s", "BM_COMMON_IND", len, str);
                bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_BRIDGE_BM_COMMON_IND_TOPIC, sizeof(APP_PUB_SUB_BM_BRIDGE_BM_COMMON_IND_TOPIC)-1, reinterpret_cast<const uint8_t*>(str),len, APP_PUB_SUB_BM_BRIDGE_BM_COMMON_IND_TYPE, APP_PUB_SUB_BM_BRIDGE_BM_COMMON_IND_VERSION);
                break;
            case BM_COMMON_AGG:
                printf("[%s] %.*s", "BM_COMMON_AGG", len, str);
                bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_BRIDGE_BM_COMMON_AGG_TOPIC, sizeof(APP_PUB_SUB_BM_BRIDGE_BM_COMMON_AGG_TOPIC)-1, reinterpret_cast<const uint8_t*>(str),len, APP_PUB_SUB_BM_BRIDGE_BM_COMMON_AGG_TYPE, APP_PUB_SUB_BM_BRIDGE_BM_COMMON_AGG_VERSION);
                break;
            default:
                printf("ERROR: Unknown log type in bridgerSensorLogPrintf\n");
                break;
        }
    }
}
