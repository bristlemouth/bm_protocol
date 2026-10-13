# Bridge vs mote IEEE 1588 comparison

Comparison of the bridge grandmaster (`src/apps/bridge/adin_ptp.cpp`) and the
mote slave (`src/apps/bm_soft_module/handle_ptp.cpp`), covering the peer to peer
delay handshake and Sync + Follow_Up. Supporting code reviewed:
`src/apps/bm_soft_module/pps_timer.cpp`, `src/lib/bm_ncp/ncp_uart.cpp`, and the
ADIN2111 driver in `src/lib/bm_core/drivers/adin2111/`.

This is a review of the code only. No logs were inspected and nothing was run.

## Summary

The message exchange is the same on both ends and matches IEEE 1588-2008
two-step peer to peer delay. No field, sequence ID, port identity, length or
correction-field mismatch was found that would make either side reject the
other's frames. The driver plumbing (TTSC register addresses, 64-bit timestamp
word order, egress capture slot enum, OA SPI RX timestamp unpacking, unknown
multicast forwarded to host) also checks out against the datasheet.

The most likely reason the mote doesn't sync is in its clock-step logic: once
the mote needs a second step, it can step forever and never settle (issue 1
below).

## Peer delay handshake

`send_pdelay_req`, the Pdelay cases of `handle_rx`, `handle_egress_ts` and
`pdelay_try_complete` are essentially the same in both files.

| | Bridge (`adin_ptp.cpp`) | Mote (`handle_ptp.cpp`) |
|---|---|---|
| Sends Pdelay_Req | Every 1 s per port, egress timestamp in slot B, backs off to 32 s after 3 missed responses | Same |
| Answers Pdelay_Req | Pdelay_Resp (t2, two-step flag), then Pdelay_Resp_Follow_Up (t3) once the slot C egress timestamp is ready | Same |
| Link delay | `((t4 − t1) − (t3 − t2) − corr) / 2`, rejected if 100 µs or more | Same |
| What "capable" gates | Whether the bridge keeps sending Sync on that port | Whether the mote uses incoming Syncs |
| Before GPS time is set | Still answers the mote's requests; sends its own only after `adinPtpTimeSet()` | Not applicable |
| Destination MAC | `01:80:C2:00:00:0E` | `01:80:C2:00:00:0E` |

Checked and consistent on both sides:

- Sequence ID of Pdelay_Resp and Pdelay_Resp_Follow_Up copied from the request.
- requestingPortIdentity copied from the request and checked by the initiator.
- Pdelay_Resp_Follow_Up sourcePortIdentity checked against the Pdelay_Resp.
- messageLength 54, control field 5, logMessageInterval 0x7F.
- The initiator handles the t1 egress timestamp, Pdelay_Resp and
  Pdelay_Resp_Follow_Up arriving in any order (`pdelay_have` bitmask).
- Initiator (slot B) and responder (slot C) use separate egress capture slots,
  so simultaneous exchanges in both directions on one port don't collide.

## Sync + Follow_Up

| | Bridge (grandmaster) | Mote (slave) |
|---|---|---|
| Sync | 1 Hz to `01:1B:19:00:00:00`, two-step flag, egress timestamp slot A. Sent only if the port is capable, plus one forced Sync right after the time is set or the link comes up | Accepted only with a valid receive timestamp and the two-step flag; stores t2, seq, correction and master port ID |
| Follow_Up | preciseOriginTimestamp = Sync egress time, same seq and source port ID, control field 2 | Must match seq and master port ID; dropped if no Follow_Up within 500 ms |
| Offset | — | `err = (t1 + link_delay + sync_corr + fup_corr) − t2`; positive means the mote is behind |
| Correction | — | Step if \|err\| > 1 ms; otherwise PI loop on TS_ADDEND |
| Master selection | — | Follows the first port Syncs arrive on until it is quiet for 3 s |

Notes:

- The forced Sync always arrives before the mote has measured a link delay, so
  the mote ignores it. Usable Syncs start about 1 s after link-up. This is
  expected.
- Servo sign is correct: positive error raises ADDEND, and a larger ADDEND runs
  the ADIN2111 timer faster (120 MHz accumulator, 16 ns per carry, nominal
  `0x85555555`).
- The PI loop (Kp = 1/4, Ki = 1/64 per 1 s Sync) has closed-loop poles near
  0.91 and 0.83, so it's stable and overdamped.

## Problems found, most likely first

### 1. Mote step can land in a window where TIM2's phase is still wrong

After every step, `step_job` (`handle_ptp.cpp:347`) restarts TS_TIMER and calls
`ppsTimerReacquire()`.

- `ppsTimerGetTimeSinceSecond()` reports itself usable as soon as
  `pps_ctx.acquired` is set (`pps_timer.cpp:153`, `pps_timer.cpp:294-300`).
- The phase correction from that same edge (`trim = err`) only takes effect at
  the next TIM2 wrap, which can be up to about 2 s later. Until then `TIM2->CNT`
  is still counted from the old wrap, so the time since the second boundary is
  off by the full phase shift the previous step caused.
- The step holdoff in `sync_complete` (`handle_ptp.cpp:577`) only waits for
  `pps_ok`. A second step therefore happens on the first Sync after reacquire,
  almost always inside that window. `adin_time_now()` returns the wrong current
  time and the step lands wrong by about as much as the previous step moved the
  clock. The "closest second" selection can also pick the wrong second, adding
  a 1 s error.
- The next Sync measures an error over 1 ms and steps again. Each step
  reproduces the error, so the loop never settles.

Anything that makes a single step miss by more than 1 ms can start this:

- the bridge stepping its own timer (see issue 3);
- a systematic offset in the assumed TS_TIMER edge position (10 ms after the
  second, `PPS_TIMER_EDGE_OFFSET_NS`), for example if the pin polarity puts the
  rising edge at the end of the 50 ms high period;
- SPI time inside `adin2111_TsSetTimerAbsolute`, which reads ADDEND, writes
  ADDEND = 0 (freezing the clock), writes TS_SEC_CNT and TS_NS_CNT, then
  restores ADDEND. Neither the time between computing the target and loading
  it nor the time the clock is frozen is compensated.

What it looks like:

- **Scope:** the mote's TS_TIMER pulse jumps every 2–3 s and never settles.
- **Mote log:** `PTP set ADIN2111 timer to …` repeats.

Possible fixes:

- In `pps_timer.cpp`, only report the time as usable once the acquisition phase
  correction has been applied at a wrap, for example a flag cleared in
  `handle_capture` on acquire and set in `handle_update` after the corrected
  period ends.
- Compensate the step for the SPI and freeze time, or measure the result of a
  step before allowing another one.

### 2. Bridge accesses the ADIN2111 outside the L2 thread

`bm_serial_adin_utc_cb` (`ncp_uart.cpp:243`) reads TS_EXT_CAPT0/1 and writes
TS_ADDEND, TS_SEC_CNT and TS_NS_CNT from the NCP task. `bm_adin2111.h` says
these accesses must happen only in the L2 thread, and the mote follows that rule
by running `set_freq_job` and `step_job` through `bm_l2_run_in_thread`.

The driver's register access (`MAC_ReadRegister` in `adi_mac.c`) uses a
`pendingCtrl` flag and a busy-wait, not a lock. Two tasks accessing registers
at once can occasionally collide with each other or with the PTP egress
timestamp reads and frame traffic.

Fix: run the bridge's ADIN timer reads and writes as L2 thread jobs, as the mote
does.

### 3. Bridge re-steps don't notify the PTP task

When the bridge's GPS discipline error exceeds 1 ms it re-steps the timer
(`ncp_uart.cpp:319`) without calling `adinPtpTimeSet()`. That's harmless by
itself: Sync keeps flowing and the mote sees a jump and steps. But every bridge
step is one more chance to trigger issue 1.

### 4. Scope expectations

- The mote's TS_TIMER rising edge is 10 ms after the second
  (`TS_TIMER_START_NS`). When syncing works, the mote pulse should sit about
  10 ms after GPS PPS, not on top of it.
- The bridge app doesn't define `bm_adin2111_ts_timer_enabled`, so the bridge's
  P1_LED_0 isn't a time pulse. The only bridge-side reference to scope is the
  GPS PPS.
- The mote follows the bridge's ADIN2111 timer, not GPS directly. If the
  bridge's GPS discipline isn't locked (check `ADIN2111 PPS capture: …, error:`
  in the bridge log), the mote will drift against GPS even with PTP working.

### 5. Accuracy only: no rate-ratio correction

Neither side corrects the responder turnaround (t3 − t2) for the clock-rate
difference between the two ends (neighborRateRatio). The turnaround goes
through the L2 thread, the PTP task and back, so it's several ms. With tens of
ppm of crystal offset that biases the link delay by roughly 100 ns. This won't
stop syncing.

## Finding which stage fails

Check the mote log in this order:

1. `PTP port N: neighbor answering peer delay, link delay … ns`. If it's
   missing, the problem is in the peer delay exchange or timestamps. Look for
   `invalid link delay` or `PTP failed to read egress timestamp`.
2. The same line in the bridge log. If it's missing, the bridge never sends
   more than the one forced Sync.
3. `PTP port N: following master`, then `PTP Sync: master …, ADIN2111 error: …`
   once a second. If these are missing, Sync or Follow_Up isn't matching or the
   mote's port isn't capable.
4. Repeated `PTP set ADIN2111 timer to …`: issue 1.
