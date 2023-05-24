#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include "task.h"
#include "task_priorities.h"
#include "shared/runtime/gchelper.h"
#include "shared/runtime/pyexec.h"
#include "micropython_freertos.h"

#include "py/builtin.h"
#include "py/compile.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/mpstate.h"
#include "py/stackctrl.h"

#include "mphalport.h"

#define MICROPYTHON_HEAP_SIZE (1024*128)

// static TaskHandle_t micropython_task_handle;
// static void micropython_task(void *parameters);
static SerialHandle_t *_serial_handle;

// Using micropython specific heap for now
// TODO - put this in separate memory area with linker
static uint8_t upy_heap[MICROPYTHON_HEAP_SIZE];

//
// Commented out while we use the REPL for testing
//
// static void do_str(const char *src, mp_parse_input_kind_t input_kind) {
//   nlr_buf_t nlr;
//   if (nlr_push(&nlr) == 0) {
//     // Compile, parse and execute the given string.
//     mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, src, strlen(src), 0);
//     qstr source_name = lex->source_name;
//     mp_parse_tree_t parse_tree = mp_parse(lex, input_kind);
//     mp_obj_t module_fun = mp_compile(&parse_tree, source_name, true);
//     mp_call_function_0(module_fun);
//     nlr_pop();
//   } else {
//     // Uncaught exception: print it out.
//     mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
//   }
// }

// Called if an exception is raised outside all C exception-catching handlers.
void nlr_jump_fail(void *val) {
  for (;;) {
    configASSERT(0);
  }
}

#ifndef NDEBUG
// Used when debugging is enabled.
void MP_WEAK __assert_func(const char *file, int line, const char *func, const char *expr) {
  for (;;) {
    configASSERT(0);
  }
}
#endif

static BaseType_t repl_command( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {

  // Remove unused argument warnings
  ( void ) commandString;
  ( void ) writeBuffer;
  ( void ) writeBufferLen;

  printf("Starting REPL\n");
  void (*backup_process_byte)(void *serialHandle, uint8_t byte) = NULL;

  // Save serial consol process byte handler for later
  backup_process_byte = _serial_handle->processByte;

  // Hijack process byte handler from CLI
  _serial_handle->processByte = mp_hal_process_byte;

  // Setup fun
  mp_stack_ctrl_init();
  gc_init(upy_heap, upy_heap + sizeof(upy_heap));
  mp_init();

  // Start a normal REPL; will exit when ctrl-D is entered on a blank line.
  pyexec_friendly_repl();

  // Cleanup!
  gc_sweep_all();
  mp_deinit();

  // Restore serial console process byte handler
  _serial_handle->processByte = backup_process_byte;

  printf("Stopping REPL\n");
  return pdFALSE;
}

static const CLI_Command_Definition_t cmd_repl = {
  // Command string
  "repl",
  // Help string
  "repl:\n Start python repl\n",
  // Command function
  repl_command,
  // Number of parameters
  0
};

/*!
  Initialize micropython stuff. Currently just sets up repl CLI command and
  micropython HAL

  \return true if successful false otherwise
*/
bool micropython_freertos_init(SerialHandle_t *serial_handle) {

  _serial_handle = serial_handle;
  mp_hal_init(serial_handle);

  FreeRTOS_CLIRegisterCommand( &cmd_repl );

  //
  // Will use a task to automatically run a file later
  //
  // BaseType_t rval = xTaskCreate(
  //   micropython_task,
  //   "micropython",
  //   16*1024, // TODO - check this
  //   NULL,
  //   MICROPYTHON_TASK_PRIORITY,
  //   &micropython_task_handle);

  // configASSERT(rval == pdTRUE);
  return true;
}

// static void micropython_task(void *parameters) {
//   (void)parameters;

//   while(1) {
//     mp_stack_ctrl_init();
//     gc_init(upy_heap, upy_heap + sizeof(upy_heap));
//     mp_init();
//     do_str(demo_file_input, MP_PARSE_FILE_INPUT);
//     gc_sweep_all();
//     mp_deinit();
//     vTaskDelay(1000);
//   }
// }
