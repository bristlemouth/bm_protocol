# ADIN2111 TS_TIMER debugging on the BM F+L mote

This covers a debugging session on the mote firmware (`bm_soft_module`, `BSP=bm_mote_spi_v1_0`) built in parts 1–4 of `mote-bm-f+l_md_claude_prompt.md` (see `mote-part1.md` to `mote-part4.md`). The ADIN2111 TS_TIMER output on P1_LED_0 (wired to MCU pin PA1) was a ~30 MHz square wave instead of a 50 ms pulse once a second, 10 ms past each second. The session found three bugs and fixed all of them. At the end, the mote put out a clean 1 PPS on PA1, `pps_timer` acquired and locked to it, and PTP converged to a few µs with no repeated steps. Nothing has been committed.

## Summary

| # | Problem | Cause | Fix |
|---|---|---|---|
| 1 | ~30 MHz square wave on P1_LED_0, then a flat line once the restart was skipped | `TS_TIMER_HI`/`LO` written before `adin2111_SyncConfig()` and the port enables read back correctly, but TS_TIMER behaves as if they were 0 and toggles every 16 ns (31.25 MHz) | Write the TS_TIMER and LED settings after `SyncConfig` and the port enables (`bm_adin2111.c`) |
| 2 | `pps_timer` rejected every PA1 edge (`pps_count 0`) | The mote's MSI-based TIM2 clock runs about +1.6% fast (≈15,900 ppm), over the 10,000 ppm limit | `PPS_TIMER_MAX_FREQ_PPM` raised to 30000 (`pps_timer.cpp`) |
| 3 | After fixes 1 and 2, PTP stepped every 2 Syncs and each step missed by tens to hundreds of ms | `ppsTimerGetTimeSinceSecond()` returned true as soon as `pps_timer` acquired, before the phase correction (an ARR trim) had taken effect at a wrap | Added a `phase_wraps` countdown, so the time readout is valid only after the trimmed period ends (`pps_timer.cpp/.h`) |

## Investigation

### 1. Static review

- **Values:** the TS_TIMER HI/LO/START values (50,000,000 / 950,000,000 / 10,000,000 ns) are all divisible by 16.
- **Addresses:** they go to 0x85, 0x86 and 0x88, which matches the datasheet.
- **Pin settings:**
  - `LED_CNTRL.LED0_FUNCTION` = 23 (TS_TIMER)
  - `DIGIO_TSTIMER_PINMUX` = 01 (LED_0)
  - `LED_POLARITY` = active high
- **Write order:** `TS_EN` is set before the HI/LO writes, and START is written last, as the datasheet sequence requires.
- **Built firmware:** disassembling the built `.elf` showed the right constants written to the right registers. A stale build was ruled out.
- **Other writers:** nothing else in `src/` writes the LED, TS_CFG or TS_TIMER registers, and nothing resets the MAC later.
- **MCU side (part 2):** PA1 is a TIM2_CH2 input capture, so the MCU doesn't drive the pin.
- **Conclusion:** 31.25 MHz is exactly what TS_TIMER gives when it toggles on every 16 ns step of the 1588 clock, which is the behaviour of HI = LO = 0.

### 2. Debug tooling

All of this is in the mote app only.

- **`adints` CLI command** (`handle_ptp.cpp`):
  - It prints `TS_ADDEND`, `TS_CFG`, `TS_TIMER_HI/LO/QE_CORR/START` and the port 1 PHY registers `DIGIO_PINMUX`, `LED_CNTRL` and `LED_POLARITY`.
  - It also prints the `pps_timer` status, including the last measured PPS interval against the nominal one.
  - It never reads `TS_SEC_CNT` or `TS_NS_CNT`.
  - All ADIN2111 access runs on the L2 thread through `bm_l2_run_in_thread`.
- **`adints` subcommands:**

  | Subcommand | Action |
  |---|---|
  | `stop` | Sets `TS_CFG.TS_TIMER_STOP` |
  | `start <ns>` | Writes `TS_TIMER_START` |
  | `restart` | Stops TS_TIMER, then writes `START = 10000000` |
  | `hilo <hi> <lo>` | Writes `TS_TIMER_HI` and `TS_TIMER_LO` |
  | `settime <sec> <ns>` | Calls `adin2111_TsSetTimerAbsolute()` without restarting TS_TIMER |
  | `watch <s> [restart \| start <ns>]` | See below |

- **`watch` details:**
  - It masks the TIM2 interrupt so a 30 MHz edge storm can't starve the system and trip the watchdog.
  - Every 10 ms it samples TIM2's CC2 capture flag, and once a second it prints how many of the 100 windows had a PA1 edge. About 100 means a storm, 1 means 1 PPS, and 0 means nothing.
  - It does the optional action 2 s in.
  - At the end it stops TS_TIMER if it is still storming, then unmasks the interrupt and reacquires.
- **Debug helpers in `pps_timer`:** `ppsTimerDebugMask()` and `ppsTimerDebugEdge()`, plus `last_interval_ticks` and `nominal_ticks` in `PpsTimerStatus_t`.
- **Build flags** (default 0, passed through CMake like `PPS_TIMER_INPUT_FILTER`):
  - `PTP_SKIP_TS_TIMER_RESTART`: `step_job` steps the ADIN clock but doesn't call `adin2111_ts_timer_restart()`.
  - `PTP_DISABLE_STEP`: Syncs with an error over the step threshold are only logged, never stepped.

### 3. Bench tests

These ran on the mote at `/dev/serial/by-id/usb-Sofar_Ocean_Technologies_Bristlemouth_0000A8643075-if00`, flashed with `make flash` from `preset-builds/soft`. Note that CLAUDE.md lists a different ID (`…00008D09696F`).

| Test | Result | Takeaway |
|---|---|---|
| `PTP_SKIP_TS_TIMER_RESTART=1` build | No 30 MHz, but nothing on the scope | The restart after a step started the storm; without it there's no pulse at all |
| `adints` with PTP locked | All registers read back exactly as intended, `TS_EN` = 1 | Register contents aren't the problem |
| `adints restart` with no clock jump | Mote crashed (storm) | The step's clock jump isn't required |
| `adints start 990000000`, START about 100 ms ahead of the clock, no stop | Mote crashed (storm) | START being behind the current ns isn't the trigger |
| `watch 120 restart` | Storm for the whole 117 s | Rules out a "catch up 1.79e9 s" theory (that would have taken ~57 s) |
| `PTP_DISABLE_STEP=1` build, ADIN clock near 0 s | No edges at boot | TS_TIMER never pulsed, even with no step |
| `watch 12 restart` at small seconds | Storm | Not related to the seconds value |
| `hilo 500M 500M` + restart; `hilo 100M 100M`; `hilo 50M 950M` + restart | 1 Hz; 5 Hz; 1 Hz | HI/LO work once rewritten at runtime |
| Fresh boot, `hilo 50000000 950000000` (same values) + restart | Clean 1 PPS | **Writes made during init don't take effect** |
| `adints` after that | `rejected` climbing, interval 162.54M against nominal 160M ticks | Bug 2: the MCU clock is +1.6% off |

## Fixes

### Fix 1: TS_TIMER init order (`src/lib/bm_core/drivers/adin2111/bm_adin2111.c`, bm_core submodule)

- The LED polarity, LED function and `TS_TIMER_HI/LO/START` writes moved into a new helper, `static adi_eth_Result_e ts_timer_configure_(void)`. It is behind `bm_adin2111_ts_timer_enabled`.
- `adin2111_netdevice_enable()` calls it after `adin2111_SyncConfig()` and after both ports are enabled. A failure sets `err = BmENODEV`, and the existing `end:` path powers the ADIN off.
- `adin2111_TsEnable(ADI_MAC_TS_FORMAT_NONE)` (when `bm_adin2111_ptp_enabled == 0`) stays before `SyncConfig`, because `MAC_TsEnable` returns `ADI_ETH_CONFIG_SYNC_ERROR` once the config is synced.
- Which step resets the TS_TIMER block was not pinned down. The datasheet doesn't document it; `SyncConfig` or the port enable are the likely suspects. Running the configuration after both avoids the problem.
- With this fix, `adin2111_ts_timer_restart()` after a PTP step also works and doesn't start a storm, so `PTP_SKIP_TS_TIMER_RESTART` is no longer needed.
- The bridge preset still builds. It doesn't enable `bm_adin2111_ts_timer_enabled`, so its behaviour is unchanged.

### Fix 2: PPS frequency limit (`src/apps/bm_soft_module/pps_timer.cpp`)

- `PPS_TIMER_MAX_FREQ_PPM` went from 10000 to 30000, and the comment now records the measured +1.6%.
- The mote's system clock is MSI (range 0, 48 MHz) → PLL → 160 MHz, with no LSE calibration (MSI PLL mode off). TIM2 counted about 162.53M–162.55M ticks per ADIN second.
- The intervals also jitter by about ±10k ticks (±60 µs). That shows up as a `pps_timer` phase error of about ±150–230 µs while locked.

### Fix 3: PPS time readout before the phase is applied (`src/apps/bm_soft_module/pps_timer.cpp/.h`)

**The problem:**
- On acquisition, `handle_capture()` set `acquired = true` and applied the phase step as an ARR trim. The trim is written to ARR directly when the edge comes after the wrap and there is enough margin; otherwise it goes in `pending_trim` for the next period.
- So TIM2's count is aligned to the ADIN second only after one or two more wraps.
- Until then, `ppsTimerGetTimeSinceSecond()` returned a time that could be off by up to a second. `handle_ptp`'s `step_job` used it (those steps logged without the `, no PPS timer` suffix), stepped to the wrong time, restarted TS_TIMER, and the cycle repeated.

**The fix:**
- A new `phase_wraps` field in the context is set to 1 (trim written directly) or 2 (trim pending) on acquisition.
- `handle_update()` counts it down at each wrap.
- `ppsTimerGetTimeSinceSecond()` returns false until it reaches 0.
- The step holdoff in `handle_ptp` already waits on `ppsTimerGetTimeSinceSecond()`, so steps now wait for a valid phase.

## Final bench result

This was the normal configuration: both debug flags off, steps enabled, TS_TIMER restarted after a step.

- **PTP:** the Sync error fell from 917 µs to 2.4 µs over about 60 s through the PI servo, with no further steps. The boot step happened before the log capture started, so it wasn't seen.
- **`pps_timer`:** `acquired 1, locked 1`, `pps_count` going up once per second, `rejected` steady at 2 (from start-up).
- **Mote:** stayed up, with no edge storm.

## Build state

- **`src/apps/bm_soft_module/CMakeLists.txt`:**
  - CMake passthroughs for `PTP_SKIP_TS_TIMER_RESTART` and `PTP_DISABLE_STEP` (both off unless set).
  - The `PTP_SKIP_TS_TIMER_RESTART=1` and `PTP_DISABLE_STEP=1` lines added to `APP_DEFINES` during debugging were removed.
- **CMake cache:** `preset-builds/soft/CMakeCache.txt` had `PTP_SKIP_TS_TIMER_RESTART=1` cached from an earlier `-D` configure. It was cleared with `cmake -U PTP_SKIP_TS_TIMER_RESTART .`. A cached `-D` setting stays in effect after the line is removed from `CMakeLists.txt`.
- **Changed files (uncommitted):**
  - `src/apps/bm_soft_module/CMakeLists.txt`
  - `src/apps/bm_soft_module/handle_ptp.cpp`
  - `src/apps/bm_soft_module/pps_timer.cpp`
  - `src/apps/bm_soft_module/pps_timer.h`
  - `src/lib/bm_core/drivers/adin2111/bm_adin2111.c` (bm_core submodule)

## Open items

- **Scope check:** compare the mote's PA1 against the bridge's GPS PPS. Expect the mote 10 ms later, within the PTP error.
- **Longer run:** check that the PTP error settles to tens of ns, as it did with the earlier builds.
- **MCU clock:** consider turning on MSI PLL mode (`HAL_RCCEx_EnableMSIPLLMode()`, which locks the MSI to the 32.768 kHz LSE). That would remove the +1.6% offset and much of the ±60 µs jitter. It changes the system clock for the whole mote, and the bridge uses the same clock setup.
- **Debug code:** decide whether to keep or remove the `adints` command and its subcommands, the `pps_timer` debug helpers and status fields, and the two debug build flags.
- **Docs:** update `mote-part1.md` (init order) and `mote-part4.md` (TS_TIMER restart, `phase_wraps`) to match.
