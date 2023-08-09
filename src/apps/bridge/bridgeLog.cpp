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
