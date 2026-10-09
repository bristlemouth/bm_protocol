#pragma once

#include <stdbool.h>
#include <stdint.h>

// TIM2 input filter on the PPS input (TIMx_CCMR1 IC2F), 0 disables it.
// 1-15 select the sampling rate and number of samples (RM0456 TIMx_CCMR1). The
// filter delays the capture by roughly N samples at its sampling rate, so it is
// disabled by default to keep that latency out of the captured PPS time.
// Set from cmake with -DPPS_TIMER_INPUT_FILTER=<0-15>.
#ifndef PPS_TIMER_INPUT_FILTER
#define PPS_TIMER_INPUT_FILTER 0
#endif

// Filter sampling clock division (TIMx_CR1 CKD): tDTS = tim_ker_ck * 1 (0), 2 (1) or 4 (2).
// Only used when PPS_TIMER_INPUT_FILTER is enabled.
// Set from cmake with -DPPS_TIMER_INPUT_FILTER_CKD=<0-2>.
#ifndef PPS_TIMER_INPUT_FILTER_CKD
#define PPS_TIMER_INPUT_FILTER_CKD 0
#endif

typedef struct {
  // A PPS edge has set the timer phase and frequency since boot
  bool acquired;
  // Tracking PPS with a phase error inside the lock window
  bool locked;
  // Last PPS edge minus its expected time (second boundary + 10ms), positive when the timer is ahead
  int64_t phase_err_ns;
  // Timer frequency offset from nominal
  int64_t freq_offset_ppb;
  uint32_t pps_count;
  uint32_t rejected_count;
} PpsTimerStatus_t;

void ppsTimerInit(void);
void ppsTimerGetStatus(PpsTimerStatus_t *status);
// Time since the timer second boundary, false until the first PPS edge has set the timer
bool ppsTimerGetTimeSinceSecond(uint64_t *ns);
