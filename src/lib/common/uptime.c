#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "app_util.h"
#include "stm32_rtc.h"

static bool _uptimeFromRTC;
static uint64_t _startTime;

/*!
  Get system uptime in microseconds

  \return return number of microseconds system has been running
  Note - if RTC is set, effective range prior to rollover is uint64 us => 500k years.
       - if RTC is **not** set, effective range prior to rollover is uint32 ms => 50 days.
*/
uint64_t uptimeGetMicroSeconds() {
  uint64_t uptimeMicroSeconds;

  // Compute epoch from RTC or ticks
  if (_uptimeFromRTC) {
    configASSERT(isRTCSet());

    RTCTimeAndDate_t timeAndDate;
    rtcGet(&timeAndDate);
    uint64_t epoch = rtcGetMicroSeconds(&timeAndDate);
    uptimeMicroSeconds = epoch - _startTime;
  } else {
    // microseconds since powerup derived from uint32 ms ticks
    uptimeMicroSeconds = (uint64_t)(pdTICKS_TO_MS(xTaskGetTickCount()))*(uint64_t)1000;
  }

  return uptimeMicroSeconds;
}

/*!
  Get system uptime in milliseconds

  \return return number of milliseconds system has been running
*/
uint64_t uptimeGetMs() {
  return (uint64_t)( uptimeGetMicroSeconds() / (uint64_t)1000 );
}

/*!
  Get system start time
  Used when updating RTC

  \return return system start time
*/
uint64_t uptimeGetStartTime() {
  return _startTime;
}

/*!
  Initialize system uptime
*/
void uptimeInit() {
  // If the RTC is already set, set the uptime
  if (isRTCSet()) {
    RTCTimeAndDate_t timeAndDate;
    rtcGet(&timeAndDate);
    _startTime = rtcGetMicroSeconds(&timeAndDate);
    _uptimeFromRTC = true;
  } else {
    // Going to use ticks for the start time, since we don't have GPS time
    _startTime = 0;
  }
}

/*!
  Initialize system uptime.
  MUST be called after RTC is set

  This assumes RTC is set before ms ticks rolls over, which happens at ~50 days.
*/
void uptimeUpdate(uint64_t prevUptimeUS) {
  configASSERT(isRTCSet());


  RTCTimeAndDate_t timeAndDate;
  rtcGet(&timeAndDate);
  uint64_t currentTime = rtcGetMicroSeconds(&timeAndDate);

  // Account for time before getting RTC.
  _startTime = currentTime - prevUptimeUS;
  _uptimeFromRTC = true;
}
