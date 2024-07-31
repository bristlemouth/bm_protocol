#include "bcmp_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define _Malloc BcmpMalloc
#define _Free BcmpFree
#define _GetMs BcmpGetMs
#define _DelayMs BcmpDelayMs

#ifdef __cplusplus
}
#endif