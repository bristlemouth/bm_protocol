#ifndef __TASK_MONITOR_H__
#define __TASK_MONITOR_H__

#include "bm_os.h"
#include "util.h"
#include <stdint.h>

void task_monitor_start(void);
BmErr task_monitor_add(BmTaskHandle handle, const char *task_name, uint32_t timeout_ms);
BmErr task_monitor_update(BmTaskHandle handle, uint32_t timeout_ms);
BmErr task_monitor_check_in(BmTaskHandle handle);

#endif
