# Step 3: IEEE 1588 peer delay and Sync/Follow_Up on the bridge

Branch: `mbella/claude_bridge_fw_ptp`. The bm_core submodule is on branch `mbella_claude_ptp_changes`.
None of these changes are committed yet.

## What was asked
- Step 3 of `bm-f+l_md_claude_prompt.md` only:
  - Add a new FreeRTOS task on the bridge that runs IEEE 1588 peer-to-peer delay and two-step Sync/Follow_Up, using the ADIN2111 hardware timestamps.
  - The task is signaled by `bm_serial_adin_utc_cb`. The first signal starts Sync/Follow_Up.
  - It must time out and retry without flooding the network.
- Fill in the one TODO in `src/lib/bm_ncp/ncp_uart.cpp` with the coordination method the FreeRTOS docs prefer.
- Make the smallest, non-invasive change that follows the existing patterns. Only the bridge app is in scope.

## Coordination method (the TODO)
- The callback and the task coordinate with a **direct-to-task notification** (`xTaskNotify` with `eSetBits`).
  - The FreeRTOS docs in `third_party/FreeRTOS/Source/include/task.h` (V10.5.1) recommend task notifications over a binary semaphore or event group when a single task is the receiver. They are faster and use less RAM.
  - `eSetBits` lets the notification value work as a small event group. Its bits are `TIME_SET`, `EVT` and `LINK`.
- Data (received frames and egress timestamps) goes through a queue that is 8 deep. The notification only wakes the task.
- The TODO in `ncp_uart.cpp` was replaced with `adinPtpTimeSet();`, which runs right after the ADIN2111 timer is first set from UTC.

## Files changed

### bm_protocol
- **`src/apps/bridge/adin_ptp.h` / `adin_ptp.cpp`** (new). These hold the `ADIN_PTP` task: 1024 words of stack, priority 6.
  - **Message types:** Pdelay_Req, Pdelay_Resp and Pdelay_Resp_Follow_Up (two-step), plus Sync and Follow_Up, all over raw Ethernet (ethertype 0x88F7).
    - Peer delay messages go to `01:80:C2:00:00:0E`.
    - Sync and Follow_Up go to `01:1B:19:00:00:00`.
  - **Port state:** each port keeps its own initiator, responder and Sync state.
  - **Capture slots:** A is used for Sync, B for Pdelay_Req and C for Pdelay_Resp.
  - **Link delay:** computed as `((t4−t1) − (t3−t2) − corr) / 2`. It is accepted only within ±100 µs.
  - **Timing and retry:**
    - Sync goes out every 1 s.
    - Pdelay starts at a 1 s interval. After 3 missed responses it backs off, up to a maximum of 32 s, so a dead link isn't flooded.
    - The response timeout is 500 ms and the egress-timestamp timeout is 100 ms.
  - **When Sync is sent:** only to ports with a valid peer delay. The exception is a forced first Sync after the time is set or a link comes up.
  - **Bus power off:** resets the port state, marks the time invalid and empties the queue.
  - **ADIN access:** TX and egress-timestamp reads run on the bm_core L2 thread through `bm_l2_run_in_thread`, because the ADI driver's TX queue is not thread-safe.
  - **Link changes:** come from `bm_l2_register_link_change_callback`.
  - **Clock identity:** `node_id()` (big-endian). The source MAC comes from `mac_address()`.
  - **Latency constants:** `ADIN_PTP_TX_LATENCY_NS` and `ADIN_PTP_RX_LATENCY_NS` are both set to **0**. The timestamp point is SFD detection in the PHY (`CONFIG2.SFD_DETECT_SRC` = 0). With ADIN2111s on both ends of a link these latencies cancel, so the constants only matter for correcting a measured asymmetry.
- **`src/apps/bridge/app_main.cpp`:** calls `adinPtpInit(&bridge_power_controller)` before `ncpInit(...)`.
- **`src/apps/bridge/task_priorities.h`:** adds `ADIN_PTP_TASK_PRIORITY 6`.
- **`src/apps/bridge/CMakeLists.txt`:** adds `adin_ptp.cpp` to the build.
- **`src/lib/bm_ncp/ncp_uart.cpp`:** includes `adin_ptp.h` and calls `adinPtpTimeSet()` (the TODO).

### bm_core submodule
- **`network/l2.h` / `l2.c`:** add `bm_l2_run_in_thread(L2ThreadFn fn, void *arg)`.
  - This adds a new `L2Fn` queue element that runs a function on the L2 thread.
  - It returns `BmENOMEM` if the item can't be queued within 10 ms.
- **`drivers/adin2111/bm_adin2111.h` / `.c`** (all inside `bm_adin2111_ptp_enabled`):
  - New callbacks for PTP receive and egress timestamps (`Adin2111PtpCallbacks`).
  - Received 0x88F7 frames go to the PTP callback with their RX timestamp. All other frames still go to L2.
  - `adin2111_netdevice_send` gains an egress capture argument. The normal send path passes `NONE`.
  - New functions: `adin2111_ptp_register_callbacks`, `adin2111_ptp_send` and `adin2111_ptp_get_egress_timestamp`.
  - The egress timestamp-ready callback is registered after `adin2111_TsEnable`.

## Verification
- **Bridge firmware:** builds cleanly with `cd preset-builds/bridge && make -j8`.
- **bm_core host tests:** pass, 190/190.
- **Hardware:** not tested yet.

## Questions answered along the way
- **Timestamp point:** the ADIN2111 datasheet does say where the timestamp is taken: SFD detection, in the PHY by default. What it doesn't give is the latency from the MDI to the detector. The earlier 3200/6400 ns defaults were wrong and were set to 0, as explained under Latency constants above.
- **PI loop sign** (step 2, `bm_serial_adin_utc_cb`): correct as written, so no change is needed.
  - `err = TsSubtract(pps_utc, capt)` gives UTC − ADIN. The driver confirms TsSubtract returns A − B.
  - When the ADIN clock is behind, `err > 0`, `ppb > 0`, and the addend goes above 0x85555555, so the clock speeds up.
  - When the ADIN clock is ahead, the addend goes below nominal and the clock slows down.
  - The step path for errors over 1 ms uses the same sign.

## Known caveats and follow-ups
- There are no Announce messages and no best-master selection. The bridge is assumed to be the master.
- Step 2's PPS handler reads ADIN registers from the NCP task while the L2 thread may also be using the driver. This is a possible race.
- `adin2111_TsSetTimerAbsolute` stops the clock briefly during a step and rounds the nanoseconds down to a multiple of 16. This is minor, and the PI loop removes the leftover offset.
