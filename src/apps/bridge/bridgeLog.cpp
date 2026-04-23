#include "bridgeLog.h"
#include "FreeRTOS.h"
#include "app_pub_sub.h"
#include "bm_os.h"
#include "bm_serial.h"
#include "device_info.h"
#include "q.h"
#include "stm32_rtc.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

#define SHARE_MESSAGE_BUF_SIZE 2048

// Message sizes rarely should come close to the shared buffer size, this should adequate
#define QUEUE_BUFFER_SIZE (2 * SHARE_MESSAGE_BUF_SIZE + 2 * sizeof(QItem))

typedef struct {
  uint8_t type;
  uint8_t version;
  uint16_t topic_length;
  uint16_t data_length;
  const char *topic;
  const uint8_t *data;
} BridgeLogPubInfo;

typedef struct {
  bool lock;              // If locked, enqueues items to send upon unlocking
  BmTaskHandle lock_task; // Task performing the locking operation
  BmSemaphore mut;
  Q message_queue;
  uint8_t message_queue_buf[QUEUE_BUFFER_SIZE];
  uint8_t shared_message_buf[SHARE_MESSAGE_BUF_SIZE];
} BridgeLogCtx;

static BridgeLogCtx ctx = {};

static BmErr bridge_log_enqueue(const BridgeLogPubInfo *info) {
  uint32_t topic_offset = sizeof(BridgeLogPubInfo);
  uint32_t data_offset = topic_offset + info->topic_length;
  uint32_t total_size = data_offset + info->data_length;

  if (total_size > SHARE_MESSAGE_BUF_SIZE) {
    return BmENOMEM;
  }

  uint8_t *buf = ctx.shared_message_buf;

  // Copy over the static parts of the struct
  memcpy(buf, info, sizeof(BridgeLogPubInfo));

  // Copy over the topic and data
  memcpy(&buf[topic_offset], info->topic, info->topic_length);
  memcpy(&buf[data_offset], info->data, info->data_length);

  BmErr err = q_enqueue(&ctx.message_queue, buf, total_size);

  return err;
}

static BmErr bridge_log_dequeue(BridgeLogPubInfo *info) {
  uint32_t topic_offset = sizeof(BridgeLogPubInfo);
  uint8_t *buf = ctx.shared_message_buf;
  uint32_t buf_size = sizeof(ctx.shared_message_buf);

  BmErr err = q_dequeue(&ctx.message_queue, buf, buf_size);
  if (err != BmOK) {
    // Clear queue in case of unexpected error
    if (err != BmENODATA) {
      q_clear(&ctx.message_queue);
    }
    return err;
  }

  // Copy static portions of the struct
  memcpy(info, buf, sizeof(BridgeLogPubInfo));

  // Assign pointers to the topic and data members
  uint32_t data_offset = topic_offset + info->topic_length;
  info->topic = reinterpret_cast<const char *>(&buf[topic_offset]);
  info->data = &buf[data_offset];

  return BmOK;
}

static void log_dequeue(void) {
  BridgeLogPubInfo info;
  // Dequeue all bridge log elements while
  while (bridge_log_dequeue(&info) == BmOK) {
    bm_serial_pub(getNodeId(), info.topic, info.topic_length, info.data, info.data_length,
                  info.type, info.version);
  }
}

static void check_lock_publish_or_enqueue(const BridgeLogPubInfo *info) {
  bool locked = ctx.lock;
  bool is_locking_task = xTaskGetCurrentTaskHandle() == ctx.lock_task;

  if (!locked || is_locking_task) {
    bm_serial_pub(getNodeId(), info->topic, info->topic_length, info->data, info->data_length,
                  info->type, info->version);
  } else {
    bridge_log_enqueue(info);
  }

  return;
}

void bridgeLogInit(void) {
  ctx.mut = bm_mutex_create();

  q_create_static(&ctx.message_queue, ctx.message_queue_buf, QUEUE_BUFFER_SIZE);
}

/*!
 @brief Lock Bridge Logging

 @details Locks other tasks from publishing log messages. This function will
          only unlock the interface if the original calling task invokes this
          function with false as the lock argument. Upon unlocking, log
          messages queued during the locked period will be dequeued immedietely.
          
 @param lock Whether bridge logging should be locked or unlocked.

 @return true if bridge log was locked or unlocked, false if it could not be
         locked or unlocked
 */
bool bridgeLogLock(bool lock) {
  BmTaskHandle current_task = xTaskGetCurrentTaskHandle();

  bm_semaphore_take(ctx.mut, BM_MAX_DELAY_UINT32);
  bool can_lock = !ctx.lock_task || current_task == ctx.lock_task;

  if (can_lock) {
    // If unlocking dequeue all buffered messages
    if (ctx.lock && !lock) {
      log_dequeue();
      ctx.lock_task = NULL;
    } else {
      ctx.lock_task = current_task;
    }
    ctx.lock = lock;
  }
  bm_semaphore_give(ctx.mut);

  return can_lock;
}

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

    BridgeLogPubInfo info = {};
    info.data = reinterpret_cast<const uint8_t *>(log_msg);
    info.data_length = msg_len;
    switch (type) {
    case BRIDGE_SYS:
      info.topic = APP_PUB_SUB_BM_BRIDGE_PRINTF_TOPIC;
      info.topic_length = sizeof(APP_PUB_SUB_BM_BRIDGE_PRINTF_TOPIC) - 1;
      info.type = APP_PUB_SUB_BM_BRIDGE_PRINTF_TYPE;
      info.version = APP_PUB_SUB_BM_BRIDGE_PRINTF_VERSION;
      break;
    case BRIDGE_CFG:
      info.topic = APP_PUB_SUB_BM_BRIDGE_CFG_PRINTF_TOPIC;
      info.topic_length = sizeof(APP_PUB_SUB_BM_BRIDGE_CFG_PRINTF_TOPIC) - 1;
      info.type = APP_PUB_SUB_BM_BRIDGE_CFG_PRINTF_TYPE;
      info.version = APP_PUB_SUB_BM_BRIDGE_CFG_PRINTF_VERSION;
      break;
    default:
      printf("ERROR: Unknown log type in bridgeLogPrintf\n");
      break;
    }

    bm_semaphore_take(ctx.mut, BM_MAX_DELAY_UINT32);
    check_lock_publish_or_enqueue(&info);
    bm_semaphore_give(ctx.mut);
  } while (0);
  if (log_msg) {
    vPortFree(log_msg);
  }
}

void bridgeSensorLogPrintf(bridgeSensorLogType_e type, const char *str, size_t len) {
  if (len > 0) {
    BridgeLogPubInfo info = {};
    info.data = reinterpret_cast<const uint8_t *>(str);
    info.data_length = len;
    switch (type) {
    case BM_COMMON_IND:
      printf("[%s] %.*s", "BM_COMMON_IND", (int)len, str);
      info.topic = APP_PUB_SUB_BM_BRIDGE_SENSOR_IND_TOPIC;
      info.topic_length = sizeof(APP_PUB_SUB_BM_BRIDGE_SENSOR_IND_TOPIC) - 1;
      info.type = APP_PUB_SUB_BM_BRIDGE_SENSOR_IND_TYPE;
      info.version = APP_PUB_SUB_BM_BRIDGE_SENSOR_IND_VERSION;
      break;
    case BM_COMMON_AGG:
      printf("[%s] %.*s", "BM_COMMON_AGG", (int)len, str);
      info.topic = APP_PUB_SUB_BM_BRIDGE_SENSOR_AGG_TOPIC;
      info.topic_length = sizeof(APP_PUB_SUB_BM_BRIDGE_SENSOR_AGG_TOPIC) - 1;
      info.type = APP_PUB_SUB_BM_BRIDGE_SENSOR_AGG_TYPE;
      info.version = APP_PUB_SUB_BM_BRIDGE_SENSOR_AGG_VERSION;
      break;
    default:
      printf("ERROR: Unknown log type in bridgerSensorLogPrintf\n");
      break;
    }

    bm_semaphore_take(ctx.mut, BM_MAX_DELAY_UINT32);
    check_lock_publish_or_enqueue(&info);
    bm_semaphore_give(ctx.mut);
  }
}
