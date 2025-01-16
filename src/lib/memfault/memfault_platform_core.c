//
// Created by Genton Mo on 1/14/21.
//

#include <stdio.h>
#include <string.h>
#include "memfault_platform_core.h"

#include "bsp.h"
#include "debug.h"
#include "device_info.h"
#include "ff.h"
#include "log.h"
#include "memfault/core/build_info.h"
#include "memfault/core/compiler.h"
#include "memfault/core/log.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/core.h"
#include "memfault/core/platform/device_info.h"
#include "memfault/core/platform/system_time.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/core/trace_event.h"
#include "memfault/metrics/metrics.h"
#include "memfault/panics/coredump.h"
#include "memfault/panics/platform/coredump.h"
#include "memfault/ports/freertos.h"
#include "memfault/ports/reboot_reason.h"
#include "reset_reason.h"
#include "sd.h"
#include "stm32_rtc.h"
#include "stm32l4xx_hal.h"
#include "app_util.h"
#include "version.h"

// static sMfltCoredumpRegion s_coredump_regions[16];
static uint8_t s_event_storage[1024];
static uint8_t s_log_buf_storage[2048];

Log_t *MFLTLog;

static SemaphoreHandle_t s_memfault_packetizer_mutex;

// Note: reboot tracking needs to be placed in a noinit region
// because metadata across resets is tracked
MEMFAULT_PUT_IN_SECTION(".noinit")
static uint8_t s_reboot_tracking[MEMFAULT_REBOOT_TRACKING_REGION_SIZE];

void memfault_platform_log(eMemfaultPlatformLogLevel level, const char *fmt, ...) {
  LogLevel_t logLevel = LOG_LEVEL_INFO;
  switch(level) {
    case kMemfaultPlatformLogLevel_Debug: {
      logLevel = LOG_LEVEL_DEBUG;
      break;
    }
    case kMemfaultPlatformLogLevel_Info: {
      logLevel = LOG_LEVEL_INFO;
      break;
    }
    case kMemfaultPlatformLogLevel_Warning: {
      logLevel = LOG_LEVEL_WARNING;
      break;
    }
    case kMemfaultPlatformLogLevel_Error: {
      logLevel = LOG_LEVEL_ERROR;
      break;
    }
    default: {
      break;
    }
  }
  if(MFLTLog) {
    va_list args;
    va_start(args, fmt);
    vLogPrintf(MFLTLog, logLevel, true, fmt, args);
    logPrintf(MFLTLog, logLevel, "\n");
    va_end(args);
  }
}

BaseType_t rtcGet(RTCTimeAndDate_t *timeAndDate);
bool isRTCSet();
// uint32_t utcFromDateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
bool memfault_platform_time_get_current(sMemfaultCurrentTime *time) {
  bool rval = false;
  RTCTimeAndDate_t timeAndDate;

  if(rtcGet(&timeAndDate) == pdPASS) {
    time->type = kMemfaultCurrentTimeType_UnixEpochTimeSec;
    time->info.unix_timestamp_secs = utcFromDateTime( timeAndDate.year,
                                                      timeAndDate.month,
                                                      timeAndDate.day,
                                                      timeAndDate.hour,
                                                      timeAndDate.minute,
                                                      timeAndDate.second);
    rval = true;
  }


  return rval;
}

void memfault_platform_get_device_info(sMemfaultDeviceInfo *info) {
  // platform specific version information

  *info = (sMemfaultDeviceInfo) {
    .device_serial = getUIDStr(),
    .software_type = APP_NAME,
    .software_version = getFWVersionStr(),
    // TODO - read hwid pins
    .hardware_version = BSP_NAME,
  };
}

typedef struct RamRegions {
  uint32_t start_addr;
  size_t length;
} sRamRegions;

static const sRamRegions s_ram_regions[] = {
  {.start_addr = SRAM1_BASE, .length = SRAM1_SIZE_MAX},
  {.start_addr = SRAM2_BASE, .length = SRAM2_SIZE},
};

size_t memfault_platform_sanitize_address_range(void *start_addr, size_t desired_size) {
  for (size_t i = 0; i < MEMFAULT_ARRAY_SIZE(s_ram_regions); i++) {
    const sRamRegions *region = &s_ram_regions[i];
    const uint32_t end_addr = region->start_addr + region->length;
    if ((uint32_t)start_addr >= region->start_addr && ((uint32_t)start_addr < end_addr)) {
      return MEMFAULT_MIN(desired_size, end_addr - (uint32_t)start_addr);
    }
  }

  return 0;
}

// Linker defined symbols specifying the region to store coredumps to
//
// NOTE: The size of the regions you collect must be less than the size
// of the space reserved for saving coredumps!
extern uint32_t __CoreStart;
extern uint32_t __CoreEnd;

//! To capture the entire "RAM" region, define these symbols in the .ld script
//! of your project:
//!
//! .mflt_coredump_symbols :
//! {
//!   __MfltCoredumpRamStart = ORIGIN(RAM);
//!   __MfltCoredumpRamEnd = ORIGIN(RAM) + LENGTH(RAM);
//! } > FLASH
extern uint32_t __MfltCoredumpRamStart;
extern uint32_t __MfltCoredumpRamEnd;

#define CORE_REGION_START ((uint32_t)&__CoreStart)
#define CORE_REGION_LENGTH ((uint32_t)&__CoreEnd - CORE_REGION_START)

const sMfltCoredumpRegion *memfault_platform_coredump_get_regions(const sCoredumpCrashInfo *crash_info,
                                                                  size_t *num_regions) {
  // Let's collect the callstack at the time of crash
  static sMfltCoredumpRegion s_coredump_regions[1];

#if (MEMFAULT_PLATFORM_COREDUMP_CAPTURE_STACK_ONLY == 1)
  const void *stack_start_addr = crash_info->stack_address;
  // Capture only the interrupt stack. Use only if there is not enough storage to capture all of RAM.
  size_t stack_size = memfault_platform_sanitize_address_range((void *)stack_start_addr, MAX_STACK_SIZE);
  s_coredump_regions[0] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(stack_start_addr, stack_size);
#else
  (void)crash_info;
  // Capture all of RAM. Recommended: it enables broader post-mortem analyses, but has larger storage requirements.
  s_coredump_regions[0] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(&__MfltCoredumpRamStart,
      (uint32_t)&__MfltCoredumpRamEnd - (uint32_t)&__MfltCoredumpRamStart);
#endif

  *num_regions = MEMFAULT_ARRAY_SIZE(s_coredump_regions);

  return s_coredump_regions;
}

void memfault_platform_reboot(void) {
  memfault_platform_halt_if_debugging();
  resetSystemFromISR(RESET_REASON_MEM_FAULT);
  while (1) {} // unreachable
}

#define COREDUMP_BUFF_SIZE 1024*8
void memfault_save_raw_coredump() {
  size_t coredumpSize = 0;
  char *filename = pvPortMalloc(MAX_LOG_PATH_LEN);
  configASSERT(filename != NULL);

  if (sdIsMounted() && memfault_coredump_has_valid_coredump(&coredumpSize)) {
    uint8_t *buff = pvPortMalloc(COREDUMP_BUFF_SIZE);
    configASSERT(buff != NULL);
    size_t offset = 0;

    snprintf(filename, MAX_LOG_PATH_LEN, "log/%04ld_coredump.bin", logGetIndex());
    logPrint(SYSLog, LOG_LEVEL_INFO, "Saving coredump to %s\n", filename);

    uint32_t start = xTaskGetTickCount();

    FIL fp;
    FRESULT filRes = f_open(&fp, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if(filRes != FR_OK) {
      logPrint(SYSLog, LOG_LEVEL_ERROR, "Error opening coredump file\n");
    }

    while((coredumpSize > 0) && (filRes == FR_OK)) {
      size_t numBytes = coredumpSize;
      if (coredumpSize > COREDUMP_BUFF_SIZE) {
        numBytes = COREDUMP_BUFF_SIZE;
      }

      if(!memfault_platform_coredump_storage_read(offset, buff, numBytes)) {
        logPrint(SYSLog, LOG_LEVEL_ERROR, "Error reading coredump.\n");
        break;
      }

      UINT bw;
      filRes = f_write(&fp, buff, numBytes, &bw);
      if(filRes != FR_OK) {
        logPrint(SYSLog, LOG_LEVEL_ERROR, "Error writing to coredump file\n");
      }

      coredumpSize -= numBytes;
      offset += numBytes;
    }
    filRes = f_close(&fp);
    if(filRes != FR_OK) {
      logPrint(SYSLog, LOG_LEVEL_ERROR, "Error closing coredump file\n");
    }
    memfault_platform_coredump_storage_clear();

    logPrint(SYSLog, LOG_LEVEL_INFO, "Coredump saved. %lums\n", xTaskGetTickCount() - start);
    vPortFree(filename);
    vPortFree(buff);
  } else if (memfault_coredump_has_valid_coredump(NULL)) {
    // If there's no SD card, clear the coredump for now so it doesn't show up
    // on memfault_packetizer_get_chunk
    memfault_platform_coredump_storage_clear();
  }
}

static const sMemfaultEventStorageImpl *evt_storage = NULL;
// memfault_platform_boot() MUST be called before any any possible memfault event call
int memfault_platform_boot(void) {
  s_memfault_packetizer_mutex = xSemaphoreCreateMutex();
  configASSERT(s_memfault_packetizer_mutex != NULL);
  memfault_log_boot(s_log_buf_storage, sizeof(s_log_buf_storage));

  memfault_freertos_port_boot();

  // capture reset information
  sResetBootupInfo reset_info = { 0 };
  memfault_reboot_reason_get(&reset_info);
  memfault_reboot_tracking_boot(s_reboot_tracking, &reset_info);

  evt_storage = memfault_events_storage_boot(s_event_storage, sizeof(s_event_storage));
  memfault_trace_event_boot(evt_storage);
  memfault_reboot_tracking_collect_reset_info(evt_storage);

  sMemfaultMetricBootInfo boot_info = {
    .unexpected_reboot_count = memfault_reboot_tracking_get_crash_count(),
  };
  memfault_metrics_boot(evt_storage, &boot_info);

  return 0;
}

int memfault_platform_start(void) {
  MFLTLog = logCreate("MFLT", "log", LOG_LEVEL_INFO, LOG_DEST_ALL);
  logLoadCfg(MFLTLog, "lmflt");
  logSetBuffSize(MFLTLog, 1024);
  logSetFlushTimeout(MFLTLog, 1000); // Flush after 1 second
  logInit(MFLTLog);

  memfault_build_info_dump();

  // Save coredump if available
  memfault_save_raw_coredump();

  return 0;
}


extern uint32_t _estack;
extern uint32_t _Min_Stack_Size;

// Enable MPU protection for all FLASH writes (and MSP stack guard)
// This will protect from null pointer de-referencing, which is address 0
void memfault_enable_mpu() {
  // Let's set MEMFAULTENA so MemManage faults get tripped
  // (otherwise we will immediately get a HardFault)
  volatile uint32_t *shcsr = (uint32_t *)0xE000ED24;
  *shcsr |= (0x1 << 16);

  // From https://interrupt.memfault.com/blog/fix-bugs-and-secure-firmware-with-the-mpu
  volatile uint32_t *mpu_rbar = (uint32_t *)0xE000ED9C;
  volatile uint32_t *mpu_rasr = (uint32_t *)0xE000EDA0;
  // The STM32L496 internal flash is 1MB and starts at address 0x0
  // No need to mask the address since it's fixed and 1MB aligned
  uint32_t base_addr = 0x0;
  // Let's use the 'VALID' bit to select region 1 in 'MPU_RNR'
  *mpu_rbar = (base_addr | 1 << 4 | 1);

  // MPU_RASR settings for flash write protection:
  //  AP=0b110 to make the region read-only regardless of privilege
  //  TEXSCB=0b000010 because the Code is in "Flash memory"
  //  SIZE=19 because we want to cover 1MB
  //  ENABLE=1
  *mpu_rasr = (0b110 << 24) | (0b000010 << 16) | (19 << 1) | 0x1;


  // Now for the main stack protection...

  // Top of MSP stack (The last 32 bytes of the stack will be read-only
  // to catch overflows/overwrites)
  uint32_t guard_addr = (uint32_t)((&_estack) - ((uint32_t)&_Min_Stack_Size) /sizeof(uint32_t));

  // Go 1kB lower than the end of the stack
  guard_addr -= 1024;
   // Let's use the 'VALID' bit to select region 2 in 'MPU_RNR'
  *mpu_rbar = (guard_addr | 1 << 4 | 2);

  // MPU_RASR settings for MSP write protection:
  //  AP=0b110 to make the region read-only regardless of privilege
  //  TEXSCB=0b000110 because the Code is in "Internal SRAM"
  //  SIZE=9 because we want to cover 1024B
  //  ENABLE=1
  *mpu_rasr = (0b110 << 24) | (0b000110 << 16) | (9 << 1) | 0x1;

  // Finally, activate the MPU and default memory map (PRIVDEFENA)
  volatile uint32_t *mpu_ctrl = (uint32_t *)0xE000ED94;
  *mpu_ctrl = 0x5;
}


bool memfault_threadsafe_packetizer_get_chunk_from_source(void* chunk_buff, size_t* chunk_len, uint32_t src_mask) {
  xSemaphoreTake(s_memfault_packetizer_mutex, portMAX_DELAY);
  memfault_packetizer_set_active_sources(src_mask);
  bool success = memfault_packetizer_get_chunk(chunk_buff, chunk_len);
  xSemaphoreGive(s_memfault_packetizer_mutex);
  return success;
}

bool memfault_threadsafe_packetizer_is_data_available_from_source(uint32_t src_mask) {
  xSemaphoreTake(s_memfault_packetizer_mutex, portMAX_DELAY);
  memfault_packetizer_set_active_sources(src_mask);
  bool ret = memfault_packetizer_data_available();
  xSemaphoreGive(s_memfault_packetizer_mutex);
  return ret;
}
