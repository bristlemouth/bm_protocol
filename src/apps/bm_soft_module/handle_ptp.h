#pragma once

#include <stdbool.h>
#include <stdint.h>

void handlePtpInit(void);
// Current network time from the ADIN2111 1588 timer, in ns since the epoch.
// False until the timer has been set from a PTP master and the PPS timer is
// tracking its TS_TIMER pulse.
bool handlePtpGetTime(uint64_t *ns);
