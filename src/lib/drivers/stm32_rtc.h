#pragma once

#include <stdbool.h>
#include "FreeRTOS.h"
#include "stm32u5xx_hal.h"
#include "stm32u5xx_ll_rtc.h"

#ifdef __cplusplus
extern "C" {
#endif

// leap year calulator expects year argument as years offset from 1970
#define LEAP_YEAR(Y)     ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )

#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24UL)

typedef struct {
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint16_t ms;
} RTCTimeAndDate_t;

BaseType_t rtcInit();
BaseType_t rtcSet(const RTCTimeAndDate_t *timeAndDate);
BaseType_t rtcGet(RTCTimeAndDate_t *timeAndDate);
uint64_t rtcGetMicroSeconds(RTCTimeAndDate_t *timeAndDate);
bool isRTCSet();
bool isRTCValid(RTCTimeAndDate_t *receivedTimeAndDate, uint32_t driftThesholdS, uint32_t *deltaS);
uint32_t getCALM();
uint32_t getCALP();
uint32_t getRECALP();
uint32_t setCALM(uint32_t calm);
uint32_t setCALP(uint32_t calp);
#ifdef __cplusplus
}
#endif
