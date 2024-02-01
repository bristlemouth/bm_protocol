#pragma once

#include <stdint.h>
#include "bm_common_pub_sub.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_PUB_SUB_UTC_TOPIC           "spotter/utc-time"
#define APP_PUB_SUB_UTC_TYPE            1
#define APP_PUB_SUB_UTC_VERSION         1


#define APP_PUB_SUB_LAST_NET_CFG_TOPIC          "spotter/request-last-network-config"
#define APP_PUB_SUB_LAST_NET_CFG_TYPE            1
#define APP_PUB_SUB_LAST_NET_CFG_VERSION         1

#define APP_PUB_SUB_PRINTF_TOPIC        "printf"
#define APP_PUB_SUB_PRINTF_TYPE         1
#define APP_PUB_SUB_PRINTF_VERSION      1

#define APP_PUB_SUB_BUTTON_TOPIC        "button"
#define APP_PUB_SUB_BUTTON_TYPE         1
#define APP_PUB_SUB_BUTTON_VERSION      1
#define APP_PUB_SUB_BUTTON_CMD_ON       "on"
#define APP_PUB_SUB_BUTTON_CMD_OFF      "off"

#define APP_PUB_SUB_BM_BRIDGE_PRINTF_TOPIC       "bridge/printf"
#define APP_PUB_SUB_BM_BRIDGE_PRINTF_TYPE        1
#define APP_PUB_SUB_BM_BRIDGE_PRINTF_VERSION     1

// TODO - keep as sensor specific logs or switch to the bm_sensor common log
#define APP_PUB_SUB_BM_BRIDGE_AANDERAA_IND_TOPIC       "bridge/aanderaa_ind"
#define APP_PUB_SUB_BM_BRIDGE_AANDERAA_IND_TYPE        1
#define APP_PUB_SUB_BM_BRIDGE_AANDERAA_IND_VERSION     1

#define APP_PUB_SUB_BM_BRIDGE_AANDERAA_AGG_TOPIC       "bridge/aanderaa_agg"
#define APP_PUB_SUB_BM_BRIDGE_AANDERAA_AGG_TYPE        1
#define APP_PUB_SUB_BM_BRIDGE_AANDERAA_AGG_VERSION     1

#define APP_PUB_SUB_BM_BRIDGE_SOFT_IND_TOPIC       "bridge/soft_ind"
#define APP_PUB_SUB_BM_BRIDGE_SOFT_IND_TYPE        1
#define APP_PUB_SUB_BM_BRIDGE_SOFT_IND_VERSION     1

#define APP_PUB_SUB_BM_BRIDGE_SOFT_AGG_TOPIC       "bridge/soft_agg"
#define APP_PUB_SUB_BM_BRIDGE_SOFT_AGG_TYPE        1
#define APP_PUB_SUB_BM_BRIDGE_SOFT_AGG_VERSION     1
// END TODO

typedef struct app_pub_sub_bm_bridge_sensor_report_data {
    uint32_t bm_config_crc32;
    size_t cbor_buffer_len;
    uint8_t cbor_buffer[0];
} __attribute__((packed)) app_pub_sub_bm_bridge_sensor_report_data_t;

#define APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_TOPIC       "bridge/sensor_report"
#define APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_TYPE        1
#define APP_PUB_SUB_BM_BRIDGE_SENSOR_REPORT_VERSION     1

#ifdef __cplusplus
}
#endif
