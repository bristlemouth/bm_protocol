#include "FreeRTOS.h"
#include "pubsub.h"
#include "util.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USE_TIMESTAMP 1
#define NO_TIMESTAMP 0

typedef enum {
  BM_PRINTF_OK,
  BM_PRINTF_STR_ZERO_LEN,
  BM_PRINTF_STR_MAX_LEN,
  BM_PRINTF_FNAME_MAX_LEN,
  BM_PRINTF_MISC_ERR,
  BM_PRINTF_TX_ERR,
} bm_printf_err_t;

bm_printf_err_t bm_fprintf(uint64_t target_node_id, const char *file_name, uint8_t print_time,
                           const char *format, ...)
    __attribute__((deprecated("Please use spotter_log instead")));

#define bm_printf(target_node_id, format, ...)                                                 \
  bm_fprintf(target_node_id, NULL, USE_TIMESTAMP, format, ##__VA_ARGS__)
