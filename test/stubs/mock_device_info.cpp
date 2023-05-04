#include "mock_device_info.h"

DEFINE_FAKE_VALUE_FUNC(uint64_t, getNodeId);
DEFINE_FAKE_VALUE_FUNC(const versionInfo_t *, getVersionInfo);
DEFINE_FAKE_VALUE_FUNC(uint32_t, getGitSHA);
