#include "task_monitor.h"

extern "C" {
#include "ll.h"
}

#include "bridgeLog.h"
#include "task_priorities.h"
#include "uptime.h"
#include <inttypes.h>
#include <stdbool.h>

typedef struct {
  const char *task_name;
  uint32_t timeout_ms;
  uint32_t last_check_in_ms;
  bool watchdog_witheld;
} TaskMonitorCtx;

static LL task_ll = {};
static BmSemaphore mutex = NULL;

static BmErr task_ll_traverse(void *data, void *arg) {
  TaskMonitorCtx *ctx = (TaskMonitorCtx *)(data);
  uint32_t uptime_ms = (uint32_t)arg;

  bool timed_out = uptime_ms - ctx->last_check_in_ms > ctx->timeout_ms;
  if (!ctx->watchdog_witheld && timed_out) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_WARNING, USE_HEADER,
                   "Task :%s has not checked in over %" PRIu32
                   " ms, last checking time: %" PRIu32 " current time: %" PRIu32
                   " withholding watchdog\n",
                   ctx->task_name, ctx->timeout_ms, ctx->last_check_in_ms, uptime_ms);
    ctx->watchdog_witheld = true;
  }

  return BmOK;
}

static void task_monitor_task(void *arg) {
  (void)arg;
  static const uint32_t task_delay_ms = 100;

  while (1) {
    uint32_t uptime_ms = uptimeGetMs();

    bm_semaphore_take(mutex, BM_MAX_DELAY_UINT32);
    ll_traverse(&task_ll, task_ll_traverse, (void *)uptime_ms);
    bm_semaphore_give(mutex);
    bm_delay(task_delay_ms);
  }
}

void task_monitor_start(void) {
  mutex = bm_semaphore_create();
  bm_semaphore_give(mutex);
  bm_task_create(task_monitor_task, "TaskMonitor", 128, NULL, TASK_MONITOR_PRIORITY, NULL);
}

BmErr task_monitor_add(BmTaskHandle handle, const char *task_name, uint32_t timeout_ms) {
  BmErr err = BmENOMEM;
  LLItem *item = NULL;
  TaskMonitorCtx ctx = {task_name, timeout_ms, uptimeGetMs(), false};

  // Do not need semaphore here as task monitor will not start until all tasks have been added
  item = ll_create_item(item, &ctx, sizeof(ctx), (uint32_t)handle);
  if (item) {
    bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                   "Adding task %s to task monitor\n", ctx.task_name);
    err = ll_item_add(&task_ll, item);
  }

  return err;
}

BmErr task_monitor_update(BmTaskHandle handle, uint32_t timeout_ms) {
  TaskMonitorCtx *ctx = NULL;
  BmErr err = BmOK;

  bm_semaphore_take(mutex, BM_MAX_DELAY_UINT32);
  err = ll_get_item(&task_ll, (uint32_t)handle, (void **)&ctx);
  if (err == BmOK && ctx) {
    ctx->timeout_ms = timeout_ms;
  }
  bm_semaphore_give(mutex);

  return err;
}

BmErr task_monitor_check_in(BmTaskHandle handle) {
  TaskMonitorCtx *ctx = NULL;
  BmErr err = BmOK;

  bm_semaphore_take(mutex, BM_MAX_DELAY_UINT32);
  err = ll_get_item(&task_ll, (uint32_t)handle, (void **)&ctx);
  if (err == BmOK && ctx) {
    ctx->last_check_in_ms = uptimeGetMs();
    if (ctx->watchdog_witheld) {
      bridgeLogPrint(BRIDGE_SYS, BM_COMMON_LOG_LEVEL_INFO, USE_HEADER,
                     "Task %s checked-in no longer withholding watchdog, time: %" PRIu32
                     " ms\n",
                     ctx->task_name, ctx->last_check_in_ms);
      ctx->watchdog_witheld = false;
    }
  }
  bm_semaphore_give(mutex);

  return err;
}
