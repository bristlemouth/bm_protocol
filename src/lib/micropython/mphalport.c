#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

#include "serial.h"

#include "py/builtin.h"
#include "py/compile.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/mpstate.h"
#include "py/stackctrl.h"
#include "shared/runtime/gchelper.h"
#include "shared/runtime/pyexec.h"

#define RX_BUF_LEN 256
static StreamBufferHandle_t _repl_rx_stream_buffer;
static SerialHandle_t *_serial_handle;

// Set up output serial buffer and repl input stream
void mp_hal_init(void *serial_handle) {
  configASSERT(serial_handle);

  _serial_handle = (SerialHandle_t *)serial_handle;
  _repl_rx_stream_buffer = xStreamBufferCreate(RX_BUF_LEN, 1);
  configASSERT(_repl_rx_stream_buffer != NULL);
}

// Used to process bytes from a serial device and put them in the
// input stream buffer
void mp_hal_process_byte(void *serialHandle, uint8_t byte) {
  (void)serialHandle;
  configASSERT(xStreamBufferSend( _repl_rx_stream_buffer,
                          &byte,
                          sizeof(byte),
                          100));
}

// Get all the ticks!
mp_uint_t mp_hal_ticks_ms(void) {
  return xTaskGetTickCount();
}

// Delay all the milliseconds!
void mp_hal_delay_ms(mp_uint_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

// Not sure what this does
void mp_hal_set_interrupt_char(int c) {
(void)c;
}

// Receive one character from the rx stream buffer or block until
// a new character is available
int mp_hal_stdin_rx_chr(void) {
  uint8_t byte;
  xStreamBufferReceive( _repl_rx_stream_buffer,     // Stream buffer
                        &byte,                      // Where to put the data
                        sizeof(byte),               // Size of transfer
                        portMAX_DELAY);             // Wait forever
  return (int)byte;
}

// Write out string to output serial device
void mp_hal_stdout_tx_strn(const char *str, size_t len) {
 if(_serial_handle && _serial_handle->enabled && len) {
    serialWrite(_serial_handle, (const uint8_t *) str, len);
  }
}

// There is no filesystem so stat'ing returns nothing.
mp_import_stat_t mp_import_stat(const char *path) {
  return MP_IMPORT_STAT_NO_EXIST;
}

// There is no filesystem so opening a file raises an exception.
mp_lexer_t *mp_lexer_new_from_file(const char *filename) {
  mp_raise_OSError(MP_ENOENT);
}

mp_obj_t mp_builtin_open(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, mp_builtin_open);

void NORETURN __fatal_error(const char *msg) {
  configASSERT(0);
}

// Do a garbage collection cycle.
void gc_collect(void) {
  gc_collect_start();
  gc_helper_collect_regs_and_stack();
  gc_collect_end();
}
