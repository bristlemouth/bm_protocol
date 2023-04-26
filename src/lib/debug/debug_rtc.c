/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "debug.h"
#include "debug_rtc.h"
#include "stm32_rtc.h"
#include "uptime.h"

static BaseType_t debugRTCCommand( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdRTC = {
  // Command string
  "rtc",
  // Help string
  "rtc:\n"
  " * rtc - get the rtc\n"
  " * rtc micro - get the rtc in microseconds\n"
  " * rtc set <YYYY MM DD hh mm ss>\n",
  debugRTCCommand,
  -1
};

void debugRTCInit() {
  FreeRTOS_CLIRegisterCommand( &cmdRTC );
}


static BaseType_t debugRTCCommand(char *writeBuffer,
                                      size_t writeBufferLen,
                                      const char *commandString) {
  BaseType_t parameterStringLength;

  ( void ) writeBuffer;
  ( void ) writeBufferLen;

  do {
    const char *parameter;
    parameter = FreeRTOS_CLIGetParameter(
                    commandString,
                    1, // Get the first parameter (command)
                    &parameterStringLength);

    if(parameter == NULL) {
      RTCTimeAndDate_t timeAndDate;
      if (rtcGet(&timeAndDate) == pdPASS) {
        printf("OK %04d-%02d-%02dT%02d:%02d:%02d\n",
                      timeAndDate.year,
                      timeAndDate.month,
                      timeAndDate.day,
                      timeAndDate.hour,
                      timeAndDate.minute,
                      timeAndDate.second);
      } else {
        printf("ERR Time not set\n");
      }
      break;
    }

    if (strncmp("set", parameter, parameterStringLength) == 0) {
      bool bTimeSetOk = false;
      RTCTimeAndDate_t timeAndDate;
      RTCTimeAndDate_t prevTimeAndDate = {0,0,0,0,0,0,0};

      do {

        parameter = FreeRTOS_CLIGetParameter(
                        commandString, 2, &parameterStringLength);
        if(parameter == NULL) {
          printf("ERR Invalid paramters\n");
          break;
        }
        timeAndDate.year = strtoul(parameter, NULL, 10);

        parameter = FreeRTOS_CLIGetParameter(
                        commandString, 3, &parameterStringLength);
        if(parameter == NULL) {
          printf("ERR Invalid paramters\n");
          break;
        }
        timeAndDate.month = strtoul(parameter, NULL, 10);

        parameter = FreeRTOS_CLIGetParameter(
                        commandString, 4, &parameterStringLength);
        if(parameter == NULL) {
          printf("ERR Invalid paramters\n");
          break;
        }
        timeAndDate.day = strtoul(parameter, NULL, 10);

        parameter = FreeRTOS_CLIGetParameter(
                        commandString, 5, &parameterStringLength);
        if(parameter == NULL) {
          printf("ERR Invalid paramters\n");
          break;
        }
        timeAndDate.hour = strtoul(parameter, NULL, 10);

        parameter = FreeRTOS_CLIGetParameter(
                        commandString, 6, &parameterStringLength);
        if(parameter == NULL) {
          printf("ERR Invalid paramters\n");
          break;
        }
        timeAndDate.minute = strtoul(parameter, NULL, 10);

        parameter = FreeRTOS_CLIGetParameter(
                        commandString, 7, &parameterStringLength);
        if(parameter == NULL) {
          printf("ERR Invalid paramters\n");
          break;
        }
        timeAndDate.second = strtoul(parameter, NULL, 10);
        uint64_t prevUptimeUS = uptimeGetMicroSeconds();
        rtcGet(&prevTimeAndDate);
        printf("Manually updating RTC from ");
        printf("%04d-%02d-%02dT%02d:%02d:%02d",
                      prevTimeAndDate.year,
                      prevTimeAndDate.month,
                      prevTimeAndDate.day,
                      prevTimeAndDate.hour,
                      prevTimeAndDate.minute,
                      prevTimeAndDate.second);
        printf(" to %04d-%02d-%02dT%02d:%02d:%02d\n",
                      timeAndDate.year,
                      timeAndDate.month,
                      timeAndDate.day,
                      timeAndDate.hour,
                      timeAndDate.minute,
                      timeAndDate.second);

        if(rtcSet(&timeAndDate) == pdPASS) {
          bTimeSetOk = true;
          uptimeUpdate(prevUptimeUS);
        }

      } while (0);

      if(bTimeSetOk) {
        printf("OK %04d-%02d-%02dT%02d:%02d:%02d\n",
                      timeAndDate.year,
                      timeAndDate.month,
                      timeAndDate.day,
                      timeAndDate.hour,
                      timeAndDate.minute,
                      timeAndDate.second);
      } else {
        printf("ERR unable to set rtc.\n");
      }

    } else if (strncmp("micro", parameter, parameterStringLength) == 0) {
      if(isRTCSet()){
        RTCTimeAndDate_t timeAndDate;
        BaseType_t status = rtcGet(&timeAndDate);
        if(status != pdPASS){
          printf("ERR Failed to get RTC\n");
          break;
        }
        uint64_t rtcMicro = rtcGetMicroSeconds(&timeAndDate);
        printf("%" PRIu64 "\n", rtcMicro);
      } else {
        printf("ERR RTC not set\n");
        break;
      }
    } else {
      printf("ERR Invalid paramters\n");
      break;
    }

  } while(0);
  return pdFALSE;
}
