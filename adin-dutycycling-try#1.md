# ADIN2111 Duty Cycling — Plan (try #1)

## Context

`bm_soft_module` keeps the ADIN2111 powered all the time. For power testing, the app should
switch the ADIN2111 on and off on a fixed schedule: **on for 20 s of every 60 s** to start with,
set by `#define`s in `src/apps/bm_soft_module/app_main.cpp`.

Constraints from the user:
- Teardown follows **§5 "Clean teardown sequence"** of `mote_bu_adin_teardown.md`.
- Bring-up reuses the **existing high-level init calls** described in `adin2111-full-init.md`. No
  register-level code is reimplemented.

### What the init doc tells us (high level)

At boot, `bcl_init()` → `bristlemouth_init()` runs `adin2111_init()` (allocates RX buffers once,
then `adin2111_netdevice_enable()`), then `bm_l2_init(network_device)`. `bm_l2_init()` creates the
L2 queue and task and calls `trait->enable()`. That is **the same `adin2111_netdevice_enable()`**,
which does the whole chip bring-up:

`power(true)` (CS high, RST pulse, 100 ms wait) → `adin2111_Init()` (MAC reset, switch config,
PHY init) → register link-change callback → submit 8 RX buffers → `adin2111_SyncConfig()` (also
enables EXTI8 at the NVIC) → enable port 1, then port 2.

So **`bm_l2_init(bristlemouth_network_device())` is the single high-level call that re-initializes
the ADIN2111 and L2**, exactly as boot pass #2 does. Re-init must **not** call:
- `adin2111_init()`: it would run enable a second time and re-register metrics components.
- `bristlemouth_init()`, `bm_ip_init()` or `bcmp_init()`: they are one-time stack setup.

### Problems found that block a safe §5 teardown today

| # | Problem | Where | Consequence |
|---|---|---|---|
| 1 | `bm_l2_deinit()` deletes the **queue before the task**. The L2 task is blocked on that queue | `l2.c` `bm_l2_deinit` | FreeRTOS undefined behaviour or assert |
| 2 | `bm_l2_deinit()` `memset`s all of `CTX`, which wipes `link_change_callback_list` (BCMP, port monitor), `routing_cb` (middleware) and `pcap_cb` | `l2.c` | After re-init, BCMP never hears about link changes and middleware routing is gone |
| 3 | `bm_l2_deinit()` doesn't drain queued `L2Tx`/`L2Rx` buffers | `l2.c` | Memory leak on every cycle |
| 4 | `DEVICE_STRUCT` is `static`, so the app can't call `adin2111_UnInit()`. Pending MAC TX descriptors are never freed | `bm_adin2111.c` | Can't do §5 Step 6. TX buffers leak on every cycle |
| 5 | After `adin2111_UnInit()`, `pPhyDevice[]` and `pMacDevice` are `NULL`. `port_stats` (metrics and BCMP CLI), `retry_negotiation`, `send`, `enable_port` and `disable_port` would dereference them | `bm_adin2111.c` | Hard fault if metrics are requested during the off window |
| 6 | `network_device_interrupt()` (the ADIN_INT ISR hook) is `static` in `bristlemouth_client.cpp` | `bm_integration` | The app can't re-register the IRQ callback (§5 Step 5 and bring-up) |
| 7 | §5 says to restore SPI with `spiInit(&spi3)`. That is **wrong for a second call**: it creates a new mutex and asserts on `MAX_NUM_SPI` | `protected_spi.c:23` | Re-init must call `MX_SPI3_Init()` instead |
| 8 | `bm_l2_netif_set_power()` / `bm_l2_set_netif()` don't take the lwIP core lock. `LWIP_FREERTOS_CHECK_CORE_LOCKING=1` | `bm_lwip.c:429`, `lwipopts.h:21` | The caller must wrap the call in `LOCK_TCPIP_CORE()`/`UNLOCK_TCPIP_CORE()` |

Items 1–5 need changes in the **`src/lib/bm_core` submodule** (currently at `f06b3b4`). Item 6 is in
this repo.

---

## Changes

### A. `src/lib/bm_core/network/l2.c` / `l2.h`: add `bm_l2_stop()` and fix ordering (fixes #1–#3)

Add `void bm_l2_stop(void);` to `l2.h`. It stops L2 but **keeps registered callbacks**, so
`bm_l2_init()` can be called again afterwards:

1. `if (CTX.task_handle) bm_task_delete(CTX.task_handle); CTX.task_handle = NULL;` (**task first**)
2. For each port: `bm_l2_stop_renegotiate_check(port)` (existing static helper; **timers second**)
3. Grab `BmQueue q = CTX.evt_queue; CTX.evt_queue = NULL;` so late `bm_l2_tx()` or IRQ calls fail
   safely with `BmENOMEM`. Then drain it: `while (bm_queue_receive(q, &evt, 0) == BmOK)` and
   `bm_l2_free(evt.buf)` for `L2Tx`/`L2Rx`. Finally `bm_queue_delete(q)` (**queue last**).
4. Save `link_change_callback_list`, `routing_cb` and `pcap_cb`, `memset(&CTX, 0, …)`, then restore
   those three fields.

Rewrite `bm_l2_deinit()` as `bm_l2_stop()` plus clearing the three preserved fields. It keeps its
current "reset everything" behaviour, which `test/src/l2_test.cpp` TearDown and
`src/lib/debug/debug_adin_raw.cpp` rely on. Update the `bm_l2_init()` doc comment to say
"`bm_l2_deinit()` **or `bm_l2_stop()`** must be called before calling this again".

Unit test (`src/lib/bm_core/test/src/l2_test.cpp`): register a link-change callback, then
`bm_l2_stop()`, then `bm_l2_init()`. Trigger a link change and check that the callback still fires.
Also check that a queued TX buffer is freed by `bm_l2_stop()`.

### B. `src/lib/bm_core/drivers/adin2111/bm_adin2111.c` / `.h`: add `adin2111_deinit()` and NULL guards (fixes #4, #5)

Add `BmErr adin2111_deinit(void);` to `bm_adin2111.h`. Implementation in `bm_adin2111.c`:
1. If `DEVICE_STRUCT.pMacDevice == NULL`, return `BmOK` (already de-initialized).
2. Free pending TX: walk `DEVICE_STRUCT.pMacDevice->txQueue` with the exported `queueIsEmpty()` and
   `queueRemove()` (declared in `adi_mac.h:732-734`). For each entry,
   `free_tx_buffer(q->pEntries[q->tail].pBufDesc)` (existing static helper in this file), NULL the
   slot, then `queueRemove(q)`. **Don't touch `RX_BUFFERS[]`.** They're allocated once and reused.
3. Call `adin2111_UnInit(&DEVICE_STRUCT)` (`adin2111.c:234`). It runs PHY UnInit ×2 and MAC UnInit,
   disables the IRQ and NULLs the handles.
4. Set `LINK_CHANGE.device_handle = NULL`.

Guards: at the top of `adin2111_netdevice_send`, `_enable_port`, `_disable_port`, `_renegotiate` and
`adin2111_port_stats`, add `if (DEVICE_STRUCT.pMacDevice == NULL) return BmENODEV;`.
`adin2111_netdevice_enable()` is unaffected, because `adin2111_Init()` repopulates the handles.

> Submodule workflow: commit A and B on a branch in the `bm_core` repo, then bump the
> `src/lib/bm_core` pointer in this repo. Only that submodule is touched, with no recursive updates.

### C. `src/lib/bm_integration/bristlemouth_client.cpp` / `.h`: expose IRQ (re)registration (fixes #6)

Add `void bcl_set_network_interrupt(bool enable);` to `bristlemouth_client.h`:

```cpp
void bcl_set_network_interrupt(bool enable) {
  IORegisterCallback(adin_pins.interrupt, enable ? network_device_interrupt : NULL, NULL);
}
```

`bcl_init()` calls `bcl_set_network_interrupt(true)` in place of the direct `IORegisterCallback`.
Passing `NULL` is safe, because `STM32IOHandleInterrupt` ignores a NULL callback.

### D. `src/apps/bm_soft_module/task_priorities.h`

Add `#define ADIN_DUTY_CYCLE_TASK_PRIORITY 2`. That's the priority `defaultTask` had when `bcl_init()`
ran at boot, so bring-up runs under the same scheduling conditions as boot (below L2 at 7, equal to
the timer task).

### E. `src/apps/bm_soft_module/app_main.cpp`: the duty-cycle task

**Configuration (top of file):**

```cpp
// ADIN2111 duty cycling (power testing)
#define ADIN_DUTY_CYCLE_ENABLE (1)
#define ADIN_DUTY_CYCLE_PERIOD_S (60)
#define ADIN_DUTY_CYCLE_ON_S (20)
#define ADIN_DUTY_CYCLE_SETTLE_MS (50) // let in-flight TX/IRQ work drain before teardown
static_assert(ADIN_DUTY_CYCLE_ON_S > 0 && ADIN_DUTY_CYCLE_ON_S < ADIN_DUTY_CYCLE_PERIOD_S,
              "ADIN on-time must be within the period");
```

**Includes to add:** `"lwip/tcpip.h"` (`LOCK_TCPIP_CORE`), `"l2.h"`, `"spi.h"` (`MX_SPI3_Init`,
`hspi3`), `"bristlemouth_client.h"`. `bm_ip.h` and `bristlemouth.h` have no C++ guards, so wrap them in
`extern "C" { … }` the same way `bristlemouth_client.cpp` does. `stress.h` and `bsp.h` (`spi3`,
`adin_pins`, `ADIN_*`) are already included.

**Schedule:** the task starts after boot with the ADIN already on. `vTaskDelayUntil` keeps the
60 s period exact, whatever teardown and bring-up cost. Bring-up time (~0.1–0.3 s, plus link
negotiation) counts against the 20 s on window.

```cpp
static void adin_duty_cycle_task(void *parameters) {
  (void)parameters;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(ADIN_DUTY_CYCLE_ON_S * 1000));
    adin_teardown();
    vTaskDelayUntil(&wake, pdMS_TO_TICKS((ADIN_DUTY_CYCLE_PERIOD_S - ADIN_DUTY_CYCLE_ON_S) * 1000));
    adin_bringup();
  }
}
```

Create it at the end of `defaultTask`, after `user_code_start()` and inside
`#if ADIN_DUTY_CYCLE_ENABLE`. Use `xTaskCreate(adin_duty_cycle_task, "adin_duty", 1024, NULL,
ADIN_DUTY_CYCLE_TASK_PRIORITY, NULL)` and `configASSERT(rval == pdPASS)`. `defaultTask` ran the
same init path with 512 words, so 1024 leaves margin. Only create it if `bcl_init()` succeeded, by
checking `bristlemouth_network_device().trait != NULL` or adding a `bcl_init` return value.

**`adin_teardown()`**: §5 steps, with Step 5 moved earlier (see the note below):

| Order | §5 step | Code |
|---|---|---|
| 1 | Step 1: stop app traffic | `stress_stop_tx();` Safe if not running. **Do not** call `stress_test_deinit()`. Its UDP pcb stays bound and isn't re-initialized (§5 "Bringing it back up" allows skipping re-init) |
| 2 | Step 2: netif down | `LOCK_TCPIP_CORE(); bm_l2_set_netif(false); UNLOCK_TCPIP_CORE();` then `vTaskDelay(ADIN_DUTY_CYCLE_SETTLE_MS)` with the IRQ still armed, so queued TX frames finish and get freed by `tx_complete` |
| 3 | Step 3: PHY software power-down | `bm_l2_netif_enable_disable_port(2, false);` then `(1, false)`. **Port 2 first.** This also tells BCMP and the port monitor that the links went down, and clears `enabled_ports_mask` |
| 4 | Step 5 (part): disarm IRQ | `bcl_set_network_interrupt(false); NVIC_DisableIRQ(ADIN_INT_EXTI_IRQn);` then a short settle so L2 drains any queued `L2Irq` and is left blocked on its queue |
| 5 | Step 4: stop L2 | `bm_l2_stop();` (change A): deletes the task, then the timers (including those started by step 3), then drains and deletes the queue. Callbacks are kept |
| 6 | Step 6: un-init driver | `adin2111_deinit();` (change B) |
| 7 | Step 7: park pins | `IOWrite(&ADIN_CS, 0); IOWrite(&ADIN_RST, 0);` then `xSemaphoreTake(spi3.mutex, portMAX_DELAY); HAL_SPI_DeInit(&hspi3); xSemaphoreGive(spi3.mutex);`. Taking the mutex waits out any in-flight transfer, for example a renegotiation-timer callback that was already running. `ADIN_INT` is left alone |
| 8 | Step 8: cut power | `bcl_power_callback(false);` (`ADIN_PWR`=0), then `LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_8); NVIC_ClearPendingIRQ(ADIN_INT_EXTI_IRQn);` |

*Why Step 5 moves before Step 4:* the app task runs below L2's priority 7. So whenever it runs, L2
is blocked, and it could be blocked mid-SPI while holding the SPI3 mutex. Disarming the IRQ first
and letting L2 drain makes sure L2 is idle on `bm_queue_receive` when it's deleted, so it can't
take the mutex with it. Once `bm_l2_stop()` has nulled `CTX.evt_queue`, a late IRQ is harmless
anyway.

**`adin_bringup()`**: §5 "Bringing it back up", plus the init doc's high-level path:

1. `MX_SPI3_Init();`: restores SPI3 clock, pins and DMA links through `HAL_SPI_MspInit`. **Not**
   `spiInit(&spi3)` (problem #7). The existing `spi3` mutex and DMA registration are still valid.
2. Clear any stale EXTI8 flag again (as in teardown step 8), then `bcl_set_network_interrupt(true);`.
3. `BmErr err = bm_l2_init(bristlemouth_network_device());`. This is the full init from the init
   doc, §4.1–§4.13: power and reset pulse, MAC/PHY init, link-change callback, RX buffers,
   `SyncConfig` (re-enables EXTI8 at the NVIC) and ports 1 → 2. The `power` and
   `debug_packet_dump` callbacks set at boot are still in the static callbacks table.
4. If `err == BmOK`: `LOCK_TCPIP_CORE(); bm_l2_set_netif(true); UNLOCK_TCPIP_CORE();`. Never call
   `bm_ip_init()` again.
5. If it fails, `printf` the error and carry on. `trait->enable()` has already called
   `power(false)`. The next teardown is still safe, because every step tolerates a half-initialized
   device: `bm_l2_stop()` checks NULLs and `adin2111_deinit()` returns early.

Log each transition with `printf("ADIN duty: on/off (uptime %lu ms)\n", …)` so power traces can be
lined up with the console.

---

## Known behaviour and limitations

- **Network traffic is unavailable for 40 s of every minute.** `bm_pub` from `user_code` (TSYS
  readings), BCMP heartbeats and Spotter logging fail during the off window. That's expected for
  this test. Expect error prints from those callers.
- **Neighbours must be awake at the same time.** The schedule starts when the module boots, not
  on wall-clock minutes. If the other end (Spotter or another mote) isn't powered during this
  node's 20 s window, no link forms. If that matters, a follow-up can align to the RTC minute
  boundary (`rtcGet`) once UTC has been received.
- Link negotiation (10BASE-T1L AN plus leader/follower) and IPv6 ND/MLD take time after bring-up,
  so the usable window is somewhat less than 20 s.
- The renegotiation-timer race in init doc §8.2 (L2 timers start before `trait->enable()` finishes)
  happens on every bring-up, as it does at boot. It's an existing issue and is accepted here.
- `stress` CLI: if a stress run was started, it stays stopped after the first teardown.

## Files touched

| File | Repo | Change |
|---|---|---|
| `src/lib/bm_core/network/l2.c`, `l2.h` | bm_core submodule | `bm_l2_stop()`, `bm_l2_deinit()` reorder and drain |
| `src/lib/bm_core/test/src/l2_test.cpp` | bm_core submodule | Tests for `bm_l2_stop()` |
| `src/lib/bm_core/drivers/adin2111/bm_adin2111.c`, `.h` | bm_core submodule | `adin2111_deinit()`, NULL guards |
| `src/lib/bm_integration/bristlemouth_client.cpp`, `.h` | bm_protocol | `bcl_set_network_interrupt()` |
| `src/apps/bm_soft_module/task_priorities.h` | bm_protocol | `ADIN_DUTY_CYCLE_TASK_PRIORITY` |
| `src/apps/bm_soft_module/app_main.cpp` | bm_protocol | Defines, task, teardown and bring-up |
| `src` (submodule pointer) | bm_protocol | Bump `src/lib/bm_core` |

## Verification

1. **Unit tests (bm_core):** build and run the bm_core gtest suite (`src/lib/bm_core/test`). The
   new `bm_l2_stop` tests pass and the existing `l2_test.cpp` tests still pass.
2. **Build:** configure and build `APP=bm_soft_module BSP=bm_mote_spi_v1_0` with the existing
   CMake preset or `flash-em.sh` flow, with no warnings in the touched files.
3. **Bench (single node and USB console):**
   - Console shows `ADIN duty: off` about 20 s after boot, then `on` 40 s later, repeating every 60 s.
   - Watch `adin_pwr` with the debug GPIO CLI or a scope. It should be high for 20 s and low for
     40 s. Measure current on the supply to confirm the power saving (the point of the exercise).
   - Run for at least 1 hour (60 cycles) and check heap with `memory_metrics` / the `info` CLI.
     Free heap stays flat, which shows nothing leaks on teardown.
4. **Bench (linked to a Spotter or second mote kept always-on):** during each on window, the console
   shows `Network Device Port N: up`, `bcmp` neighbour discovery succeeds, and TSYS readings
   publish. In the off window, `Port N: down` appears once per port and there are no hard faults or
   `Renegotiation failed` floods.
5. **Fault injection:** send a metrics or `bcmp` port-stats request during the off window. It
   returns zeros or an error and doesn't crash (guards from change B).
