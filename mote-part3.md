# BM F+L mote firmware: part 3

This covers part 3 of `mote-bm-f+l_md_claude_prompt.md`: turning on ADIN2111 timestamps for incoming and outgoing packets on the mote (`bm_soft_module`), so IEEE 1588 can sync clocks across the network. Part 4 (the `handle_ptp` task) was out of scope. Nothing has been committed yet.

## Files changed

- **`src/apps/bm_soft_module/CMakeLists.txt`:** adds `bm_adin2111_ptp_enabled=1` to `APP_DEFINES`, the same line the bridge's `CMakeLists.txt` has.

`bm_core` did not need any changes. The driver already supports packet timestamps behind `bm_adin2111_ptp_enabled`, from the bridge work (bm_core commits `bfab4bd` and `e5a1180`).

## What the flag turns on

In `src/lib/bm_core/drivers/adin2111/bm_adin2111.c`:

1. **`adin2111_TsEnable(ADI_MAC_TS_FORMAT_64B_1588)`** runs before `adin2111_SyncConfig()`. It sets `TS_EN` in `TS_CFG` (the 1588 clock) and sets `FTSE` and `FTSS` in `CONFIG0`, which turns on 64-bit receive and transmit frame timestamps.
2. **Egress timestamp callback:** `ADI_MAC_EVT_TIMESTAMP_RDY` is registered. When a frame is sent with `adin2111_ptp_send(..., ADI_MAC_EGRESS_CAPTURE_A/B/C)`, the ADIN captures its send time into `TTSCx` (port 1) or `P2_TTSCx` (port 2). The callback then reports that the timestamp is ready, and `adin2111_ptp_get_egress_timestamp()` reads it.
3. **Ingress path:** frames with ethertype `0x88F7` are passed to the registered PTP `receive` callback along with their receive timestamp, and are not given to L2. While no PTP callbacks are registered (which is the case until part 4), these frames go to the normal receive path, so nothing changes.
4. **`tsCaptPin = ADIN2111_TS_CAPT_MUX_TEST_1`:** the flag also routes TS_CAPT to TEST_1. The mote doesn't use this. The datasheet says TEST_1 has an internal pull-up and should be left unconnected if unused, so the input is harmless. The mote never reads `TS_EXT_CAPT0/1`.

## Interaction with part 1

Part 1 only calls `adin2111_TsEnable(ADI_MAC_TS_FORMAT_NONE)` when `bm_adin2111_ptp_enabled == 0`, so that call is now skipped. The 64-bit 1588 `TsEnable` call above starts the same 1588 clock, so the TS_TIMER setup that follows (`LED_POLARITY`, `LED_CNTRL`, `TS_TIMER_HI/LO/START`) works as before, with frame timestamps now on as well.

## Verification

- The `soft` preset builds with no errors and no new warnings. `bm_adin2111.c` and `l2.c` were rebuilt with the flag on. `compile_commands.json` confirms `bm_adin2111_ptp_enabled=1` is passed to the build.
- **Not tested on hardware.** Part 4 adds the code that sends PTP frames and reads their timestamps. Until then, the expected bench result is:
  - normal Bristlemouth traffic still works
  - the PA1 1 PPS pulse from parts 1 and 2 is still there
