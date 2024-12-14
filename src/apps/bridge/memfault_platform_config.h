//
// Important note! For some reason, changes to this file don't trigger a rebuild
// if the memfault libraries! Until that's fixed, run make clean then make
// after changing this file
//

// Message sender increases the number of tasks to 16, so we need to make
// sure memfault has enough space to keep track of them all
#define MEMFAULT_PLATFORM_MAX_TRACKED_TASKS 32

#define MEMFAULT_EXC_HANDLER_MEMORY_MANAGEMENT MemManage_Handler

#define MEMFAULT_ASSERT_HALT_IF_DEBUGGING_ENABLED 1

#define MEMFAULT_EVENT_STORAGE_READ_BATCHING_ENABLED 1
#define MEMFAULT_EVENT_STORAGE_READ_BATCHING_MAX_BYTES 320

// Send memfault metric updates every six hours
#define MEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS (6 * 60 * 60)

#define MEMFAULT_WATCHDOG_SW_TIMEOUT_SECS 30

// If you want to enable heap stats collection while debugging, uncomment the following lines
// #define MEMFAULT_COREDUMP_COLLECT_HEAP_STATS 1
// #define MEMFAULT_FREERTOS_PORT_HEAP_STATS_ENABLE 1
// #define MEMFAULT_COREDUMP_HEAP_STATS_LOCK_ENABLE 0
