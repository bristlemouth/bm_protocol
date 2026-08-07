#include "memory_metrics.h"
#include "bm_messages_helper.h"
#include "FreeRTOS.h"
#include "metrics_service.h"
#include "util.h"
#include <stddef.h>
#include <stdint.h>

#define MEMORY_COMPONENT_KEY "memory_stats"

typedef struct {
  uint32_t heap_free; // bytes currently available
  uint32_t heap_min_free; // low-water mark of available bytes (leak signal)
  uint32_t heap_largest_block; // largest contiguous free block (fragmentation)
  uint32_t heap_total_allocs;  // cumulative successful pvPortMalloc calls since boot
  uint32_t heap_total_frees;   // cumulative successful vPortFree calls since boot
} MemoryValues;

typedef struct {
  const char *name;
  BmField type;
  size_t offset; // location of the value within MemoryValues
} MemoryFieldDesc;

static const MemoryFieldDesc mem_fields[] = {
  {"heap_free", BM_FIELD_UINT32, offsetof(MemoryValues, heap_free)},
  {"heap_min_free", BM_FIELD_UINT32, offsetof(MemoryValues, heap_min_free)},
  {"heap_largest_block", BM_FIELD_UINT32, offsetof(MemoryValues, heap_largest_block)},
  {"heap_total_allocs", BM_FIELD_UINT32, offsetof(MemoryValues, heap_total_allocs)},
  {"heap_total_frees", BM_FIELD_UINT32, offsetof(MemoryValues, heap_total_frees)},
};

#define MEMORY_FIELD_COUNT array_size(mem_fields)

static MemoryValues mem_values;
static BmEncoderTableEntry mem_lut[MEMORY_FIELD_COUNT];

static BmErr memory_metrics_data(const char *metric_key, const BmEncoderTableEntry **lut,
                                 size_t *num_fields) {
  (void)metric_key;
  HeapStats_t s;
  vPortGetHeapStats(&s);
  mem_values.heap_free = (uint32_t)s.xAvailableHeapSpaceInBytes;
  mem_values.heap_min_free = (uint32_t)s.xMinimumEverFreeBytesRemaining;
  mem_values.heap_largest_block = (uint32_t)s.xSizeOfLargestFreeBlockInBytes;

  *lut = mem_lut;
  *num_fields = MEMORY_FIELD_COUNT;
  return BmOK;
}

BmErr memory_metrics_init(void) {
  for (size_t f = 0; f < MEMORY_FIELD_COUNT; f++) {
    mem_lut[f].key = mem_fields[f].name;
    mem_lut[f].type = mem_fields[f].type;
    mem_lut[f].value_source = (const uint8_t *)&mem_values + mem_fields[f].offset;
  }
  return metrics_service_add_component(MEMORY_COMPONENT_KEY, memory_metrics_data,
                                       MEMORY_FIELD_COUNT);
}
