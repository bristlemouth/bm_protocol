#pragma once
#include <stdint.h>
#include "fff.h"
#include "lpm.h"

DECLARE_FAKE_VOID_FUNC(lpmPeripheralActive, uint32_t);
DECLARE_FAKE_VOID_FUNC(lpmPeripheralInactive, uint32_t);