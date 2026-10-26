# ADIN2111 Teardown / Power-Down — `bringup_mote`

This document covers how the `bringup_mote` app (`src/apps/bringup_mote`) brings
the ADIN2111 10BASE-T1L switch up and down, what the current `adin off` command
actually does, what it leaves behind, and the sequence needed to de-initialize
and power the part down cleanly.

> Scope: `bringup_mote` built for BSP `bm_mote_v1.0`
> (`tools/scripts/release/configs/default.yml`, `tools/scripts/test/configs/bringup.yaml`),
> default (`LEGACY=0` → `BRINGUP_LEGACY=0`) build unless noted. Paths are
> relative to the repo root. Anything under `src/lib/bm_core/` lives in the
> `bm_core` submodule. All findings come from reading the code. None of this
> was exercised on hardware.

---

## 1. Summary

- `bringup_mote` has no ADIN logic of its own. `app_main.cpp` only calls
  `debugAdinRawInit()` (`src/apps/bringup_mote/app_main.cpp:356`). That
  registers the `adin` CLI command from `src/lib/debug/debug_adin_raw.cpp`.
  The only teardown path is **`adin off`**, which exists only in the
  `BRINGUP_LEGACY == 0` build.
- `adin off` (`src/lib/debug/debug_adin_raw.cpp:124-126`) does two things:
  1. `bcl_power_callback(false)`, which sets `ADIN_PWR` low and nothing else.
  2. Zeroes the file-local `network_device` copy. This has no effect on the
     driver, L2, or the IP stack.
- So `adin off` is a **hard power cut, not a teardown**. The L2 task and
  queue, the renegotiation timers, the stress-test task and timers, the lwIP
  netif, the ADIN IRQ callback, and the ADI driver state all stay alive.
  `ADIN_CS` and `ADIN_RST` stay driven high into the unpowered part.
- A clean teardown has to:
  1. Quiesce traffic.
  2. Put the PHYs into software power-down.
  3. Stop L2.
  4. Unregister the IRQ.
  5. Un-init the ADI driver.
  6. Park the MCU-side pins.
  7. Only then drop `ADIN_PWR`.

  §5 gives the sequence. §6 gives a lower-risk alternative (hold `ADIN_RST`
  low).

---

## 2. Hardware context (`src/bsp/bm_mote_v1.0`)

| Signal      | MCU pin | Config at boot (`Core/Src/gpio.c`, `spi.c`)    | Notes |
|-------------|---------|------------------------------------------------|-------|
| `ADIN_PWR`  | PH1     | GPIO out, reset low                            | `bspInit()` drives it **high** (`bsp.cpp:69`). The schematic decides which ADIN rails it switches, including whether VDDIO is switched. Firmware does not show this. |
| `ADIN_RST`  | PA0     | GPIO out, reset low                            | Active-low HW reset. Datasheet: hold low > 10 µs. Held low = **hardware power-down** (see §6). |
| `ADIN_CS`   | PA15    | GPIO out, reset low                            | Software CS. `HAL_SpiReadWrite()` toggles it (`src/lib/drivers/adin2111/src/adi_hal.c:68,87`). |
| `ADIN_SCK`  | PB3     | SPI3 AF (CPOL low)                             | |
| `ADIN_MISO` | PB4     | SPI3 AF                                        | |
| `ADIN_MOSI` | PB5     | SPI3 AF                                        | |
| `ADIN_INT`  | PB8     | Input, no pull, **EXTI8 falling edge**, NVIC prio 6 | Datasheet: open-drain, needs a 1.5 kΩ pull-up **to VDDIO**. |

`adin_pins = {&spi3, &ADIN_CS, &ADIN_INT, &ADIN_RST}` (`bsp.cpp:56`).

Datasheet absolute maximum ratings (Table 5, `adin2111.pdf`): SPI, INT and
RESET pins are rated **−0.3 V to VDDIO + 0.3 V**. Driving them high while
VDDIO is unpowered exceeds that rating and can back-power the part through
its ESD structures.

---

## 3. What `adin init` sets up

`adin init` (`debug_adin_raw.cpp:92-119`) creates everything a teardown has
to undo:

| # | Call | Resource created |
|---|------|------------------|
| 1 | `IORegisterCallback(adin_pins.interrupt, network_device_interrupt, NULL)` | EXTI8 → `bm_l2_handle_device_interrupt()` → L2 queue. |
| 2 | `HAL_Init_Hook()` | No-op (`adi_hal.c:52`). |
| 3 | `network_device = adin2111_network_device()` + `callbacks->power = bcl_power_callback` | Static `NETWORK_DEVICE`/callbacks in `bm_adin2111.c`. |
| 4 | `adin2111_init()` (`bm_adin2111.c:758`) | First call only: allocates `RX_QUEUE_NUM_ENTRIES` aligned RX buffers. These are never freed by design. Then calls `adin2111_netdevice_enable()`, which runs `power(true)` (`ADIN_PWR`=1, `CS`=1, `RST` pulsed low 1 ms, then 100 ms wait), `adin2111_Init()`, link-change callback, RX buffer submission, `SyncConfig`, and enables ports 1 and 2. |
| 5 | `bm_l2_deinit()` then `bm_l2_init(network_device)` (`l2.c:536,557`) | L2 event queue, `"L2"` task, and per-port renegotiation timers (started from the L2 thread). **`bm_l2_init()` calls `trait->enable()` again**, so the chip is power-cycled/reset and `adin2111_Init()` runs a second time. |
| 6 | `bm_ip_init()` (first time only, `bm_lwip.c:287`) | `tcpip` thread, lwIP netif (up, link up), MLD join to `ff03::1`. |
| 7 | `stress_test_deinit()` then `stress_test_init(...)` (`src/lib/common/stress.c`) | Stress queue, `"stress"` task, UDP PCB, TX and stats timers, `stress` CLI command. |

In the legacy build (`BRINGUP_LEGACY == 1`) steps 6 and 7 are replaced by
overriding `callbacks->receive = debug_l2_rx`, and there is **no `off`
command**.

---

## 4. What `adin off` does today, and what it leaves behind

```c
// src/lib/debug/debug_adin_raw.cpp:124
} else if (strncmp("off", parameter, parameterStringLength) == 0) {
  bcl_power_callback(false);          // IOWrite(&ADIN_PWR, 0) — nothing else
  network_device = (NetworkDevice){}; // clears a local copy only
```

`bcl_power_callback(false)` (`src/lib/bm_integration/bristlemouth_client.cpp:43`)
only writes `ADIN_PWR = 0`. The `if (on)` branch is the only place that touches
`ADIN_CS`/`ADIN_RST`.

What is still live after `adin off`:

1. **PHYs are not put into software power-down first.**
   `trait->disable()` (`adin2111_netdevice_disable`, `bm_adin2111.c:372`)
   would do that, but `adin off` bypasses it. Link partners see the link
   disappear abruptly instead of an orderly power-down.
2. **L2 still runs.** The `"L2"` task, event queue and `CTX` are untouched.
   `CTX.enabled_ports_mask` still says the ports are up, because no
   link-down event can arrive from a dead chip. Any outbound frame is still
   queued and handed to `adin2111_netdevice_send()`, which calls
   `adin2111_SubmitTxBuffer()` and does SPI traffic to an unpowered device.
3. **Renegotiation timers keep polling.** Any port whose link was down at
   power-off still has a 100 ms auto-reload timer (`l2.c:114-140`). It calls
   `retry_negotiation()`, which reads PHY AN status over SPI. On a dead chip
   MISO is undefined, so the result can trigger `adin2111_Renegotiate()`
   writes. SPI never reports an error here because the master doesn't know the
   slave is gone.
4. **The IRQ path stays armed.** EXTI8 is still enabled and the pin callback
   is still `network_device_interrupt`. If `ADIN_PWR` also removes VDDIO
   (where the INT pull-up goes), INT falls and produces a falling-edge
   interrupt. That queues `L2Irq`, and the L2 thread runs the ADI MAC IRQ
   handler, which reads status registers over SPI from the dead chip.
5. **The lwIP netif is still up** (`netif_set_up`/`netif_set_link_up` from
   `bm_ip_init()`). MLD, ND, and stress traffic keep flowing down into L2
   (see item 2).
6. **The stress test is still running.** If `stress start` was issued, the TX
   timer keeps generating UDP traffic. The stats timer calls `port_stats()`
   on any port L2 still thinks is up, which is more SPI to a dead chip.
7. **TX buffers can leak.** Frames sitting in the ADI MAC TX queue never
   complete, so `tx_complete()`/`free_tx_buffer()` never runs. The next
   `adin2111_Init()` → `MAC_Init()` runs `memset`s on `DEVICE_MEMORY`
   (`adi_mac.c:493`), which drops those descriptors and leaks their
   `bm_malloc`/`aligned_malloc` allocations.
8. **Pins are left driven into the unpowered part.** After `adin off`,
   `ADIN_CS` and `ADIN_RST` are still outputs driven **high** (from the last
   `power(true)` and `HAL_SpiReadWrite`). MOSI holds its last bit. SPI3 stays
   enabled with SCK/MISO/MOSI in AF mode. If VDDIO is off, this violates the
   abs-max rating in §2.
9. **ADI driver state is stale.** `adin2111_UnInit()` is never called, so the
   MAC/PHY handles still report initialized.

### Re-running `adin init` after `adin off`

This is the only "recovery" path in the app today. It works mostly by
brute force, with these caveats:

- `bm_l2_deinit()` deletes the event queue **before** the L2 task
  (`l2.c:537-542`). The L2 task is normally blocked in `bm_queue_receive()`
  on that queue. FreeRTOS says not to delete a queue that has tasks blocked
  on it. Deleting the task first is the safe order.
- `bm_l2_deinit()` `memset`s `CTX`, which drops `link_change_callback_list`
  without freeing its items, so those items leak.
- `stress_test_deinit()` calls `udp_remove()` from the CLI task without the
  lwIP core lock. With `LWIP_TCPIP_CORE_LOCKING=1` and
  `LWIP_FREERTOS_CHECK_CORE_LOCKING=1` (`src/lib/lwip/lwipopts.h:20-21`),
  `udp_remove()` → `LWIP_ASSERT_CORE_LOCKED()` → `configASSERT(0)`. So a
  **second** `adin init` on the non-legacy build is expected to assert, once
  the stress task has created its PCB.
- `stress_test_init()` re-registers the `stress` CLI command every time.
- Each `adin init` power-cycles the chip twice (once in `adin2111_init()`,
  once in `bm_l2_init()`).

---

## 5. Clean teardown sequence (recommended)

Order matters. Stop producers first, stop the chip second, cut power last.
None of these steps exist as a single API today. They would go in a new
`adin off` implementation in `debug_adin_raw.cpp`. Steps that touch L2
internals need either a bm_core change or must run while the L2 task is not
running.

### Step 1 — Stop application traffic

- `stress_stop_tx()` if the stress test is running. This stops the TX and
  stats timers.
- `stress_test_deinit()` **with the lwIP core locked** (`LOCK_TCPIP_CORE()` /
  `UNLOCK_TCPIP_CORE()` around it, or at least around `udp_remove()`).

### Step 2 — Take the netif down

- Call `bm_l2_set_netif(false)` (`bm_lwip.c:429`), which runs
  `netif_set_link_down` + `netif_set_down`, **under `LOCK_TCPIP_CORE()`**.
  `bm_l2_set_netif()` does not take the lock itself, so calling it bare
  (including via `bm_l2_netif_set_power(false)`) trips the core-locking
  assert.
- This stops lwIP from pushing new frames into `bm_l2_link_output()`.

### Step 3 — Put both PHYs into software power-down

- `trait->disable_port(self, 2)` then `trait->disable_port(self, 1)`.
  Each calls `adin2111_DisablePort()` → `PHY_EnterSoftwarePowerdown()`.
  Port 1 power-down also takes down port 2 (datasheet p.25, comment at
  `bm_adin2111.c:249`), so do port 2 first.
- Alternatively call `trait->disable(self)` (`adin2111_netdevice_disable`).
  It disables ports 1 and 2 and then calls `power(false)`. Don't use it as
  the final step, though, because it cuts power before the pins are parked
  (Step 7).
- Optionally call `bm_l2_netif_enable_disable_port(port, false)` for each
  port instead. It calls `disable_port` **and** the L2 `link_change(…, false)`,
  which clears `enabled_ports_mask` and notifies link-change subscribers.
  Note that it also **starts** a renegotiation timer for that port, which
  Step 4 must then stop.

### Step 4 — Stop L2

- Stop/delete the renegotiation timers, delete the `"L2"` task, then delete
  the event queue. `bm_l2_deinit()` does all three, but in queue → task →
  timers order. Prefer fixing it to task → timers → queue.
- Any `L2Tx` events still in the queue hold `bm_l2_new()` buffers. Drain the
  queue and `bm_l2_free()` each `buf` before deleting it, or accept the leak.
- After `bm_l2_deinit()`, `CTX.evt_queue == NULL`. `bm_l2_handle_device_interrupt()`
  and `bm_l2_tx()` both handle that safely (`bm_queue_send(NULL, …)` returns
  `BmENOMEM`, `bm_freertos.c:29`), so a late IRQ or TX won't fault.

### Step 5 — Disarm the ADIN interrupt

- `IORegisterCallback(adin_pins.interrupt, NULL, NULL)`. `STM32IORegisterCallback`
  just stores the pointer (`src/lib/drivers/stm32_io.c:50-57`), and
  `STM32IOHandleInterrupt` ignores a NULL callback.
- `NVIC_DisableIRQ(ADIN_INT_EXTI_IRQn)`. `adin2111_UnInit()` does this too
  (Step 6), via `HAL_DisableIrq()`.
- After power is removed (Step 8), clear any pending EXTI8 falling flag
  (`LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_8)` and
  `NVIC_ClearPendingIRQ(EXTI8_IRQn)`), so the INT drop doesn't fire on
  re-enable.

### Step 6 — Un-initialize the ADI driver

- `adin2111_UnInit(&DEVICE_STRUCT)` (`adin2111.c:234`). It runs `PHY_UnInit`
  ×2 and `MAC_UnInit`, which disable the IRQ and mark the state as
  uninitialized, then calls `HAL_UnInit_Hook()` (a no-op) and NULLs the
  handles. `DEVICE_STRUCT` is `static` in `bm_adin2111.c`, so this needs a
  new wrapper there. Today `adin2111_netdevice_disable()` does **not** call
  it.
- Before or after UnInit, walk the MAC TX queue and free each pending
  `pBufDesc` with `free_tx_buffer()` so the next `MAC_Init()` `memset`
  doesn't leak them (§4 item 7). This also needs a bm_core change.
- RX buffers (`RX_BUFFERS[]`) are allocated once and reused across inits.
  **Do not free them.**

### Step 7 — Park the MCU pins

Before cutting power, so nothing is driven into an unpowered VDDIO:

- `ADIN_CS` → drive **low**, or switch to analog/high-Z.
- `ADIN_RST` → drive **low**. This also puts the part into hardware
  power-down (§6) for the moment before power drops.
- SPI3: `HAL_SPI_DeInit(&hspi3)`. Its `HAL_SPI_MspDeInit` disables the SPI3
  clock, `HAL_GPIO_DeInit`s PB3/PB4/PB5 to analog, de-inits the DMA channels
  and disables `SPI3_IRQn` (`Core/Src/spi.c:357-379`). `spiInit(&spi3)` /
  `MX_SPI3_Init()` must be called again on bring-up. At minimum, make sure
  MOSI isn't left high.
- `ADIN_INT` is already an input with no pull, so leave it alone.

### Step 8 — Cut power

- `IOWrite(&ADIN_PWR, 0)`, which is what `bcl_power_callback(false)` does.
- Clear the pending EXTI8 flag (Step 5).
- Zero any local `NetworkDevice` copies, as `adin off` already does.

### Bringing it back up

- Restore pins: `spiInit(&spi3)` (or `MX_SPI3_Init()`), and `ADIN_CS` high.
  `bcl_power_callback(true)` already sets CS high and pulses RST.
- Re-register the IRQ callback, then run the existing `adin init` path. Fix
  the `stress_test_deinit()` locking first (§4), or skip re-initializing the
  stress test if it was already torn down in Step 1.
- `bm_ip_init()` must **not** be called again (`static stack_initialized`
  already guards it). Bring the netif back with `bm_l2_set_netif(true)`
  under the core lock.

---

## 6. Lower-risk alternative: hardware power-down via `ADIN_RST`

Per the datasheet ("Power-Down Modes → Hardware Power-Down Mode"), holding
`RESET` low disables all analog and digital circuits, gates the clocks, and
**tristates all I/O pins**. Consumption drops to leakage. Registers are not
accessible.

Doing Steps 1–6 as above, then driving `ADIN_RST` low and **leaving
`ADIN_PWR` on**, gives:

- No back-powering or abs-max concerns: VDDIO stays up, so CS/SCK/MOSI can
  stay as they are. Parking CS low or high-Z is still tidy.
- No spurious INT edge, because the INT pull-up rail stays powered and the
  tristated INT stays high.
- Wake-up is the existing `power(true)` path: RST high → 100 ms →
  `adin2111_Init()`.

The trade-off is leakage current versus a full rail cut. Whether that
matters depends on the `ADIN_PWR` load switch and the board's power budget.
Measure it on hardware.

---

## 7. Gaps that need code changes

| Gap | Where | Fix |
|-----|-------|-----|
| `adin off` is a raw power cut | `src/lib/debug/debug_adin_raw.cpp:124` | Implement §5. |
| No legacy-build `off` | `debug_adin_raw.cpp:61-65,123-136` | Add one if needed. |
| `bcl_power_callback(false)` doesn't park CS/RST | `src/lib/bm_integration/bristlemouth_client.cpp:43` | Drive `ADIN_RST`/`ADIN_CS` low before `ADIN_PWR`=0. |
| `adin2111_netdevice_disable()` doesn't `adin2111_UnInit()` or drain TX queue | `src/lib/bm_core/drivers/adin2111/bm_adin2111.c:372` | bm_core change (upstream PR). |
| `bm_l2_deinit()` deletes queue before task; leaks link-change list and queued buffers | `src/lib/bm_core/network/l2.c:536` | bm_core change. |
| `bm_l2_set_netif()` / `bm_l2_netif_set_power()` call lwIP without the core lock | `src/lib/bm_core/network/bm_lwip.c:429`, `l2.c:669` | Lock in caller or in bm_core. |
| `stress_test_deinit()` calls `udp_remove()` without the core lock | `src/lib/common/stress.c:163` | Wrap in `LOCK_TCPIP_CORE()`/`UNLOCK_TCPIP_CORE()`. |
| `adin init` power-cycles/inits the chip twice | `debug_adin_raw.cpp:98-103` + `l2.c:571` | Optional cleanup. |
