# Bridge firmware: GPS-disciplined ADIN2111 IEEE 1588 time and PPS-locked TIM2

This is a complete account of the work on branch `mbella/claude_bridge_fw_ptp` (base: `develop`) for the BM F+L prototype. It combines the session notes for each step (`parts-1-and-2.md`, `part3.md`, `part4.md`) with the commit history of this repo and of the `bm_core` and `bm_serial` submodules.

## Goal

The bridge must carry GPS time onto the Bristlemouth network with roughly 100 µs accuracy. The work had four steps, all set out in `bm-f+l_md_claude_prompt.md`, which is committed on the branch:

1. Receive the spotter's UTC timestamp for each GPS PPS edge over bm_serial, as a new `BM_SERIAL_PTP` message on topic `spotter/utc-pps-time`.
2. Use that timestamp to set the ADIN2111 IEEE 1588 timer, then discipline the timer in phase and frequency with a PI loop. The loop compares each UTC value with the ADIN2111's own `TS_CAPT` snapshot of the same PPS edge.
3. Run IEEE 1588 peer-to-peer delay measurement and two-step Sync/Follow_Up to the neighboring node in a new FreeRTOS task, using the ADIN2111 hardware ingress and egress timestamps.
4. Capture the same PPS on PA0 / TIM2_CH1 and run a PI loop that disciplines TIM2 itself, so the MCU has a PPS-locked timebase.

The prompt set these rules for every step:
- Make the smallest, least invasive change, follow existing patterns, and reuse existing utilities.
- Change the bridge app only. Spotter apps other than `spotter` can be ignored.
- STOP2 may be disabled.
- `bm_core` and `bm_serial` may be modified, because they stay on experimental branches.

## Commit map

### bm_protocol (parent repo)

| Commit | Content |
|---|---|
| `a598acd1` | Steps 1 and 2: `ncp_uart.cpp` PPS/UTC callback and PI loop, `app_pub_sub.h` topic defines, `bm_adin2111_ptp_enabled=1` in the bridge CMake, bm_serial pointer bump |
| `8d220bfd` | Fixed the bm_core pointer so it includes `bfab4bd` (timestamp enable / TS_CAPT mux) |
| `68153d12` | Step 3: new `adin_ptp.cpp/.h` task, wired into `app_main.cpp`, `task_priorities.h`, CMake and `ncp_uart.cpp`; bm_core pointer bump; adds the prompt file |
| `cc8b5228` | Step 4: new `pps_timer.cpp/.h`, `app_main.cpp` init, CMake filter options, `LPM_TIM2`, PA0 removed from the GPIO output init |

Net diff against `develop`: 14 files, +1303 / −5.

### bm_core (`f06b3b4` → `e5a1180`, branch `mbella_claude_ptp_changes`)

| Commit | Content |
|---|---|
| `bfab4bd` | `bm_adin2111_ptp_enabled` flag, TS_CAPT mux on TEST_1, `adin2111_TsEnable(64B_1588)` before `SyncConfig` |
| `e5a1180` | PTP RX/egress callbacks, raw PTP send with egress capture, egress timestamp read, `bm_l2_run_in_thread` |

### bm_serial (`37beab7` → `a7029f4`)

| Commit | Content |
|---|---|
| `7ddfca2` | Upstream merge of the metrics request/reply PR (#33). It replaces the pre-squash commit `37beab7` that the parent repo used to point to. |
| `d434003` | Sender side, used by the spotter: `BM_SERIAL_PTP = 0x0C` message type and `bm_serial_ptp_pub()` |
| `a7029f4` | Receiver side, used by the bridge: `ptp_fn` callback and the `case BM_SERIAL_PTP:` handler |

---

## Step 1: receiving `BM_SERIAL_PTP` over bm_serial

The spotter publishes the UTC time of each PPS rising edge with `bm_serial_ptp_pub`:

| Field | Value |
|---|---|
| Topic | `spotter/utc-pps-time` |
| Type / version | 1 / 1 |
| Payload | `bm_common_pub_sub_utc_t`, a `uint64_t utc_us` holding the UTC of the PPS edge |

`bm_serial_ptp_pub` is a copy of `bm_serial_pub` that sends message type `BM_SERIAL_PTP` instead of `BM_SERIAL_PUB`. The bridge therefore handles the message itself and does not forward it onto the Bristlemouth bus.

Changes:
- **`bm_serial_messages.h`** (`d434003`): adds `BM_SERIAL_PTP = 0x0C`.
- **`bm_serial.h`** (`a7029f4`): adds `ptp_fn` to the end of `bm_serial_callbacks_t`, with the same signature as `pub_fn`.
- **`bm_serial.c`** (`a7029f4`): adds `case BM_SERIAL_PTP:` right after `BM_SERIAL_PUB`. It parses the pub header the same way, including the topic-length overflow check (`BM_SERIAL_INVALID_TOPIC_LEN`), and calls `_callbacks.ptp_fn(...)`.
- **`src/apps/bridge/app_pub_sub.h`**: adds `APP_PUB_SUB_UTC_PPS_TOPIC/TYPE/VERSION` next to the existing `spotter/utc-time` defines.

## Step 2: setting and disciplining the ADIN2111 timer

### Enabling timestamping in bm_core (option B)

**The problem.** The ADI driver's `MAC_TsEnable` does two things:
- sets FTSE, which turns on frame timestamping;
- records `timestampFormat`, which the OA SPI receive path (`adi_spi_oa.c`) needs in order to strip timestamps from received frames.

`MAC_TsEnable` refuses to run once `adin2111_SyncConfig` has run. Setting FTSE behind the driver's back afterwards is unsupported, and it would probably break RX framing with 64-bit timestamps.

**The options.**
- Option A (rejected): change only the bridge, enabling the timer and pin mux at runtime without frame timestamps. Step 3 needs frame timestamps, so this was not enough.
- Option B (chosen): enable timestamping during ADIN2111 startup in bm_core, behind an opt-in flag.

**What was changed.** Commit `bfab4bd` makes these changes in `drivers/adin2111/bm_adin2111.c`:
- **New flag:** `bm_adin2111_ptp_enabled`, which defaults to 0 and follows the `bm_metrics_enabled` pattern.
- **Pin mux:** with the flag on, `DRIVER_CONFIG.tsCaptPin = ADIN2111_TS_CAPT_MUX_TEST_1`. TEST_1 carries the GPS PPS. The driver applies this routing at init and again after a reset.
- **Timestamp enable:** with the flag on, `adin2111_netdevice_enable()` calls `adin2111_TsEnable(&DEVICE_STRUCT, ADI_MAC_TS_FORMAT_64B_1588)` before `adin2111_SyncConfig`. A failure returns `BmENODEV`.
- **Build wiring:** `src/apps/bridge/CMakeLists.txt` adds `bm_adin2111_ptp_enabled=1` to `APP_DEFINES`. These defines are applied before `add_subdirectory(bm_core)`, so bm_core sees them. Every bridge build variant gets the flag, and no other app does.

TX buffer descriptors are zero-initialized. Normal frames therefore do not request egress captures, and ordinary traffic is unchanged.

### `bm_serial_adin_utc_cb` (`src/lib/bm_ncp/ncp_uart.cpp`)

The callback is registered as `bm_serial_callbacks.ptp_fn`. It reaches the driver through `extern adin2111_DeviceHandle_t pDeviceHandle`.

On each message it does the following:

1. **Validate.** Check the topic, type, version and payload size. A mismatch prints `Unrecognized version...`, matching the `utc-time` handler.
2. **Check the device.** If `pDeviceHandle` is NULL or bus power is off, clear `time_set` and return. The ADIN2111 is reset when the bus powers back up.
3. **Detect a reset.** Read `TS_SEC_CNT`. If it is more than 1 s away from the UTC seconds, the chip was reset (its timer restarts from 0), so clear `time_set`.
4. **First UTC after boot or reset:**
   - Write `TS_ADDEND` from the learned frequency (`freq_acc / KI`). The learned frequency survives resets.
   - Set the timer with `adin2111_TsSetTimerAbsolute`.
   - Save the current `TS_EXT_CAPT0/1` as the baseline.
   - Print `Set ADIN2111 timer to …`.
   - Call `adinPtpTimeSet()` to start step 3. This call filled the TODO left in step 2.
5. **Every later PPS:**
   - Read `TS_EXT_CAPT0` (ns) and `TS_EXT_CAPT1` (sec). TS_CAPT has no status bit, so a capture counts as new only if the value changed. Otherwise the callback prints `No ADIN2111 TS_CAPT timestamp for PPS edge`.
   - Convert the capture with `adin2111_TsConvert(..., 64B_1588)`.
   - Compute `err = TsSubtract(pps_utc, capt)`, which is UTC − ADIN. A positive value means the ADIN clock is behind.
   - If |err| > 1 ms, step the timer by `err` with `adin_ptp_step` and print `Stepping ADIN2111 timer by …`.
   - Otherwise apply the PI loop below.

### PI loop

The loop is modeled on the spotter's `bm_ptp.cpp`. Because the PPS period is 1 s, a phase error in ns is also a frequency error in ppb.

| Parameter | Value |
|---|---|
| Kp | 1/4 (`ADIN_PTP_PI_KP_DIV`) |
| Ki | 1/64 (`ADIN_PTP_PI_KI_DIV`), giving an overdamped loop with a time constant of about 10 s |
| Max slew (P term) | 100 ppm. The phase error fed to P is clamped to ±400 µs. |
| Max frequency (I term) | ±100 ppm (integrator clamp) |
| Step threshold | 1 ms |

- **Integrating:** the error is integrated only when it is in range and the previous message was exactly 1 s earlier. This prevents windup while slewing and after a gap in PPS.
- **Output:** `ppb = freq_acc/KI + clamp(err)/KP`.
- **Writing the result:** `adin_ptp_set_freq(ppb)` writes `TS_ADDEND = nominal + nominal·ppb/1e9`, where nominal is `RSTVAL_MAC_TS_ADDEND = 0x85555555` (about 0.447 ppb per LSB).
- **Sign check:** the sign was re-checked during step 3 and is correct. When ADIN is behind, err > 0, ppb > 0, the addend rises and the clock speeds up.
- **Stepping:** `adin_ptp_step` reads SEC, NS, SEC and re-reads NS if the seconds rolled over. It then adds the offset and calls `TsSetTimerAbsolute`. The driver briefly stops the clock while doing this and rounds ns down to a multiple of 16. The PI loop removes the leftover error.

All state is kept in `AdinPtpContext_t adin_ptp_ctx` (`time_set`, `last_utc_us`, `last_capt_lo/hi`, `freq_acc`).

### ADIN2111 datasheet notes used

| Register | Address | Notes |
|---|---|---|
| `TS_ADDEND` | 0x80 | Timer rate is proportional to the addend |
| `TS_SEC_CNT` | 0x82 | Seconds |
| `TS_NS_CNT` | 0x83 | Nanoseconds; must be a multiple of 16 |
| `TS_CFG` | 0x84 | `TS_EN` is bit 0 |
| `TS_EXT_CAPT0` | 0x89 | Captured ns |
| `TS_EXT_CAPT1` | 0x8A | Captured sec |
| `DIGIO_PINMUX` | PHY1 0x1E8C56 | `TSCAPT = 10b` selects TEST_1 |
| `TTSC{A,B,C}{L,H}` / `P2_TTSC…` | n/a | Per-port egress timestamp capture slots A, B and C |

The timestamp point is SFD detection in the PHY (`CONFIG2.SFD_DETECT_SRC = 0`).

---

## Step 3: IEEE 1588 peer delay and Sync/Follow_Up task

### bm_core support (`e5a1180`)

**`network/l2.h/.c`** adds `bm_l2_run_in_thread(L2ThreadFn fn, void *arg)`:
- It queues a new `L2Fn` element, and the L2 thread then calls `fn(arg)`.
- It returns `BmENOMEM` if the element can't be queued within 10 ms.
- It exists because the ADI driver's TX queue is not thread-safe, so all PTP TX and egress-timestamp reads have to run on the L2 thread.

**`drivers/adin2111/bm_adin2111.h/.c`** adds the following, all behind `bm_adin2111_ptp_enabled`:
- **`Adin2111PtpCallbacks`:** `receive(port, data, len, rx_ts)` and `egress_timestamp_ready(port, capture)`.
- **`ADIN2111_ETHERTYPE_PTP` (0x88F7):** `receive_callback` sends frames with this ethertype to the PTP callback, together with their converted RX timestamp (NULL if invalid). These frames do not go to L2. All other frames go to L2 as before.
- **Egress capture argument:** `adin2111_netdevice_send` gains `adi_mac_EgressCapture_e capture`, written into `egressCapt`. The normal netdevice path passes `ADI_MAC_EGRESS_CAPTURE_NONE`.
- **Ready callback:** `egress_timestamp_ready_callback_` is registered for `ADI_MAC_EVT_TIMESTAMP_RDY`, after `TsEnable` as the driver requires. It fans the per-port A/B/C ready flags out to the PTP callback.
- **New public functions:** `adin2111_ptp_register_callbacks`, `adin2111_ptp_send(frame, len, port, capture)` and `adin2111_ptp_get_egress_timestamp(port, capture, &ts)`. The last one reads the TTSC registers and converts the value.

### Bridge task (`src/apps/bridge/adin_ptp.cpp/.h`)

**Task setup**
- The task is `ADIN_PTP`, with 1024 words of stack and `ADIN_PTP_TASK_PRIORITY 6`.
- `adinPtpInit(&bridge_power_controller)` is called in `app_main.cpp` just before `ncpInit`.
- The bridge is the grandmaster. There are no Announce messages and no BMCA.

**Coordination with the step 2 callback**
- The callback signals the task with a direct-to-task notification (`xTaskNotify`, `eSetBits`). The FreeRTOS docs (V10.5.1 `task.h`) prefer this over a semaphore or event group when there is a single receiver.
- The notification bits are `TIME_SET`, `EVT` and `LINK`.
- Received frames and egress timestamps travel through an 8-deep queue of `PtpEvt_t`. The notification only wakes the task.

**Transport**
- IEEE 802.3 / Ethernet (Annex F), ethertype 0x88F7, PTPv2, domain 0. Frames are padded to 60 bytes.
- Pdelay messages go to `01:80:C2:00:00:0E`. Sync and Follow_Up go to `01:1B:19:00:00:00`.
- The clock identity is `node_id()` (big-endian), and the port number is 1 or 2. The source MAC comes from `mac_address()`.

**Egress capture slots**

| Slot | Used for |
|---|---|
| A | Sync |
| B | Pdelay_Req (t1) |
| C | Pdelay_Resp (t3) |

**Per-port state machine.** Every port is both initiator and responder.
- **Initiator:**
  - Sends Pdelay_Req and collects t1 (egress), t2/t4 (from Pdelay_Resp and its RX timestamp) and t3 (from Pdelay_Resp_Follow_Up).
  - Sequence ID, requesting port identity and responder identity are all checked.
  - `link_delay = ((t4−t1) − (t3−t2) − corr) / 2`, accepted only within ±100 µs.
- **Responder:**
  - Answers Pdelay_Req with a two-step Pdelay_Resp carrying t2 (the RX timestamp).
  - Once the slot C egress timestamp is ready, sends Pdelay_Resp_Follow_Up carrying t3 and echoing the requester's correction field.
- **Sync:**
  - Two-step Sync every 1 s, with the originTimestamp left zero.
  - The slot A egress timestamp then goes out in the Follow_Up.
  - Sync goes only to "capable" ports, meaning the neighbor answers Pdelay. The exception is one forced Sync right after the time is set or a link comes up.
- **Latency constants:** `ADIN_PTP_TX_LATENCY_NS` and `ADIN_PTP_RX_LATENCY_NS` are both 0. With ADIN2111s on both ends of a link these latencies cancel. The earlier 3200/6400 ns guesses were removed.

**Timeouts, retry and back-off.** These cover the "don't flood the network" requirement.

| Item | Value or behavior |
|---|---|
| Pdelay interval | 1 s |
| Pdelay response timeout | 500 ms |
| Egress-timestamp timeout | 100 ms |
| Back-off | After 3 lost responses the port stops being capable and the Pdelay interval doubles each time, up to 32 s |
| Back-off reset | A received Pdelay_Req or a successful exchange resets the interval to 1 s and retries immediately |
| Wait time | The task sleeps in `xTaskNotifyWait` for exactly the time to the next deadline (`ticks_to_wait`), and forever when nothing is scheduled |

**Lifecycle events**
- **Link changes:** come from `bm_l2_register_link_change_callback`, and restart that port's state.
- **Bus power off:** marks the time invalid, resets all ports and flushes the queue. Nothing is sent again until `bm_serial_adin_utc_cb` sets the time again after power-up.
- **`TIME_SET`:** restarts every linked port and starts Pdelay and Sync immediately.

**Other wiring:** `ncp_uart.cpp` includes `adin_ptp.h`, `task_priorities.h` gains the new priority, and `CMakeLists.txt` adds the source file.

---

## Step 4: TIM2 disciplined to PPS (`src/apps/bridge/pps_timer.cpp/.h`)

**Hardware setup**
- PA0 is set to AF1 (TIM2_CH1).
- TIM2 is the 32-bit timer, clocked from the PCLK1 timer clock with no prescaler. The clock is about 160 MHz, doubled when APB1 is divided.
- ARR is set so the counter wraps once per second, so `TIM2->CNT` counts ticks since the start of the current second.
- ARR preload is off (ARPE = 0), so a trim can affect the current period.

**Capture**
- CH1 captures directly from TI1 on the rising edge (CC1S = TI1, CC1E, CC1IE, CCR1, CC1IF/CC1OF).
- The prompt asked for CH2 in one place. The first version used CH2 in indirect mode, and a follow-up changed everything to CH1 because PA0 is TIM2_CH1.

**Interrupt**
- `TIM2_IRQHandler` handles both the update (wrap) and the CC1 capture.
- If both are pending and the capture is in the second half of the period, the capture is processed first, because it happened before the wrap.
- The NVIC priority is `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`.

**Control loop.** The input is the phase error, the capture relative to the wrap; a positive value means the timer is ahead.

| Term | Behavior |
|---|---|
| P | `err/4`, clamped to ±100 ppm of the period. Large errors are slewed and never stepped once acquired. |
| I | `err/64` is added to the frequency estimate, only inside the lock window (400 µs), to avoid windup |
| FLL assist | The measured PPS interval pulls the frequency estimate 1/16 of the way each edge, rate-limited to 1 ppm per edge, and only when both edges of the interval were accepted |

- The frequency estimate is fixed-point with 8 fractional bits and is clamped to ±10,000 ppm, the range expected for the MSI-based clock.
- **Applying the trim:** if the edge came after the wrap and `CNT` is at least 1 ms short of the new ARR, ARR is written for the current period. Otherwise the trim goes into `pending_trim`, which the next update applies.

**Robustness**
- **Acquisition:** the first good edge, which needs a valid 1 s interval, sets the frequency and steps the phase directly. Nothing uses the timer yet at that point.
- **Holdover:** after 3 periods with no PPS the timer is no longer locked and runs on its last frequency estimate. A stale phase trim is never left applied.
- **PPS returns:** the phase slews back at ≤100 ppm and the frequency moves ≤1 ppm per edge, so there are no sudden period jumps.
- **Edges that are rejected:**
  - intervals more than 10,000 ppm from 1 s;
  - captures flagged by an overcapture (CC1OF);
  - while locked, up to 3 consecutive out-of-window edges (glitch rejection). After that the loop accepts the new phase.

**Status:** `ppsTimerGetStatus()` returns `acquired`, `locked`, `phase_err_ns`, `freq_offset_ppb`, `pps_count` and `rejected_count`. It copies the state inside a critical section.

**Input filter.** The filter is off by default, because it adds capture latency.
- `PPS_TIMER_INPUT_FILTER` (0–15) sets IC1F, and `PPS_TIMER_INPUT_FILTER_CKD` (0–2) sets CR1.CKD.
- Both can be set from CMake, for example `-DPPS_TIMER_INPUT_FILTER=4 -DPPS_TIMER_INPUT_FILTER_CKD=1`, which passes them through `APP_DEFINES`. They can also be overridden in `pps_timer.h`.
- `static_assert`s check the ranges.

**Supporting changes**
- `app_main.cpp` calls `ppsTimerInit()` right after `lpmPeripheralActive(LPM_BOOT)`, and removes the `tp10` debug GPIO entry.
- `src/bsp/bridge_v1_0/Core/Src/gpio.c` removes `TP10_Pin` (PA0) from the output init, so the pin no longer drives against PPS. CRLF line endings were kept.
- `src/lib/common/lpm.h` adds `LPM_TIM2 (1UL << 11)`. It is held active, which keeps the MCU out of STOP1/STOP2, because TIM2 stops in STOP modes. The prompt allowed this.

---

## End-to-end data flow

```
GPS PPS ─┬─► spotter MCU ── NMEA+PPS ──► bm_serial BM_SERIAL_PTP "spotter/utc-pps-time" (utc_us)
         │                                     │
         │                                     ▼
         │                        ncp_uart.cpp bm_serial_adin_utc_cb   (NCP task)
         │                          first: set ADIN timer ──► adinPtpTimeSet() ──notify──► ADIN_PTP task
         │                          then: PI on TS_ADDEND using TS_EXT_CAPT
         ├─► ADIN2111 TEST_1 (TS_CAPT) ── snapshot of 1588 timer ──┘
         │
         └─► bridge PA0 / TIM2_CH1 ──► TIM2_IRQHandler PI+FLL ──► PPS-locked TIM2 (ppsTimerGetStatus)

ADIN_PTP task ──bm_l2_run_in_thread──► L2 thread ──adin2111_ptp_send (egress capture A/B/C)──► wire
wire ──► ADIN2111 RX (ingress ts) ──► bm_adin2111 receive_callback (0x88F7) ──► PtpEvt queue ──► ADIN_PTP task
```

## Build and test status

- **Steps 1–2:** built object by object, because a full `make` or CMake reconfigure runs `git submodule update --init`, which would have moved `bm_serial` while edits were uncommitted.
  - The toolchain came from `~/miniconda3-intel/envs/bristlemouth/bin`, because `pixi` was not on PATH.
  - There were no compile warnings, and `adin2111_TsEnable` was confirmed to be in the ELF.
- **Step 3:** `cd preset-builds/bridge && make -j8` builds cleanly. The bm_core host tests pass (190/190).
- **Step 4:** `make` builds cleanly. The only warning is the RWX LOAD segment linker warning that was already there.
- **Hardware:** none of the steps has been tested on hardware yet.

### Hardware bring-up checklist

- **Network traffic:** normal RX/TX still works with FTSE and 64-bit timestamps enabled.
- **TS_CAPT:**
  - It triggers on the PPS rising edge, with sec in `CAPT1` and ns in `CAPT0`.
  - If `No ADIN2111 TS_CAPT timestamp for PPS edge` repeats, check the TEST_1 wiring and the capture edge.
- **ADIN servo log sequence:** expect one `Set ADIN2111 timer to …`, then about one `Stepping ADIN2111 timer by …`, then a PI lock with no further steps.
- **Addend:** confirm the timer rate is proportional to `TS_ADDEND`, as assumed.
- **PTP logs:** expect `PTP starting, ADIN2111 timer set`, then `PTP port N: neighbor answering peer delay, link delay … ns`. Check that link delays are plausible (well under 100 µs).
- **Back-off:** with no neighbor, Pdelay requests should back off to one every 32 s.
- **TIM2:** `ppsTimerGetStatus()` should show `acquired`, then `locked`, with `rejected_count` staying low. Pulling the PPS should give holdover with no period jump.

## Known caveats and follow-ups

- **Possible register-access race:** step 2's PPS handler reads and writes ADIN registers from the NCP task, while the L2 thread may also be using the driver. SPI is mutex-protected and control transfers are serialized by `pendingCtrl`, but moving this work onto the L2 thread with `bm_l2_run_in_thread` would remove the risk entirely.
- **No Announce or BMCA:** the bridge is assumed to be the master. Downstream nodes need their own slave-side implementation to consume Sync/Follow_Up.
- **Timer stepping:** `adin2111_TsSetTimerAbsolute` stops the clock briefly and truncates ns to a multiple of 16. The effect is minor and the servo absorbs it.
- **MSI clock:** the MCU clock is MSI → PLL, not locked to the LSE. Enabling MSIPLLEN would reduce the wander the TIM2 loop has to track.
- **CubeMX:** the `.ioc` file was not updated. Regenerating from CubeMX would turn PA0 back into the TP10 output.
- **TIM2 status not exposed:** `ppsTimerGetStatus()` is not yet wired to a CLI command or to logging.
- **TIM2 not cross-referenced:** nothing yet uses the disciplined TIM2, for example to cross-reference it against the ADIN2111 PTP time.
- **Submodule branches:** bm_core and bm_serial changes live on their own branches, and the parent repo only records their pointers. Any PR needs matching submodule PRs. Pull only the submodules you need, and avoid deep `--recursive` updates, such as mcuboot's esp-idf.
- **`part3.md` is out of date on one point:** it says step 3 is uncommitted, but it has since been committed as `68153d12` with bm_core `e5a1180`.
