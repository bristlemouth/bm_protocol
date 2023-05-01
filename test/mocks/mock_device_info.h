#pragma once
#include "device_info.h"
#include "fff.h"

DECLARE_FAKE_VALUE_FUNC(uint64_t, getNodeId);
DECLARE_FAKE_VALUE_FUNC(const versionInfo_t *, getVersionInfo);
