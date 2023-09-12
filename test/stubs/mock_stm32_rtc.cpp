#include "mock_stm32_rtc.h"

DEFINE_FAKE_VALUE_FUNC(bool, isRTCSet);
DEFINE_FAKE_VALUE_FUNC(BaseType_t, rtcGet, RTCTimeAndDate_t*);
DEFINE_FAKE_VALUE_FUNC(uint64_t, rtcGetMicroSeconds, RTCTimeAndDate_t*);
