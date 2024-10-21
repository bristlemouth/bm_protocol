#include "mock_uptime.h"

DEFINE_FAKE_VALUE_FUNC(uint64_t, uptimeGetMicroSeconds);
DEFINE_FAKE_VALUE_FUNC(uint64_t, uptimeGetMs);
DEFINE_FAKE_VALUE_FUNC(uint64_t, uptimeGetStartTime);
DEFINE_FAKE_VOID_FUNC(uptimeInit);
DEFINE_FAKE_VOID_FUNC(uptimeUpdate, uint64_t);
