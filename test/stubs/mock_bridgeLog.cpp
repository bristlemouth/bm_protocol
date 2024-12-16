#include "mock_bridgeLog.h"

DEFINE_FAKE_VOID_FUNC_VARARG(bridgeLogPrint, bridgeLogType_e, BmLogLevel, bool,
                             const char *, ...);
