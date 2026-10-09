# BM F+L mote firmware: part 4

This covers part 4 of `mote-bm-f+l_md_claude_prompt.md`: a new `handle_ptp` task on the mote (`bm_soft_module`). It runs the IEEE 1588 peer-to-peer handshake and receives two-step `Sync`/`Follow_Up` from the bridge. It uses the ADIN2111 hardware timestamps to work out the master's time and then sets or servos the ADIN2111 timer. Nothing has been committed yet.

As the prompt asked, a subagent read `adin2111.pdf` first. The findings that shaped the design are listed under "Datasheet points used" below.

## Files changed

- **`src/apps/bm_soft_module/handle_ptp.cpp` / `handle_ptp.h` (new):** the `handle_ptp` task. It is 1024 words of stack at `HANDLE_PTP_TASK_PRIORITY` 6. It started as a copy of the bridge's `src/apps/bridge/adin_ptp.cpp`, changed for the slave side. The bridge files were not touched.
- **`src/apps/bm_soft_module/pps_timer.cpp` / `.h`:** adds `ppsTimerReacquire()`.
- **`src/apps/bm_soft_module/app_main.cpp`:** calls `handlePtpInit()` right after `bcl_init()`. The bridge calls `adinPtpInit()` after `bcl_init()` too.
- **`src/apps/bm_soft_module/task_priorities.h`:** adds `HANDLE_PTP_TASK_PRIORITY 6`. That is the same value as the bridge's PTP task, and one below the L2 TX task.
- **`src/apps/bm_soft_module/CMakeLists.txt`:** adds `handle_ptp.cpp`.
- **bm_core `drivers/adin2111/bm_adin2111.c/.h`:** adds `adin2111_ts_timer_restart()`. It is behind `bm_adin2111_ts_timer_enabled`, and it reuses the part 1 `TS_TIMER_START_NS` and the driver's `adin2111_TsTimerStop()`.

## What the task does

### Peer delay (link delay to the master)
This is unchanged from the bridge.
- Every port is both initiator and responder.
- The responder side matters: the bridge only sends Sync to ports whose neighbor answers its `Pdelay_Req`.
- Initiator math: `link_delay = ((t4−t1) − (t3−t2) − corr) / 2`, accepted only within ±100 µs.
- Capture slot B is used for Pdelay_Req and slot C for Pdelay_Resp.
- There is a 1 s interval with back-off to 32 s, and the timeouts match the bridge.

### Sync / Follow_Up (slave)
- **On `Sync` (two-step only):** record t2 (the ADIN2111 RX timestamp), the sequence ID, the correction field, the master's port identity, and the RX tick.
- **On the matching `Follow_Up`:** same sequence and source port identity, received within 500 ms. Then t1 = `preciseOriginTimestamp`.
- Syncs are used only on a port with a valid link delay.
- There is no BMCA. The task follows the first port Syncs arrive on until that port has been quiet for 3 s.
- **Error:** `err = (t1 + link_delay + corr_sync + corr_fup) − t2`, which is master − ADIN. A positive value means the ADIN is behind. This is the same sign convention as the bridge's `bm_serial_adin_utc_cb`.

### Servo
The constants are the bridge's (`ncp_uart.cpp`). The bridge sends Sync once a second, so an error in ns is also a frequency error in ppb.

| Item | Value |
|---|---|
| Kp | 1/4 |
| Ki | 1/64. The error is integrated only when it is within ±400 µs and the Sync is consecutive (sequence ID + 1). |
| Max slew | 100 ppm |
| Max integral frequency | ±100 ppm |
| Step threshold | 1 ms |

- **Within the threshold:** write `TS_ADDEND = 0x85555555 · (1 + ppb·1e-9)`.
- **Over the threshold** (including the first Sync after boot): step the timer.

All ADIN2111 register access (the addend write and the step) runs on the L2 thread through `bm_l2_run_in_thread`, because bm_core's PTP functions must only be called there.

### Stepping without reading `TS_SEC_CNT` / `TS_NS_CNT`
To step, the task needs the ADIN2111's *current* time, but those registers must never be read. It works it out like this:
1. **Seconds:** from the Sync's t2, plus the FreeRTOS ticks elapsed since that frame arrived.
2. **Nanoseconds:** `ppsTimerGetTimeSinceSecond()`. TIM2 wraps on each ADIN2111 second boundary, because pps_timer locks it to the TS_TIMER pulse at +10 ms.
3. **Choosing the second:** the second that puts the TIM2 time closest to the tick estimate. The tick estimate only has to be within ±0.5 s.

The step job then:
- calls `adin2111_TsSetTimerAbsolute(now + err)`, which keeps the addend;
- calls `adin2111_ts_timer_restart()`;
- calls `ppsTimerReacquire()`.

If the PPS timer isn't tracking, the tick estimate alone is used, which is accurate to about a millisecond, and the log says `no PPS timer`.

**After a step:**
- The step job increments an epoch counter on the L2 thread. Every received frame and egress timestamp carries the epoch it was taken in, so any timestamp from before the step is dropped and pending exchanges are reset. This stops a stale t2 from causing a second step.
- Steps then wait up to 5 Syncs for the PPS timer to lock onto the restarted pulse. The next step, if one is needed, can then use the precise TIM2 time.

### `ppsTimerReacquire()`
This clears `acquired`, `locked` and the outlier count. It also drops the last capture, so the interval that spans the step is not used as a frequency measurement. The next clean interval re-steps TIM2's phase and frequency the same way as at boot, about 2 s later. Without this, a pulse phase change of up to 0.5 s would be slewed out at 100 ppm, which takes hours.

### `handlePtpGetTime(uint64_t *ns)`
This returns the current network time from the ADIN2111 timer, using the same seconds + TIM2 method.
- The reference is the last Sync the servo used, and it must be less than 60 s old.
- It returns false before the first good Sync, while a step is in progress, or while the PPS timer is reacquiring.
- Nothing calls it yet.

## Datasheet points used

| Point | How it was used |
|---|---|
| TS_TIMER compares against the ns counter only, and does not realign when TS_SEC_CNT/TS_NS_CNT are written. To restart it, set TS_CFG.TS_TIMER_STOP, then write TS_TIMER_START. | This is why `adin2111_ts_timer_restart()` exists. The datasheet leaves the realignment behavior undocumented; the subagent inferred it. |
| There is no offset/adjust register. | Setting the time means writing SEC/NS (the driver freezes the clock by writing ADDEND = 0 during the write, then restores it). Fine adjustments are made through ADDEND. |
| ADDEND: 32-bit accumulator on a 120 MHz clock, +16 ns per carry, nominal 0x85555555, about 0.45 ppb per LSB. | Matches the bridge's `adin_ptp_set_freq` math. |
| The timestamp point is SFD detection in the PHY (CONFIG2.SFD_DETECT_SRC = 0), with 16 ns resolution. | With ADIN2111s at both ends the PHY latencies are symmetric. `ADIN_PTP_TX/RX_LATENCY_NS` stay 0, as on the bridge. |
| CONFIG2 forwards unknown DAs to the host, and the UNK2P1/UNK2P2 port-to-port forwarding is off. | The PTP multicast frames (`01:80:C2:00:00:0E`, `01:1B:19:00:00:00`) reach the MCU and are not switched through to the other port. That is correct, because the ADIN2111 has no transparent-clock correction. |

## Verification

- The `soft` preset (`preset-builds/soft`) builds with no errors and no new warnings. Only the existing RWX LOAD segment linker warning remains.
- The `bridge` preset also still builds, because the bm_core header changed.
- **Not tested on hardware.** With a bridge that has GPS time, expect this log sequence on the mote:
  1. `PTP port N: neighbor answering peer delay, link delay … ns`, with a plausible delay well under 100 µs.
  2. `PTP port N: following master`.
  3. The first `PTP Sync: … ADIN2111 error: <huge> ns`, then `PTP set ADIN2111 timer to …`.
  4. A few Syncs while the PPS timer reacquires. There may be one more small step.
  5. The errors converge toward 0 under the PI loop, with no further steps.
- Then check:
  - On a scope, the mote's PA1 pulse should land 10 ms past the bridge's GPS PPS edge, well within 100 µs.
  - `ppsTimerGetStatus()` should show `acquired`/`locked` again after each step.

## Known caveats and follow-ups

- **No BMCA or Announce.** The bridge is assumed to be the only master.
- **No downstream propagation.** The mote doesn't send Sync to its other port, so a mote two hops from the bridge gets no time. That would need the mote to act as a boundary clock on its other port.
- **Step accuracy.** `TsSetTimerAbsolute` freezes the clock for the length of its SPI writes, and it truncates ns to a multiple of 16. That gives a small residual after a step, and the PI loop removes it.
- **TS_TIMER realignment is inferred, not documented.** That TS_TIMER needs a restart after a time set is the subagent's inference from the datasheet. Restarting is harmless either way. On the bench, check that the pulse realigns to +10 ms after `PTP set ADIN2111 timer`.
