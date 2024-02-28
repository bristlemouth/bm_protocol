#include "mock_bridgeLog.h"

DEFINE_FAKE_VOID_FUNC_VARARG(bridgeLogPrint, bridgeLogType_e, bm_common_log_level_e, bool,
                             const char *, ...);