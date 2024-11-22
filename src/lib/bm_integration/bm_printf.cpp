#include "bm_printf.h"
#include "FreeRTOS.h"
#include "bcmp.h"
#include "bm_common_pub_sub.h"
#include "spotter.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*!
  ATTENTION: This API is deprecated, please use spotter_log instead

  Bristlemouth generic fprintf function, will publish the data to end in a file or
  to the console depending on if there is a file name

  \param[in] target_node_id - node_id to send to (0 = all nodes), the accept if it is
                              subscribed to the topic that the printf is publishing to
  \param[in] file_name - (optional) file name to print to (this will append to file")
  \param[in] print_time - whether or not the timestamp will be written to the file
  \param[in] *format - normal printf format string
*/
bm_printf_err_t bm_fprintf(uint64_t target_node_id, const char *file_name, uint8_t print_time,
                           const char *format, ...) {
  bm_printf_err_t err = BM_PRINTF_OK;
  va_list args;
  printf("WARNING: bm_fprintf if deprecated, please use spotter_log instead\n");
  va_start(args, format);
  err = spotter_log(target_node_id, file_name, print_time, format, args) == BmOK
            ? BM_PRINTF_OK
            : BM_PRINTF_MISC_ERR;
  va_end(args);
  return err;
}
