#include "FreeRTOS.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "bm_util.h"
#include "bm_pubsub.h"

typedef struct {
  uint64_t target_node_id;
  uint16_t fname_len;
  uint16_t data_len;
  uint8_t fnameAndData[0];
} __attribute__((packed)) bm_print_publication_t;

typedef enum {
  BM_PRINTF_OK,
  BM_PRINTF_STR_ZERO_LEN,
  BM_PRINTF_STR_MAX_LEN,
  BM_PRINTF_FNAME_MAX_LEN,
  BM_PRINTF_MISC_ERR,
  BM_PRINTF_TX_ERR,
} bm_printf_err_t;

bm_printf_err_t bm_fprintf(uint64_t target_node_id, const char* file_name, const char* format, ...);
bm_printf_err_t bm_file_append(uint64_t target_node_id, const char* file_name, const uint8_t *buff, uint16_t len);

#define bm_printf(target_node_id, format, ...) bm_fprintf(target_node_id, NULL, format, ##__VA_ARGS__)
