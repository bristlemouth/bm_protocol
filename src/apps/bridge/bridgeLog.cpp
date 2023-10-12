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
        printf("[%s] %.*s", (type == AANDERAA_IND) ? "AANDERAA_IND" : "AANDERAA_AGG", len, str);
        switch(type) {
            case AANDERAA_IND:
                bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_BRIDGE_SENSOR_IND_TOPIC, sizeof(APP_PUB_SUB_BM_BRIDGE_SENSOR_IND_TOPIC)-1, reinterpret_cast<const uint8_t*>(str),len, APP_PUB_SUB_BM_BRIDGE_SENSOR_IND_TYPE, APP_PUB_SUB_BM_BRIDGE_SENSOR_IND_VERSION);
                break;
            case AANDERAA_AGG:
                bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_BRIDGE_SENSOR_AGG_TOPIC, sizeof(APP_PUB_SUB_BM_BRIDGE_SENSOR_AGG_TOPIC)-1, reinterpret_cast<const uint8_t*>(str),len, APP_PUB_SUB_BM_BRIDGE_SENSOR_AGG_TYPE, APP_PUB_SUB_BM_BRIDGE_SENSOR_AGG_VERSION);
                break;
            default:
                printf("ERROR: Unknown sensor type in bridgerSensorLogPrintf\n");
                break;
        }
    }
}
