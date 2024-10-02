#include "bm_util.h"

#include "bm_rtc.h"
#include "stm32_rtc.h"

BmErr bm_rtc_set(const RtcTimeAndDate *time_and_date) {
  BmErr err = BmOK;
  if (rtcSet((const RTCTimeAndDate_t *)time_and_date) != pdPASS) {
    err = BmEINVAL;
  }
  return err;
}
BmErr bm_rtc_get(RtcTimeAndDate *time_and_date) {
  BmErr err = BmOK;
  if (rtcGet((RTCTimeAndDate_t *)time_and_date) != pdPASS) {
    err = BmEINVAL;
  }
  return err;
}

uint64_t bm_rtc_get_micro_seconds(RtcTimeAndDate *time_and_date) {
  return rtcGetMicroSeconds((RTCTimeAndDate_t *)time_and_date);
}

// const ip6_addr_t multicast_global_addr = {{0x3FF,0x0,0x0,0x1000000}, 0};
// const ip6_addr_t multicast_ll_addr = {{0x2FF,0x0,0x0,0x1000000}, 0};
