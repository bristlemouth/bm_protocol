#pragma once

#include <stdint.h>
#include "bm_common_pub_sub.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_PUB_SUB_UTC_TOPIC           "spotter/utc-time"
#define APP_PUB_SUB_UTC_TYPE            1
#define APP_PUB_SUB_UTC_VERSION         1

#define APP_PUB_SUB_BUTTON_TOPIC        "button"
#define APP_PUB_SUB_BUTTON_TYPE         1
#define APP_PUB_SUB_BUTTON_VERSION      1
#define APP_PUB_SUB_BUTTON_CMD_ON       "on"
#define APP_PUB_SUB_BUTTON_CMD_OFF      "off"

#define APP_PUB_SUB_BM_PRINTF_TOPIC     "spotter_log_console"
#define APP_PUB_SUB_BM_PRINTF_TYPE      1
#define APP_PUB_SUB_BM_PRINTF_VERSION   1

#ifdef __cplusplus
}
#endif
