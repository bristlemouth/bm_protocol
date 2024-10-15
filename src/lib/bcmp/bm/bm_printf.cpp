#include "FreeRTOS.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "bcmp.h"
#include "bm_printf.h"

#define MAX_FILE_NAME_LEN 64
#define MAX_STR_LEN(fname_len) (int32_t)(max_payload_len - sizeof(bm_print_publication_t) - fname_len)
static constexpr uint8_t fprintfType = 1;
static constexpr uint8_t printfType = 1;
static constexpr uint8_t fappendType = 1;

/*!
  Bristlemouth generic fprintf function, will publish the data to end in a file or
  to the console depending on if there is a file name
  \param[in] target_node_id - node_id to send to (0 = all nodes), the accept if it is
                              subscribed to the topic that the printf is publishing to
  \param[in] file_name - (optional) file name to print to (this will append to file")
  \param[in] print_time - whether or not the timestamp will be written to the file
  \param[in] *format - normal printf format string
*/
bm_printf_err_t bm_fprintf(uint64_t target_node_id, const char* file_name,
                           uint8_t print_time, const char* format, ...) {
  bm_printf_err_t rval = BM_PRINTF_OK;
  bm_print_publication_t* printf_pub = NULL;
  va_list va;
  va_start(va, format);

  do {
    // check how long the string we are printing will be
    int32_t data_len = vsnprintf(NULL, 0, format, va);
    if (data_len == 0) {
      rval = BM_PRINTF_STR_ZERO_LEN;
      break;
    }

    int32_t fname_len = 0;
    if (file_name) {
      fname_len = strnlen(file_name, MAX_FILE_NAME_LEN);
      if (fname_len == MAX_FILE_NAME_LEN) {
        rval = BM_PRINTF_FNAME_MAX_LEN;
        break;
      }
    }

    if (data_len > MAX_STR_LEN(fname_len)) {
      rval = BM_PRINTF_STR_MAX_LEN;
      break;
    }

    size_t printf_pub_len = sizeof(bm_print_publication_t) + data_len + fname_len;
    printf_pub = reinterpret_cast<bm_print_publication_t*>(pvPortMalloc(printf_pub_len));
    configASSERT(printf_pub);

    memset(printf_pub, 0, printf_pub_len);
    printf_pub->target_node_id = target_node_id;
    printf_pub->fname_len = fname_len;
    printf_pub->data_len = data_len;
    printf_pub->print_time = print_time;

    if (file_name) {
      memcpy(printf_pub->fnameAndData, file_name, fname_len);
    }

    data_len += 1; // add one so vsnprintf doesn't overwrite the last character with null
    // do this after we malloc a buffer, so the extra null after the last character doesn't print to the file/ascii terminal
    int32_t res = vsnprintf((char *)&printf_pub->fnameAndData[fname_len], data_len, format, va);
    if (res < 0 || (res != data_len - 1)) {
      rval = BM_PRINTF_MISC_ERR;
      break;
    }

    if (file_name) {
      if (!bm_pub("spotter/fprintf", printf_pub, printf_pub_len, fprintfType)) {
        rval = BM_PRINTF_TX_ERR;
      }
    } else {
      if (!bm_pub("spotter/printf", printf_pub, printf_pub_len, printfType)) {
        rval = BM_PRINTF_TX_ERR;
      }
    }
  } while (0);

  va_end(va);

  if(printf_pub){
    vPortFree(printf_pub);
    printf_pub = NULL;
  }

  return rval;
}

/*!
  Bristlemouth generic append to file,
  \param[in] target_node_id - node_id to send to (0 = all nodes), the accept if it is
                              subscribed to the topic that the printf is publishing to
  \param[in] file_name - file name to print to
  \param[in] *buff - buffer to send
*/
bm_printf_err_t bm_file_append(uint64_t target_node_id, const char* file_name, const uint8_t *buff, uint16_t len) {
  configASSERT(file_name != NULL);
  configASSERT(buff != NULL);

  bm_printf_err_t rval = BM_PRINTF_OK;

  int32_t fname_len = strnlen(file_name, MAX_FILE_NAME_LEN);

  size_t file_append_pub_len = sizeof(bm_print_publication_t) + len + fname_len;
  bm_print_publication_t* file_append_pub = reinterpret_cast<bm_print_publication_t *>(pvPortMalloc(file_append_pub_len));
  configASSERT(file_append_pub);

  file_append_pub->target_node_id = target_node_id;
  file_append_pub->fname_len = fname_len;
  file_append_pub->data_len = len;
  memcpy(file_append_pub->fnameAndData, file_name, fname_len);
  memcpy(&file_append_pub->fnameAndData[fname_len], buff, len);

  bm_pub("fappend", file_append_pub, file_append_pub_len, fappendType);

  vPortFree(file_append_pub);

  return rval;
}
