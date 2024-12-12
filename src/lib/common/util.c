#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "app_util.h"

#define MAX_FRACTION_LEN 45

/*!
  Convert string to float

  \param[in] numString number string
  \param[out] *fVal pointer to result float
  \return True if conversion successful, false otherwise

  Note: Leading spaces are OK, trailing spaces are not. (Must be \0 delimited string)
  Scientific/engineering notation is NOT supported
*/
bool bStrtof(char *numStr, float *fVal) {
  bool bRval = false;
  char *endPtr;
  configASSERT(numStr != NULL);
  configASSERT(fVal != NULL);

  // Walk through leading spaces
  while(*numStr == ' ') {
    numStr++;
  }

  bool isNegative = (numStr[0] == '-');

  do {

    *fVal = (float)strtoll(numStr, &endPtr, 10);

    // This also takes care of strings starting with .
    if(*endPtr == '.') {
      char *fractionStart = &endPtr[1];
      float fFraction = (float)strtoll(fractionStart, &endPtr, 10);

      if(endPtr == fractionStart || *endPtr != 0) {
        // No number was processed
        break;
      }
      int32_t numDigits = strnlen(fractionStart, MAX_FRACTION_LEN);
      while(numDigits > 0) {
        fFraction /= 10.0;
        numDigits--;
      }

      if(isNegative) {
        *fVal -= fFraction;
      } else {
        *fVal += fFraction;
      }
      bRval = true;

    } else if (endPtr == numStr) {
      // No number was processed
      break;
    } else {
      // No decimal point is ok
      bRval = true;
    }

  } while(0);

  return bRval;
}


/*!
  Convert string to double

  \param[in] numString number string
  \param[out] *fVal pointer to result double
  \return True if conversion successful, false otherwise

  Note: Leading spaces are OK, trailing spaces are not. (Must be \0 delimited string)
  Scientific/engineering notation is NOT supported
*/
bool bStrtod(char *numStr, double *dVal) {
  bool bRval = false;
  char *endPtr;
  configASSERT(numStr != NULL);
  configASSERT(dVal != NULL);

  // Walk through leading spaces
  while(*numStr == ' ') {
    numStr++;
  }

  bool isNegative = (numStr[0] == '-');

  do {

    *dVal = (double)strtoll(numStr, &endPtr, 10);

    // This also takes care of strings starting with .
    if(*endPtr == '.') {
      char *fractionStart = &endPtr[1];
      double fFraction = (double)strtoll(fractionStart, &endPtr, 10);

      if(endPtr == fractionStart || *endPtr != 0) {
        // No number was processed
        break;
      }
      int32_t numDigits = strnlen(fractionStart, MAX_FRACTION_LEN);
      while(numDigits > 0) {
        fFraction /= 10.0;
        numDigits--;
      }

      if(isNegative) {
        *dVal -= fFraction;
      } else {
        *dVal += fFraction;
      }
      bRval = true;

    } else if (endPtr == numStr) {
      // No number was processed
      break;
    } else {
      // No decimal point is ok
      bRval = true;
    }

  } while(0);

  return bRval;
}
// leap year calulator expects year argument as years offset from 1970
#define LEAP_YEAR(Y)     ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )

#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24UL)
#define MICROSECONDS_PER_SECOND (1000000)

static const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31}; // API starts months from 1, this array starts from 0
static const uint8_t monthDaysLeapYear[]={31,29,31,30,31,30,31,31,30,31,30,31}; // API starts months from 1, this array starts from 0

/*!
  Convert date (YYYYMMDDhhmmss) into unix timestamp(UTC)

  \param[in] year - Full year (2022, for example)
  \param[in] month - Month starting at 1
  \param[in] day - Day starting at 1
  \param[in] hour - Hour starting at 0 and ending at 23
  \param[in] minute - Minute starting at 0 and ending at 59
  \param[in] second - Minute starting at 0 and ending at 59
  \return UTC seconds since Jan 1st 1970 00:00:00
*/
uint32_t utcFromDateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
  int i;
  uint32_t seconds;

  // seconds from 1970 till 1 jan 00:00:00 of the given year
  seconds = (year - 1970) * (SECS_PER_DAY * 365);
  for (i = 1970; i < year; i++) {
    if (LEAP_YEAR(i - 1970)) {
      seconds +=  SECS_PER_DAY;   // add extra days for leap years
    }
  }

  // add days for this year, months start from 1
  for (i = 1; i < month; i++) {
    if ( (i == 2) && LEAP_YEAR(year - 1970)) {
      seconds += SECS_PER_DAY * 29;
    } else {
      seconds += SECS_PER_DAY * monthDays[i-1];  //monthDay array starts from 0
    }
  }
  seconds+= (day-1) * SECS_PER_DAY;
  seconds+= hour * SECS_PER_HOUR;
  seconds+= minute * SECS_PER_MIN;
  seconds+= second;
  return seconds;
}

#define DAYS_PER_YEAR (365)
#define DAYS_PER_LEAP_YEAR (DAYS_PER_YEAR + 1)

static uint32_t getDaysForYear(uint32_t year) {
  return LEAP_YEAR(year - 1970) ? DAYS_PER_LEAP_YEAR : DAYS_PER_YEAR;
}

static const uint8_t * getDaysPerMonth(uint32_t year) {
  return LEAP_YEAR(year - 1970) ? monthDaysLeapYear : monthDays;
}

/*!
  Convert unix timestamp(UTC) in microseconds to date (YYYYMMDDhhmmss)

  \param[in] utc_us - utc in microseconds
  \param[out] utcDateTime - date time
  \return None
*/
void dateTimeFromUtc(uint64_t utc_us, utcDateTime_t *dateTime) {
  configASSERT(dateTime);

  // year
  uint64_t days = (utc_us / MICROSECONDS_PER_SECOND) / SECS_PER_DAY;
  dateTime->year = 1970;
  while( days >= getDaysForYear(dateTime->year) ) {
    days -= getDaysForYear(dateTime->year);
    dateTime->year++;
  }

  // months
  const uint8_t * monthDays = getDaysPerMonth(dateTime->year);
  dateTime->month = 1;
  while(days >= monthDays[dateTime->month-1]){
    days -= monthDays[dateTime->month-1];
    dateTime->month++;
  }

  // days
  dateTime->day = days + 1;

  uint64_t secondsRemaining = (utc_us / MICROSECONDS_PER_SECOND) % SECS_PER_DAY;
  // hours
  dateTime->hour = secondsRemaining / SECS_PER_HOUR;

  // minutes
  dateTime->min = (secondsRemaining / SECS_PER_MIN) % SECS_PER_MIN;

  // seconds
  dateTime->sec = secondsRemaining % SECS_PER_MIN;

  // useconds
  dateTime->usec = utc_us % MICROSECONDS_PER_SECOND;
}


/*!
  Calculate time remaining relative to start time, current time, and timeout.

  Deals with uint32_t overflow and returns 0 when expired. Will not work if
  time difference or timeout is greater than UINT32_MAX/2

  \param[in] startTime reference start time
  \param[in] currentTime reference time to calculate time remaining at
  \param[in] timeout timeout (if any)
  \return time remaining
*/
uint32_t timeRemainingGeneric(uint32_t startTime, uint32_t currentTime, uint32_t timeout) {
  // Signed variable on purpose to catch when timeRemaining goes negative
  int32_t timeRemaining = (int32_t)((startTime + timeout) - currentTime);

  if (timeRemaining < 0) {
    timeRemaining = 0;
  }

  return timeRemaining;
}

/*!
  Make a copy of a string and return it.
  Returning string MUST be freed by caller!

  \param[in] inStr - String to copy
  \return copy of string or NULL on failure
*/
char *duplicateStr(const char *inStr) {
  char *outStr = NULL;
  do {
    size_t len = strlen(inStr) + 1;
    outStr = (char *)pvPortMalloc(len);

    if(!outStr) {
      break;
    }

    strncpy(outStr, inStr, len);

  } while(0);

  return outStr;
}

/*!
  Check if a string only has ASCII characters

  \param[in] inStr - String to check
  \return true if all characters are <= 127, false otherwise
*/bool isASCIIString(const char *str){
  while (str && (*str != 0)){
    if ((uint8_t)*str > 127){
      return false;
    }
    str++;
  }
  return true;
}

/*!
  Convert degrees to radians

  \param[in] deg - degrees
  \return radians
*/
double degToRad(double deg) {
  return deg * (M_PI / 180.0);
}
