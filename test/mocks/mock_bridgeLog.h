#pragma once
#include "bridgeLog.h"
#include "fff.h"

DECLARE_FAKE_VOID_FUNC_VARARG(bridgeLogPrint, bridgeLogType_e, BmLogLevel, bool,
                              const char *, ...);
