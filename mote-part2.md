# BM F+L mote firmware: part 2

This covers part 2 of `mote-bm-f+l_md_claude_prompt.md`: capturing the ADIN2111 TS_TIMER pulse on the mote (`bm_soft_module`) and using it to discipline TIM2 into a high-resolution clock. The pulse comes from P1_LED_0, which is wired to MCU pin PA1. PA1 is set to TIM2_CH2 (AF1), channel 2 captures each rising edge, and a PI loop keeps TIM2 locked to the pulse. Parts 3–4 were out of scope for this session. As the plan asked, nothing else in the firmware uses the timer yet. Nothing has been committed yet.

The prompt's rules were: make the smallest change possible, follow existing patterns, reuse existing utilities, and allow STOP2 to be disabled.

## Files changed

- **`src/apps/bm_soft_module/pps_timer.cpp` / `pps_timer.h` (new):** a copy of the bridge's `src/apps/bridge/pps_timer.cpp` / `.h` (from step 4 of the bridge work), changed for the mote. The bridge files were not touched.
- **`src/apps/bm_soft_module/app_main.cpp`:**
  - includes `pps_timer.h`
  - calls `ppsTimerInit()` early in `defaultTask`, right after `lpmPeripheralActive(LPM_BOOT)`, the same place the bridge calls it
  - removes the `i2c_mux_rst` entry from `debugGpioPins`
- **`src/apps/bm_soft_module/CMakeLists.txt`:** adds `pps_timer.cpp` to `APP_FILES`. It also adds the same optional `PPS_TIMER_INPUT_FILTER` / `PPS_TIMER_INPUT_FILTER_CKD` CMake passthrough the bridge has; both are off by default.
- **`src/bsp/bm_mote_spi_v1_0/Core/Src/gpio.c`:** removes `I2C_MUX_RESET_Pin` (PA1) from the GPIOA output pin reset and init. The file's CRLF line endings were kept.
- **`src/bsp/bm_mote_spi_v1_0/bsp.cpp`:** removes `IOWrite(&I2C_MUX_RESET, 1)` from `bspInit()`.

## Changes from the bridge module

| | Bridge | Mote |
|---|---|---|
| Input pin | PA0, AF1 | PA1, AF1 |
| Timer channel | TIM2_CH1 (`CC1S`, `IC1F`, `CC1E`, `CC1IE`, `CC1IF`/`CC1OF`, `CCR1`) | TIM2_CH2 (`CC2S`, `IC2F`, `CC2E`, `CC2IE`, `CC2IF`/`CC2OF`, `CCR2`) |
| Pulse source | GPS PPS | ADIN2111 TS_TIMER on P1_LED_0 |
| Where the counter wraps | On the PPS edge | On the second boundary, 10 ms before the pulse |
| Time readout | `ppsTimerGetTimeSincePps()` | `ppsTimerGetTimeSinceSecond()` |

Everything else is unchanged: the PI gains, the frequency-locked loop, glitch and outlier rejection, holdover, slew limits, the IRQ ordering logic and `ppsTimerGetStatus()`.

## Design decisions

### Reusing the bridge module

The bridge already has a working TIM2 PPS-discipline module that does what part 2 asks: capture rising edges and keep the timer locked with a PI loop. That module is hard-coded to CH1/PA0. Sharing one file between the two apps would mean changing the bridge code, so it was copied into the mote app and changed there instead.

### Locking to the second boundary, not the pulse

The ADIN pulse arrives 10 ms after each second boundary (`TS_TIMER_START` = 10000000 ns from part 1). A new constant, `PPS_TIMER_EDGE_OFFSET_NS = 10000000`, is turned into `edge_offset_ticks` at init. In `handle_capture()` the phase error is now measured against that offset:

- `err = capture - edge_offset_ticks` when the edge lands in the first half of the period (after the wrap)
- `err = capture - edge_offset_ticks - period` when it lands in the second half (before the wrap)

The loop drives `err` to 0. The counter therefore wraps exactly on the second boundary, and `TIM2->CNT` is the number of ticks since the top of the second. That is the value a real-time clock needs.

Which period gets the trim is now decided by the same first-half/second-half test (`after_wrap`) instead of the bridge's `err >= 0`. On the bridge the two tests were the same, but with the 10 ms offset `err` can be negative while the edge is still after the wrap. If the edge came after the wrap, the trim goes on the current period, written to ARR right away when CNT is far enough from it. If it came before the wrap, the trim waits in `pending_trim` for the next period.

### Time readout

`ppsTimerGetTimeSinceSecond(uint64_t *ns)` reads `TIM2->CNT` in a critical section. It converts ticks to nanoseconds using the measured ticks per second (`nominal_ticks + freq_q`), not the nominal clock rate, so the timer clock's frequency error doesn't scale the result.

- It returns `false` until the first pulse has set the timer phase.
- While a phase step is being slewed out, a trimmed period can run a little past one second. The result is capped at 999,999,999 ns so it never reports a full second.
- If the counter has wrapped but the update interrupt hasn't run yet, CNT is already the time since the new second, so no extra correction is needed. The bridge's time-since-pulse readout needed one.

Nothing calls this function yet, as the plan asked.

### Freeing PA1 from `I2C_MUX_RESET`

On `bm_mote_spi_v1_0`, PA1 was set up as the push-pull `I2C_MUX_RESET` output and driven high in `bspInit()`. With the ADIN's LED_0 pin also driving that line, the MCU would have been driving against the ADIN. So the pin was taken out of the output setup in `gpio.c`, the `IOWrite` was removed from `bsp.cpp`, and the debug GPIO entry was removed so it can't be driven from the CLI. The bridge's TP10 change was done the same way.

- `bm_soft_module` is the only app built for `bm_mote_spi_v1_0` (the `soft` preset), and it doesn't use the I2C mux.
- The `I2C_MUX_RESET` pin handle in `bsp_pins.c` / `bsp.h` and the `.ioc` file were left as they were.
- `ppsTimerInit()` sets PA1 to AF1 before `bspInit()` runs.

### Low power

TIM2 doesn't run in STOP modes. As on the bridge, `ppsTimerInit()` calls `lpmPeripheralActive(LPM_TIM2)`, which keeps the MCU out of STOP2. The plan allowed this. `LPM_TIM2` already existed in `src/lib/common/lpm.h` from the bridge work.

### Interrupt

`TIM2_IRQHandler` overrides the weak default in the mote startup file. Its priority is `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5 in the mote's `FreeRTOSConfig.h`), the same as the bridge. Nothing else on the mote uses TIM2; the HAL timebase uses TIM8.

## Verification

- The `soft` preset (`APP=bm_soft_module`, `BSP=bm_mote_spi_v1_0`) builds with no errors and no new warnings. The linker's RWX segment warning was already there.
- **Not tested on hardware.** With the part 1 TS_TIMER output running, check:
  - `ppsTimerGetStatus()` should report `acquired` after the second pulse, then `locked`.
  - `pps_count` should go up by one each second, and `rejected_count` should stay flat after start-up.
  - `phase_err_ns` should settle near 0.
  - `freq_offset_ppb` should settle to the timer clock's offset from the ADIN's clock.
  - On a scope, PA1 should show the ADIN pulse with no sign of the MCU driving the line.

The timer follows the ADIN's 1588 clock. It only lines up with GPS time once the ADIN clock is set or steered over PTP, which is part 4.
