#include "stm32_rtc.h"
#include "stm32u5xx_ll_rcc.h"
#include "app_util.h"
#include <stdio.h>
#include <string.h>

// Magic number to write into backup register 0 to indicate that the rtc is set
#define RTC_SET_MAGIC 0x836A20DD

static const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31}; // API starts months from 1, this array starts from 0

BaseType_t rtcInit() {
  BaseType_t rval = pdPASS;

  // Force enable RTCAPB clock (bootloader disables it so LL_RTC_Init hangs)
  // See ST forum for more info:
  // https://community.st.com/s/question/0D50X00009XkfyW/stm32l432kc-rtc-init-fails-after-dfu
  __HAL_RCC_RTCAPB_CLK_ENABLE();

  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  configASSERT(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) == HAL_OK);

  // Enable RTC Clock
  LL_RCC_EnableRTC();

  // {Hour format, asynch prescaler, synch prescaler}
  LL_RTC_InitTypeDef RTC_InitStruct = {LL_RTC_HOURFORMAT_24HOUR, 127, 255};
  if(LL_RTC_Init(RTC, &RTC_InitStruct) != SUCCESS) {
    rval = pdFAIL;
  }

  return rval;
}

bool isRTCSet() {
  return (LL_RTC_BKP_GetRegister(RTC, LL_RTC_BKP_DR0) == RTC_SET_MAGIC);
}

/*!
  Check current RTC against a received time to make sure it's within the valid drift threshold

  \param[in] receivedTimeAndDate - the current time and date parsed from a received message
  \param[in] driftThesholdS - maximum drift, in seconds
  \param[out] deltaS - Delta seconds from previous time to current time (absolute value)
  \return true if RTC time within maximum allowed drift, false if not
*/
bool isRTCValid(RTCTimeAndDate_t *receivedTimeAndDate, uint32_t driftThesholdS, uint32_t *deltaS) {
  if (!isRTCSet()) {
    return false;
  }

  RTCTimeAndDate_t rtcTimeAndDate = {0};
  BaseType_t status = rtcGet(&rtcTimeAndDate);
  if (status != pdPASS) {
    return false;
  }

  uint64_t rtcTimeMicroSeconds = rtcGetMicroSeconds(&rtcTimeAndDate);
  uint64_t receivedTimeMicroSeconds = rtcGetMicroSeconds(receivedTimeAndDate);

  // Make sure the microseconds received are within +/- driftThesholdS of current RTC value
  if (receivedTimeMicroSeconds > rtcTimeMicroSeconds) {
    if (deltaS != NULL) {
      *deltaS = (int64_t)(receivedTimeMicroSeconds - rtcTimeMicroSeconds);
    }
    return (receivedTimeMicroSeconds - rtcTimeMicroSeconds) <= ((uint64_t)driftThesholdS * 1e6);
  } else {
    if (deltaS != NULL) {
      *deltaS = (int64_t)(rtcTimeMicroSeconds - receivedTimeMicroSeconds);
    }
    return (rtcTimeMicroSeconds - receivedTimeMicroSeconds ) <= ((uint64_t)driftThesholdS * 1e6);
  }
}

BaseType_t rtcGet(RTCTimeAndDate_t *timeAndDate) {
  BaseType_t rval = pdPASS;
  configASSERT(timeAndDate != NULL);

  if(isRTCSet()) {
    uint32_t subSecond = LL_RTC_TIME_GetSubSecond(RTC);
    uint32_t prediv = LL_RTC_GetSynchPrescaler(RTC);
    timeAndDate->second = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetSecond(RTC));
    timeAndDate->minute = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetMinute(RTC));
    timeAndDate->hour = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetHour(RTC));

    timeAndDate->day = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetDay(RTC));
    timeAndDate->month = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetMonth(RTC));

    // Year is stored as only 2 digits :(
    timeAndDate->year = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetYear(RTC)) + 2000;

    if(prediv >= subSecond) {
      timeAndDate->ms = (1000 * (prediv-subSecond))/(prediv+1);
    } else {
      timeAndDate->ms = 0;
    }
  } else {
    rval = pdFAIL;
  }

  return rval;
}

BaseType_t rtcPrint(char* buffer, RTCTimeAndDate_t* timeAndDate) {
  RTCTimeAndDate_t rtc = {};
  if (timeAndDate == NULL) {
    rtcGet(&rtc);
  } else {
    memcpy(&rtc, timeAndDate, sizeof(RTCTimeAndDate_t));
  }
  if (rtc.year != 0) {
    return sprintf(buffer, "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
            rtc.year,
            rtc.month,
            rtc.day,
            rtc.hour,
            rtc.minute,
            rtc.second,
            rtc.ms);
  } else {
    strcpy(buffer, "0");
    return 0;
  }
}

BaseType_t rtcSet(const RTCTimeAndDate_t *timeAndDate) {
  BaseType_t rval = pdPASS;

  configASSERT(timeAndDate != NULL);

  LL_RTC_TimeTypeDef RTC_TimeStruct;
  LL_RTC_DateTypeDef RTC_DateStruct;

  RTC_TimeStruct.TimeFormat = LL_RTC_TIME_FORMAT_AM_OR_24;
  RTC_TimeStruct.Hours = timeAndDate->hour;
  RTC_TimeStruct.Minutes = timeAndDate->minute;
  RTC_TimeStruct.Seconds = timeAndDate->second;

  if (LL_RTC_TIME_Init(RTC, LL_RTC_FORMAT_BIN, &RTC_TimeStruct) != SUCCESS) {
    rval = pdFAIL;
  }

  RTC_DateStruct.WeekDay = LL_RTC_WEEKDAY_MONDAY;
  RTC_DateStruct.Month = timeAndDate->month;
  RTC_DateStruct.Day = timeAndDate->day;

  // Year is stored as only 2 digits :(
  RTC_DateStruct.Year = timeAndDate->year - 2000;

  if(LL_RTC_DATE_Init(RTC, LL_RTC_FORMAT_BIN, &RTC_DateStruct) != SUCCESS) {
    rval = pdFAIL;
  }

  if(rval == pdPASS) {
    LL_RTC_BKP_SetRegister(RTC, LL_RTC_BKP_DR0, RTC_SET_MAGIC);
  }

  return rval;
}

uint64_t rtcGetMicroSeconds(RTCTimeAndDate_t *timeAndDate){
  int i;
  uint64_t microseconds = 0;

  if (!isRTCSet()) {
    return microseconds;
  }

  // microseconds from 1970 till 1 jan 00:00:00 of the given year
  microseconds = (timeAndDate->year - 1970) * (SECS_PER_DAY * 365);
  for (i = 1970; i < timeAndDate->year; i++) {
    if (LEAP_YEAR(i - 1970)) {
      microseconds +=  SECS_PER_DAY;   // add extra days for leap years
    }
  }

  // add days for this year, months start from 1
  for (i = 1; i < timeAndDate->month; i++) {
    if ( (i == 2) && LEAP_YEAR(timeAndDate->year - 1970)) {
      microseconds += SECS_PER_DAY * 29;
    } else {
      microseconds += SECS_PER_DAY * monthDays[i-1];  //monthDay array starts from 0
    }
  }
  microseconds += (timeAndDate->day-1) * SECS_PER_DAY;
  microseconds += timeAndDate->hour * SECS_PER_HOUR;
  microseconds += timeAndDate->minute * SECS_PER_MIN;
  microseconds += timeAndDate->second;
  microseconds *= 1e6;
  microseconds += (uint64_t)timeAndDate->ms * 1000;
  return microseconds;
}

uint32_t getCALM(){
  return LL_RTC_CAL_GetMinus(RTC);
}

uint32_t getCALP(){
  return LL_RTC_CAL_IsPulseInserted(RTC);
}

uint32_t getRECALP(){
  return LL_RTC_IsActiveFlag_RECALP(RTC);
}

uint32_t setCALM(uint32_t calm){
  LL_RTC_DisableWriteProtection(RTC);
  LL_RTC_CAL_SetMinus(RTC, calm);
  LL_RTC_EnableWriteProtection(RTC);
  return 0;
}

uint32_t setCALP(uint32_t calp){
  LL_RTC_DisableWriteProtection(RTC);
  if(calp){
    LL_RTC_CAL_SetPulse(RTC, LL_RTC_CALIB_INSERTPULSE_SET);
  }
  else{
    LL_RTC_CAL_SetPulse(RTC,LL_RTC_CALIB_INSERTPULSE_NONE);
  }
  LL_RTC_EnableWriteProtection(RTC);
  return 0;
}

/*!
  Get current time as string (RTC date if present, system ticks otherwise)

  \param[out] *timeStr Buffer in which to store the string
  \param[in] len size of string buffer
  \param[in] epoch use unix time if RTC is present
  \return false if string does not fit in buffer, true otherwise
*/
bool logRtcGetTimeStr(char *timeStr, size_t len, bool epoch) {
  RTCTimeAndDate_t timeAndDate;
  size_t numChars = 0;
  if (rtcGet(&timeAndDate) == pdPASS) {
    if(epoch) {
      uint32_t time = utcFromDateTime(timeAndDate.year,
                                      timeAndDate.month,
                                      timeAndDate.day,
                                      timeAndDate.hour,
                                      timeAndDate.minute,
                                      timeAndDate.second);

      numChars = snprintf(timeStr, len, "%lu.%03d", time, timeAndDate.ms);
    } else {
      numChars = snprintf(timeStr, len, "%04d-%02d-%02dT%02d:%02d:%02d.%03uZ",
                          timeAndDate.year,
                          timeAndDate.month,
                          timeAndDate.day,
                          timeAndDate.hour,
                          timeAndDate.minute,
                          timeAndDate.second,
                          timeAndDate.ms);
    }
  } else {
    numChars = snprintf(timeStr, len, "%lut", xTaskGetTickCount());
  }

  return (numChars <= len);
}
