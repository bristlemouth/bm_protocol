#include "debug.h"
#include "util.h"
#include "user_code.h"


void setup(void) {
  /* USER CODE GOES HERE */

  /* USER CODE END */
}



void loop(void) {
  /* USER CODE GOES HERE */



  /*
    DO NOT REMOVE

    This delay is REQUIRED for the FreeRTOS task scheduler
    to allow for lower priority tasks to be serviced

    Keep this delay in the range of 100 - UINT32_MAX MS)
  */
  vTaskDelay(pdMS_TO_TICKS(1000));

  /* USER CODE END */
}
