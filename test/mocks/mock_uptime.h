#pragma once
#include "fff.h"
#include "uptime.h"

DECLARE_FAKE_VALUE_FUNC(uint64_t, uptimeGetMicroSeconds);
DECLARE_FAKE_VALUE_FUNC(uint64_t, uptimeGetMs);
DECLARE_FAKE_VALUE_FUNC(uint64_t, uptimeGetStartTime);
DECLARE_FAKE_VOID_FUNC(uptimeInit);
DECLARE_FAKE_VOID_FUNC(uptimeUpdate, uint64_t);
