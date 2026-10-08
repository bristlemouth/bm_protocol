# BM F+L bridge firmware: steps 1 and 2

This covers steps 1 and 2 of `bm-f+l_md_claude_prompt.md`: receiving the spotter's PPS-aligned UTC time on the bridge and using it to discipline the ADIN2111 IEEE 1588 timer. The work was committed as `a598acd1` ("Steps 1 and 2 of the bridge firmware change plan for BM F+L done."), and the submodule pointer was fixed in `8d220bfd`. Steps 3 and 4 were out of scope for this session. They came later, in `68153d12` and `cc8b5228`, and those commits also touched some of the files below.

The prompt's rules were: make the smallest change possible, follow existing patterns, change only the bridge app, STOP2 may be disabled, and ignore spotter apps other than `spotter`.

## Step 1: `BM_SERIAL_PTP` handling in bm_serial

The spotter sends `bm_serial_ptp_pub` with:

| Field | Value |
|---|---|
| Topic | `spotter/utc-pps-time` |
| Type / version | 1 / 1 |
| Payload | `bm_common_pub_sub_utc_t` (`uint64_t utc_us`, the UTC of the PPS edge) |

Changes:

- **`src/lib/bm_serial/bm_serial.h`:** added a new callback, `ptp_fn`, at the end of `bm_serial_callbacks_t`. It has the same signature as `pub_fn`.
- **`src/lib/bm_serial/bm_serial.c`:** added `case BM_SERIAL_PTP:` to the receive switch, right after `BM_SERIAL_PUB`. It does the same thing as the PUB case, including the topic-length overflow check that returns `BM_SERIAL_INVALID_TOPIC_LEN`, but calls `_callbacks.ptp_fn(...)`. These messages are handled on the bridge and are not forwarded to the Bristlemouth bus.
- **`src/apps/bridge/app_pub_sub.h`:** added `APP_PUB_SUB_UTC_PPS_TOPIC`, `APP_PUB_SUB_UTC_PPS_TYPE` and `APP_PUB_SUB_UTC_PPS_VERSION` next to the existing `spotter/utc-time` defines.

## Step 2: `bm_serial_adin_utc_cb` in `src/lib/bm_ncp/ncp_uart.cpp`

The callback is registered with `bm_serial_callbacks.ptp_fn = bm_serial_adin_utc_cb;`, next to the other callback assignments.

### What it does on each message

1. **Validate:** check the topic, type, version and payload length. On a mismatch it prints `Unrecognized version...`, the same way the `utc-time` handler does.
2. **Check the device:** if `pDeviceHandle` is NULL or bridge bus power is off, set `time_set = false` and return.
3. **Detect a reset:** read `TS_SEC_CNT`. If it is more than 1 s away from the UTC seconds, the chip was reset or powered up, so set `time_set = false`.
4. **First UTC after boot or reset:**
   - Write the addend using the learned frequency (`freq_acc / KI`).
   - Set seconds and nanoseconds with `adin2111_TsSetTimerAbsolute`.
   - Save the current `TS_EXT_CAPT0/1` as the baseline.
   - Print `Set ADIN2111 timer to ...`.
   - The step-3 hand-off is left as `// TODO - signal the PTP task (step 3) to start the peer delay and Sync handshake`.
5. **Every PPS after that:**
   - Read `TS_EXT_CAPT0` (ns) and `TS_EXT_CAPT1` (sec). TS_CAPT has no status bit, so a capture counts as new only if the value changed. If it didn't, print `No ADIN2111 TS_CAPT timestamp for PPS edge`.
   - Convert the capture with `adin2111_TsConvert(..., ADI_MAC_TS_FORMAT_64B_1588)`.
   - Compute `err = adin2111_TsSubtract(pps_utc, capt)`.
   - If |err| > 1 ms, step the timer with `adin_ptp_step(err)`.
   - Otherwise run the PI loop on `TS_ADDEND`.

### PI loop

The PI loop is modeled on the spotter's `fleet-spotter-fw/src/lib/bm_bridge/bm_ptp.cpp`.

| Constant | Value |
|---|---|
| Kp | 1/4 (`ADIN_PTP_PI_KP_DIV`) |
| Ki | 1/64 (`ADIN_PTP_PI_KI_DIV`) |
| Max slew | 100 ppm (`ADIN_PTP_MAX_SLEW_PPB`), which limits the phase error fed to P |
| Max frequency | 100 ppm (`ADIN_PTP_MAX_FREQ_PPB`), which limits the integrator |

- The error is integrated only when the PPS messages are 1 s apart and the error is in range.
- The output is `freq_acc/KI + err/KP` in ppb.

### Helpers

- **`adin_ptp_set_freq(ppb)`:** writes `TS_ADDEND = nominal + nominal * ppb / 1e9`. Nominal is `RSTVAL_MAC_TS_ADDEND` = `0x85555555`, which is about 0.447 ppb per LSB.
- **`adin_ptp_step(offset_ns)`:** reads SEC, then NS, then SEC again. If the seconds rolled over, it reads NS again. Then it adds the offset and calls `adin2111_TsSetTimerAbsolute`.
- **State** is kept in `AdinPtpContext_t adin_ptp_ctx`: `time_set`, `last_utc_us`, `last_capt_lo/hi` and `freq_acc`.
- **Includes:** added `adin2111.h` and `app_pub_sub.h`, plus `extern adin2111_DeviceHandle_t pDeviceHandle;` inside the existing `extern "C"` block.

### Concurrency

Register access from the NCP task is safe. The SPI bus is protected by a mutex (`protected_spi`), and the MAC driver serializes control transfers with its `pendingCtrl` handshake.

## ADIN2111 datasheet notes

A subagent pulled these facts from `adin2111.pdf`:

| Register | Address | Notes |
|---|---|---|
| `TS_ADDEND` | 0x80 | Timer rate is proportional to the addend |
| `TS_SEC_CNT` | 0x82 | Seconds |
| `TS_NS_CNT` | 0x83 | Nanoseconds; must be a multiple of 16 |
| `TS_CFG` | 0x84 | `TS_EN` is bit 0 |
| `TS_EXT_CAPT0` | 0x89 | Captured ns |
| `TS_EXT_CAPT1` | 0x8A | Captured sec |
| `DIGIO_PINMUX` | PHY1 0x1E8C56 | `TSCAPT` = 10b routes TEST_1, which carries the GPS PPS |

## Decision: how timestamping gets enabled (option B)

**The problem:** the ADI driver's `MAC_TsEnable` sets FTSE and records `timestampFormat`, which the OA SPI receive path (`adi_spi_oa.c`) uses to strip timestamps from received frames. But `MAC_TsEnable` returns an error once `adin2111_SyncConfig` has run. Setting FTSE directly afterwards, without the driver knowing, is unsupported and untested, and is likely to cause problems with 64-bit timestamps.

**The options:**
- **A** (rejected): change the bridge only, enabling the timer and pin mux at runtime from the callback, without frame timestamps.
- **B** (chosen): enable timestamping during ADIN2111 startup in `bm_core`, behind an opt-in flag that only the bridge turns on.

**Option B changes:**

- **`src/lib/bm_core/drivers/adin2111/bm_adin2111.c`** (bm_core submodule):
  - New flag `bm_adin2111_ptp_enabled`, which defaults to 0. It follows the `bm_metrics_enabled` pattern.
  - When the flag is on, `DRIVER_CONFIG` sets `.tsCaptPin = ADIN2111_TS_CAPT_MUX_TEST_1` (otherwise `ADIN2111_TS_CAPT_MUX_NA`). The driver applies this routing at startup and again after a reset.
  - When the flag is on, `adin2111_netdevice_enable` calls `adin2111_TsEnable(&DEVICE_STRUCT, ADI_MAC_TS_FORMAT_64B_1588)` before `adin2111_SyncConfig`. If that call fails, it returns `BmENODEV`.
- **`src/apps/bridge/CMakeLists.txt`:** added `bm_adin2111_ptp_enabled=1` to `APP_DEFINES`. These defines are applied with `add_compile_definitions` before `add_subdirectory(bm_core)`, so they reach bm_core. Every build of `src/apps/bridge` gets the flag, including variants such as raw-pressure. Other apps are unaffected.
- **`ncp_uart.cpp` simplified:** the runtime pin-mux and TS_EN code was removed. Because `TS_EN` is now always set, reset detection switched to comparing `TS_SEC_CNT` with UTC.

TX buffer descriptors are zeroed, so frames don't request a send-time capture. Normal network traffic should therefore be unchanged.

## Build and verification

- **Toolchain:** `pixi` was not on PATH, so I used `/Users/michaelbella/miniconda3-intel/envs/bristlemouth/bin`.
- **Why I avoided the normal build:** both a full `make` and a CMake reconfigure run `git submodule update --init`. That would have tried to move `bm_serial` from commit `d434003` back to the recorded `37beab7`. Uncommitted edits blocked the checkout, so nothing changed. To avoid that path, I built individual objects with `-f build.make`, built libraries with `make <lib>/fast`, and linked with `make bridge_v1_0-bridge-dbg.elf/fast`. I added `-Dbm_adin2111_ptp_enabled=1` through `C_DEFINES`/`CXX_DEFINES` overrides instead of reconfiguring.
- **Result:** no compile errors or warnings. The only linker warning is the RWX-segment warning that was already there. `adin2111_TsEnable` and `MAC_TsEnable` are present in `libbmadin2111.a` and in the final ELF.
- **Editor warnings:** clangd diagnostics about missing `string.h`, `inttypes.h` and `size_t` come from the editor's sysroot. They are not real build errors.

## Not yet verified on hardware

- Network RX/TX still works with FTSE and 64-bit timestamps enabled.
- TS_CAPT captures on the rising edge of PPS, and the capture layout is sec in `CAPT1` and ns in `CAPT0`.
- Timer rate is proportional to `TS_ADDEND`, as assumed.
- **Expected log sequence:** `Set ADIN2111 timer to ...` once, then one `Stepping ADIN2111 timer by ...`, then PI lock with no further steps.
- If `No ADIN2111 TS_CAPT timestamp for PPS edge` repeats every second, check the PPS wiring to TEST_1 and the capture edge.

## Housekeeping

- bm_core and bm_serial are separate repos, so their changes need their own commits or PRs, and the parent repo's submodule pointers must be updated (done in `8d220bfd` for this branch).
- Memory note saved: only update the submodules that are actually needed, and avoid deep `--recursive` pulls (for example, mcuboot's esp-idf).
