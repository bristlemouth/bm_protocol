#include "bridgeLog.h"
#include "FreeRTOS.h"
#include "app_pub_sub.h"
#include "bm_serial.h"
#include "device_info.h"
#include "stm32_rtc.h"
#include <stdio.h>
#include <string.h>

void bridgeLogPrint(bridgeLogType_e type, BmLogLevel level, bool print_header,
                    const char *format, ...) {
  va_list va_args;
  va_start(va_args, format);
  vBridgeLogPrint(type, level, print_header, format, va_args);
  va_end(va_args);
}

void vBridgeLogPrint(bridgeLogType_e type, BmLogLevel level, bool print_header,
                     const char *format, va_list va_args) {
  BmLog *log_msg = NULL;
  size_t msg_len = 0;
  do {
    int print_size = vsnprintf(nullptr, 0, format, va_args);
    if (print_size < 0) {
      static constexpr char error_msg[] = "vsnprintf failed in bridgeLogPrint\n";
      msg_len = sizeof(BmLog) + sizeof(error_msg);
      log_msg = static_cast<BmLog *>(pvPortMalloc(msg_len));
      configASSERT(log_msg);
      memset(log_msg, 0, msg_len);
      log_msg->level = BM_COMMON_LOG_LEVEL_ERROR;
      log_msg->message_length = sizeof(error_msg);
      log_msg->print_header = true;
      memcpy(log_msg->message, error_msg, sizeof(error_msg));
    } else {
      print_size++; // Add 1 for null terminator
      msg_len = sizeof(BmLog) + print_size;
      log_msg = static_cast<BmLog *>(pvPortMalloc(msg_len));
      configASSERT(log_msg);
      memset(log_msg, 0, msg_len);
      log_msg->level = level;
      log_msg->message_length = print_size;
      log_msg->print_header = print_header;
      vsnprintf(log_msg->message, print_size, format, va_args);
    }
    RTCTimeAndDate_t datetime;
    if (rtcGet(&datetime) == pdPASS) {
      log_msg->timestamp_utc_s = rtcGetMicroSeconds(&datetime) * 1e-6;
    }
    printf("%s", log_msg->message);
    switch (type) {
    case BRIDGE_SYS:
      bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_BRIDGE_PRINTF_TOPIC,
                    sizeof(APP_PUB_SUB_BM_BRIDGE_PRINTF_TOPIC) - 1,
                    reinterpret_cast<const uint8_t *>(log_msg), msg_len,
                    APP_PUB_SUB_BM_BRIDGE_PRINTF_TYPE, APP_PUB_SUB_BM_BRIDGE_PRINTF_VERSION);
      break;
    case BRIDGE_CFG:
      bm_serial_pub(getNodeId(), APP_PUB_SUB_BM_BRIDGE_CFG_PRINTF_TOPIC,
                    sizeof(APP_PUB_SUB_BM_BRIDGE_CFG_PRINTF_TOPIC) - 1,
                    reinterpret_cast<const uint8_t *>(log_msg), msg_len,
                    APP_PUB_SUB_BM_BRIDGE_CFG_PRINTF_TYPE,
                    APP_PUB_SUB_BM_BRIDGE_CFG_PRINTF_VERSION);
      break;
    default:
      printf("ERROR: Unknown log type in bridgeLogPrintf\n");
      break;
    }
  } while (0);
  if (log_msg) {
    vPortFree(log_msg);
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
