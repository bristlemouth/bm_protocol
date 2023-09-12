#pragma once
#include "stm32_rtc.h"
#include "fff.h"

DECLARE_FAKE_VALUE_FUNC(bool, isRTCSet);
DECLARE_FAKE_VALUE_FUNC(BaseType_t, rtcGet, RTCTimeAndDate_t*);
DECLARE_FAKE_VALUE_FUNC(uint64_t, rtcGetMicroSeconds, RTCTimeAndDate_t*);
