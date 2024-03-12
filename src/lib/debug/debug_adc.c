/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "debug.h"
#include "debug_adc.h"
#include "io_adc.h"
#include "adc.h"
#include "bsp.h"

static BaseType_t debugADCCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdADC = {
  "adc",
  "adc\n",
  debugADCCommand,
  -1
};

void debugADCInit() {
  FreeRTOS_CLIRegisterCommand( &cmdADC );
}

static uint32_t adcGetSampleMv(uint32_t channel) {
  int32_t result = 0;

  ADC_ChannelConfTypeDef config = {0};

  config.Rank = ADC_REGULAR_RANK_1;
  config.SamplingTime = ADC_SAMPLETIME_68CYCLES;
  config.SingleDiff = ADC_SINGLE_ENDED;
  config.OffsetNumber = ADC_OFFSET_NONE;
  config.Offset = 0;

  config.Channel = channel;
  IOAdcChannelConfig(&hadc1, &config);

  IOAdcReadMv(&hadc1, &result);
  return ((uint32_t)result);
}

static BaseType_t debugADCCommand(char *writeBuffer,
                                      size_t writeBufferLen,
                                      const char *commandString) {
  ( void ) writeBuffer;
  ( void ) writeBufferLen;
  ( void ) commandString;

  IOAdcInit(&hadc1);

  float vts1 = (float)adcGetSampleMv(ADC_CHANNEL_TEMPSENSOR);
  printf("VOLTAGE = %0.8f V\n", vts1);

  IOAdcDeInit(&hadc1);

  return pdFALSE;
}
