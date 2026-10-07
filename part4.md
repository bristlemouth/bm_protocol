# Step 4: TIM2 disciplined to the GPS PPS on the bridge

Branch: `mbella/claude_bridge_fw_ptp`. Committed as `cc8b5228`. No submodule changes were needed.

## What was asked
- Step 4 of `bm-f+l_md_claude_prompt.md` only:
  - Set up PA0 as TIM2_CH1 input capture, with an interrupt on the rising PPS edge.
  - Leave the input filter off by default, but make it easy to turn on and configure.
  - Add a PI loop that trims the timer so its phase and frequency lock to the PPS.
  - Handle losing and regaining GPS lock without sudden timing changes or large period swings.
- Make the smallest, non-invasive change that follows the existing patterns. Only the bridge app is in scope, and it is fine to disable STOP2.
- **Follow-up fix:** the prompt said PA0 is TIM2_CH1 but also asked to capture on CH2. The first version used CH2 in indirect mode (capturing TI1). It was then changed so everything uses CH1 directly: CC1S = TI1, the IC1F filter, CC1E, CC1IE, CCR1 and CC1IF/CC1OF.

## How it works
- **Timer setup:**
  - TIM2 is 32-bit and runs from the PCLK1 timer clock (160 MHz) with no prescaler.
  - ARR is set so the counter wraps once per second. `TIM2->CNT` then counts timer ticks since the start of the current second.
  - ARR preload is disabled (ARPE = 0), so a period trim can take effect in the current period.
- **Capture:**
  - TIM2_CH1 captures the rising edge on PA0 (AF1) into CCR1.
  - The phase error is the capture value relative to the wrap. A positive error means the timer is ahead.
- **Interrupt:**
  - `TIM2_IRQHandler` handles both update (wrap) and CC1 capture.
  - When both are pending at once, a capture in the second half of the period is processed before the update, because it happened before the wrap.
  - The NVIC priority is `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`.
- **Control loop:** a PI loop on the phase error, plus a frequency-locked-loop (FLL) assist.
  - **Proportional term:** `err / 4`, clamped to ±100 ppm of the period. Larger errors are slewed out at that rate and are never stepped.
  - **Integral term:** `err / 64` is added to the frequency estimate, but only inside the lock window. This prevents windup while slewing.
  - **FLL:** the measured PPS interval moves the frequency estimate 1/16 of the way each edge, rate limited to 1 ppm per edge. It only runs when both edges of the interval were accepted.
  - The frequency estimate uses 8 fractional bits and is clamped to ±10,000 ppm. That is the range expected from the MSI-based clock.
- **Applying the trim:**
  - When the edge came after the wrap and the counter is at least 1 ms from the new ARR, ARR is written immediately for the current period.
  - Otherwise the trim is held in `pending_trim` and applied by the next update.
- **Acquisition:** the first good PPS edge after boot (which needs a valid one-second interval) sets the frequency and steps the phase directly. Nothing uses the timer yet at that point.
- **Lock, holdover and glitches:**
  - **Locked** means the phase error is inside the lock window, which is the max slew × Kp, or 400 µs.
  - **Holdover:** after 3 periods with no PPS, the timer drops out of lock and keeps running on its last frequency estimate. A stale phase correction is never left applied.
  - **When PPS returns:** the phase slews back at ≤100 ppm and the frequency moves ≤1 ppm per edge, so the period never jumps.
  - **Rejected edges:**
    - Edges whose interval is more than 10,000 ppm from one second.
    - Edges flagged by a capture overrun (CC1OF).
    - While locked, up to 3 consecutive edges outside the lock window. This is glitch rejection; after 3 the loop accepts the new phase.
- **Status:** `ppsTimerGetStatus()` returns `acquired`, `locked`, `phase_err_ns`, `freq_offset_ppb`, `pps_count` and `rejected_count`. It takes a snapshot of the state inside a critical section.

## Input filter configuration
The input filter is off by default, because the filter delays the capture.
- `PPS_TIMER_INPUT_FILTER` (0–15) sets TIMx_CCMR1 IC1F. 0 disables the filter.
- `PPS_TIMER_INPUT_FILTER_CKD` (0–2) sets TIMx_CR1 CKD, the filter sampling clock divider (÷1, ÷2 or ÷4).
- Both can be set from cmake, for example `-DPPS_TIMER_INPUT_FILTER=4 -DPPS_TIMER_INPUT_FILTER_CKD=1`. They can also be overridden in `pps_timer.h`.
- `static_assert`s check both ranges.

## Files changed
- **`src/apps/bridge/pps_timer.h` / `pps_timer.cpp`** (new). These hold the timer and capture setup, the ISR, the PI/FLL loop and the status getter.
- **`src/apps/bridge/app_main.cpp`:**
  - Includes `pps_timer.h`.
  - Calls `ppsTimerInit()` right after `lpmPeripheralActive(LPM_BOOT)`.
  - Removes the `tp10` debug GPIO entry, because PA0 is now the PPS input.
- **`src/apps/bridge/CMakeLists.txt`:** adds `pps_timer.cpp`, and passes `PPS_TIMER_INPUT_FILTER` / `PPS_TIMER_INPUT_FILTER_CKD` through to `APP_DEFINES` when they are defined.
- **`src/lib/common/lpm.h`:** adds `LPM_TIM2 (1UL << 11)`. `ppsTimerInit()` marks it active, which keeps the MCU out of STOP1/STOP2, because TIM2 stops in STOP modes.
- **`src/bsp/bridge_v1_0/Core/Src/gpio.c`:** removes `TP10_Pin` (PA0) from the GPIO output init, so the pin is no longer driven against the PPS signal. The file's CRLF line endings are preserved.

## Verification
- **Bridge firmware:** builds cleanly with `cd preset-builds/bridge && make`. There are no new warnings; only the existing RWX LOAD segment linker warning remains.
- **Hardware:** not tested yet.

## Open items
- **MSI accuracy:** the system clock is MSI → PLL, and MSI is not locked to the LSE. Turning on MSI PLL mode (MSIPLLEN) would cut the short-term wander the loop has to track. It wasn't done, to keep the change minimal.
- **CubeMX:** the `.ioc` file was not updated. Regenerating code from CubeMX would turn PA0 back into the TP10 output.
- **CLI:** `ppsTimerGetStatus()` is not wired to a CLI command or to logging yet.
- **Using the timer:** nothing consumes the disciplined timer yet, for example to timestamp against the ADIN2111 PTP time from step 3.
