# ADIN2111 De-initialization & Power-Down — `bm_soft_module`

This document covers how the ADIN2111 10BASE-T1L switch is brought up in the
`bm_soft_module` app, everything in the firmware that touches it (directly or
indirectly), the gaps in the de-init code that exists today, and the steps
needed to de-initialize and power it down at runtime (and bring it back) without
breaking the app.

> Scope: `bm_soft_module` built for BSP `bm_mote_spi_v1_0` (see
> `tools/scripts/release/configs/default.yml` and
> `tools/scripts/test/configs/mote.yaml`). bm_core is at `f06b3b4`. All paths
> are relative to the repo root. Anything under `src/lib/bm_core/` is in the
> `bristlemouth/bm_core` submodule, so changes there need an upstream PR.

---

## 1. Summary

- No production code path powers the ADIN2111 down on this app today. The
  building blocks exist (`bm_l2_netif_set_power()`,
  `adin2111_netdevice_disable()`, `bcl_power_callback(false)`), but they are
  **not safe to call as-is**. By code inspection:
  1. `bm_l2_set_netif()` calls lwIP netif APIs without the TCP/IP core lock.
     With `LWIP_FREERTOS_CHECK_CORE_LOCKING=1` that hits a `configASSERT`.
  2. L2 never learns the ports went down (`enabled_ports_mask` is unchanged),
     so it keeps pushing frames at the dead chip.
  3. The L2 "renegotiate" timers and the ADIN IRQ path can still run SPI
     transactions against an unpowered chip.
  4. TX buffers left in the ADI MAC queue at power-off are **leaked** when the
     chip is re-initialized (`MAC_Init()` `memset`s the device memory).
  5. `bcl_power_callback(false)` cuts `ADIN_PWR` but leaves `ADIN_CS`,
     `ADIN_RST` and the SPI3 pins driven high into an unpowered device. That
     violates the datasheet's absolute max ratings (SPI/INT/RESET ≤ VDDIO + 0.3 V)
     and back-powers the part.
  6. The powered-off INT line (open-drain with a pull-up to the ADIN VDDIO)
     falls, which produces a falling-edge EXTI8 interrupt.
- Recommended approach: add a serialized power-down/power-up API to bm_core
  L2 (executed in the L2 thread), fix `adin2111_netdevice_disable()` to fully
  uninit and drain the driver, park the MCU pins before cutting power, and
  re-power through `trait->enable()` (never `bcl_init()`/`bristlemouth_init()`
  again).
- Lower-risk alternative: put the chip into **hardware power-down** by holding
  `ADIN_RST` low (datasheet: all I/Os tri-stated, leakage-only current) instead
  of switching `ADIN_PWR`. See §8.

---

## 2. Hardware context (`src/bsp/bm_mote_spi_v1_0`)

| Signal      | MCU pin | Config (CubeMX)                          | Notes |
|-------------|---------|------------------------------------------|-------|
| `ADIN_PWR`  | PH1     | GPIO out, push-pull, reset-low           | Enables ADIN supply. `bspInit()` sets it high. Which rails it switches (VDDIO included?) is **not visible in firmware**. Confirm against the schematic. |
| `ADIN_RST`  | PA0     | GPIO out, reset-low                      | Active-low HW reset. Datasheet: hold low > 10 µs. Held low = hardware power-down. |
| `ADIN_CS`   | PA15    | GPIO out, reset-low                      | Software-controlled CS (`adi_hal.c` toggles it around transfers). |
| `ADIN_SCK`  | PB3     | SPI3 AF6 (JTDO/TRACESWO)                 | |
| `ADIN_MISO` | PB4     | SPI3 AF6 (NJTRST)                        | |
| `ADIN_MOSI` | PB5     | SPI3 AF6                                 | |
| `ADIN_INT`  | PB8     | Input, no pull, EXTI8 **falling edge**, NVIC prio 6 | Open-drain on the ADIN with a 1.5 kΩ pull-up to **ADIN VDDIO** (datasheet). |

- `adin_pins = {&spi3, &ADIN_CS, &ADIN_INT, &ADIN_RST}` (`src/bsp/bm_mote_spi_v1_0/bsp.cpp:58`).
- SPI3 is wrapped by `PROTECTED_SPI("SPI3", hspi3, MX_SPI3_Init, LPM_SPI3)`
  (`bsp.cpp:51`). The wrapper takes a mutex and sets `LPM_SPI3` active only
  for the duration of each transfer.
- `adi_config.h` (`src/lib/drivers/adin2111/include/`) enables
  `CONFIG_SPI_OA_EN` (OPEN Alliance SPI) and `CONFIG_SPI_PROT_EN`.
- These pins are also exposed through the debug GPIO CLI (`adin_cs`,
  `adin_int`, `adin_pwr`, `adin_rst`, `src/apps/bm_soft_module/app_main.cpp:280`).

Relevant datasheet facts (ADIN2111 Rev A, `adin2111.pdf` in repo root):
- Abs max: SPI, INT, RESET pins −0.3 V to **VDDIO + 0.3 V**. If VDDIO is
  switched off, the MCU must not drive these pins high.
- No supply sequencing required. Supply ramp is ≤ 40 ms, internal power-good
  takes 20–43 ms after the last rail, and SPI is active ≤ 50 ms after that.
- Reset-to-standby: 50 ms (MAC + PHY1) and **90 ms (PHY2)** after RESET is
  released.
- Hardware power-down: RESET held low → all analog and digital circuits off,
  clocks gated, **all I/O tri-stated**, consumption equals leakage.
- Software power-down (per PHY): 22–44 mW. This is what
  `adin2111_DisablePort()` does. It is not a real power-off.

---

## 3. Current initialization sequence

All of this runs in `defaultTask` (`src/apps/bm_soft_module/app_main.cpp`):

1. **`mxInit()` → `MX_GPIO_Init()`** (`bsp.cpp`, `Core/Src/gpio.c`)
   - `ADIN_PWR`, `ADIN_RST` and `ADIN_CS` are configured as outputs, all low.
     The chip is unpowered and held in reset.
   - `ADIN_INT` is set up as EXTI8, falling edge, and the NVIC `EXTI8_IRQn`
     is enabled.
2. **`bspInit()`** (`app_main.cpp:369` → `bsp.cpp`)
   - `spiInit(&spi3)` runs `MX_SPI3_Init()` (pins to AF, DMA ch12/13), creates
     the SPI mutex and registers a DMA slot. Only `MAX_NUM_SPI = 3` slots
     exist, so **`spiInit()` cannot be called again**
     (`protected_spi.c:33` asserts).
   - `IOWrite(&ADIN_PWR, 1)` powers the chip, still with RST low.
3. **`bcl_init()`** (`app_main.cpp:393` → `src/lib/bm_integration/bristlemouth_client.cpp:54`)
   1. `IORegisterCallback(ADIN_INT, network_device_interrupt)`. From then on,
      every falling edge on INT calls `bm_l2_handle_device_interrupt()` from
      the ISR, which queues an `L2Irq` event.
   2. `HAL_Init_Hook()` (no-op), `config_init()`, `port_monitor_init()`,
      `device_init()`.
   3. **`bristlemouth_init(bcl_power_callback)`** (`src/lib/bm_core/middleware/bristlemouth.c`):
      - `adin2111_network_device()` builds the static `NetworkDevice` and sets
        `callbacks->power = bcl_power_callback`.
      - **`adin2111_init()`** (`drivers/adin2111/bm_adin2111.c:758`):
        - Allocates the `RX_QUEUE_NUM_ENTRIES` (8) RX buffers once (guarded by
          a `static bool initialized`).
        - `adin2111_netdevice_enable()` (`bm_adin2111.c:294`):
          `power(true)`, then `adin2111_Init()`, which runs `MAC_Init()`
          (`memset`s device memory, resets the MAC, registers the
          SPI/IRQ callbacks) and initializes both PHYs.
          `RegisterCallback(link_change_callback_)`, submit all RX buffers,
          `SyncConfig`, then `EnablePort(1)` and `EnablePort(2)` (out of SW
          power-down).
        - If `bm_metrics_enabled` (default 1): `adin2111_network_metrics_init()`
          registers the `adin_port_stats_1/2` metrics components.
          **This is not idempotent.**
      - **`bm_l2_init()`** (`network/l2.c:557`): installs the `receive` and
        `link_change` callbacks, creates the L2 queue and task, then calls
        **`trait->enable()` a second time**. The ADIN gets a full second
        power/reset/init cycle at boot. This means re-running
        `trait->enable()` on a live system is already an exercised path.
      - `bm_ip_init()` (lwIP netif, link-local and unicast addresses set
        `VALID`, MLD join), `bcmp_init()` (heartbeat timer, neighbors, DFU,
        config, ping, time, resource discovery, and a link-change callback),
        `topology_init()`, `bm_service_init()`, `bm_pubsub_init()`,
        `bm_middleware_init()`, `metrics_service_init()`.
   4. Sets `debug_packet_dump = pcapTxPacket`, `bcmp_cli_init()`, and
      `stress_test_init()` (`STRESS_TEST_ENABLE` is defined in this app's
      `CMakeLists.txt`).
4. When the L2 thread starts, it starts a 100 ms auto-reload **renegotiate
   timer** for each port. These poll `retry_negotiation()` over SPI until the
   port's link comes up (`l2.c:420`).

`bcl_power_callback()` (`bristlemouth_client.cpp:43`):
```c
IOWrite(&ADIN_PWR, on);
if (on) { CS=1; RST=0; delay 1 ms; RST=1; delay 100 ms; }
// off: only ADIN_PWR=0. CS, RST and the SPI pins are left as they were (high).
```

---

## 4. What uses the ADIN2111 at runtime

### 4.1 Direct (code that ends up doing SPI to the chip)

| Path | Trigger | Context | File |
|------|---------|---------|------|
| `trait->send` → `adin2111_SubmitTxBuffer` → `MAC_ProcessTxQueue` | Every L2 TX event with a non-zero port mask | L2 thread | `l2.c:295-359`, `bm_adin2111.c:512` |
| `trait->handle_interrupt` → `macCallback`/`oaIrqHandler` → RX callbacks, link-change | ADIN INT falling edge → `L2Irq` | ISR → L2 thread | `bristlemouth_client.cpp:35`, `l2.c:444`, `bm_adin2111.c:707` |
| `receive_callback` → `adin2111_SubmitRxBuffer` (re-arm) | Inside IRQ handling | L2 thread | `bm_adin2111.c:576` |
| `retry_negotiation` → `GetAnStatus`/`Renegotiate` | 100 ms renegotiate timers for every port whose link is down | FreeRTOS timer task | `l2.c:87-177` |
| `trait->enable_port`/`disable_port` | `bm_l2_netif_enable_disable_port()`, used by `port_monitor` | timer task | `l2.c:691`, `src/lib/common/port_monitor.c:53` |
| `trait->port_stats` | Metrics service request (`adin_port_stats_*`) | middleware/service | `adin2111_network_metrics.c` |
| `trait->port_stats` | Stress test stats timer (gated by `bm_l2_get_port_state`) | timer task | `src/lib/common/stress.c:274-290` |
| Raw GPIO pokes | Debug GPIO CLI | CLI task | `app_main.cpp:280` |

### 4.2 Indirect (application-visible things that depend on the link)

| Component | Behaviour while ADIN is off | Action needed |
|-----------|-----------------------------|---------------|
| `user_code.cpp` `loop()` → `bm_pub_wl()` with the `sensor/<id>/sofar/bm_soft_temp` data | Dropped silently (return value isn't checked) | Decide whether to drop or buffer readings while off (§6.4) |
| `spotter_log()` / `spotter_log_console()` (user code, `SensorWatchdog`, `port_monitor`) | Dropped | Same |
| `powerSampler` (INA232) publishes | Dropped | Same |
| BCMP heartbeat timer (10 s, `bcmp.c:39`) | Keeps firing and sends heartbeats into lwIP/L2 | Harmless once L2 gates on power state |
| Neighbors (`neighbors.c`) | Neighbors marked offline after 2 heartbeat periods (≈ 20 s) | None. They are rediscovered on link-up (`bcmp_link_change` sends a heartbeat) |
| Remote side (bridge/Spotter topology) | Node disappears from topology and resource discovery | Expected. Make sure the upstream side tolerates it |
| `APP_PUB_SUB_UTC_TOPIC` RTC sync | No updates while off. RTC free-runs | None |
| BCMP DFU (`dfu_core.c`) | An in-progress DFU will time out or fail | **Don't power down unless DFU is idle** (§6.1) |
| BCMP config set/commit, bm services (sys_info, echo, config_cbor_map, metrics) | Unreachable | None |
| `port_monitor` one-shot timer (`disableUnusedPortsTimeMs`, default 0 = off) | If it fires while ADIN is off, it does SPI and calls `link_change` | Gate it, or don't power down before it has fired |
| `stress` test (UDP/CLI started) | TX/stats timers keep running | Call `stress_stop_tx()` before power-down |
| lwIP netif | Doesn't block TX on its own: `udp_sendto_if`/`raw_sendto_if_src` skip the up check | L2 must be the gate |

---

## 5. Existing de-init pieces and their gaps

### 5.1 `bm_l2_netif_set_power(false)` (`l2.c:669`)
Calls `bm_l2_set_netif(false)` and then `trait->disable()`. Used only by the
bridge app (`src/apps/bridge/bridgePowerController.cpp:179`).

Gaps:
- **G1 — lwIP core lock.** `bm_l2_set_netif()` (`network/bm_lwip.c:429`)
  calls `netif_set_link_down()`/`netif_set_down()` without
  `LOCK_TCPIP_CORE()`. These call `LWIP_ASSERT_CORE_LOCKED()`, which this
  port (`sys_check_core_locking`, `LWIP_FREERTOS_CHECK_CORE_LOCKING=1`) turns
  into `configASSERT(0)` (`src/lib/lwip/lwip_support.c`).
- **G2 — L2 port mask not cleared.** `trait->disable()` doesn't generate
  link-change events, so `CTX.enabled_ports_mask` stays set and `bm_l2_tx()`
  keeps sending to "up" ports. Link-change subscribers (BCMP, port_monitor)
  aren't told either.
- **G3 — no serialization with the L2 thread.** `disable()` runs in the
  caller's task while the L2 thread may be in the middle of
  `handle_interrupt` or `send` on the same driver instance.
- **G4 — SW power-down IRQ race.** Putting the PHYs into SW power-down
  drops the links. That raises ADIN INT, which queues `L2Irq`, but power is
  cut right after, so the L2 thread services the IRQ against a dead chip.

### 5.2 `adin2111_netdevice_disable()` (`bm_adin2111.c:372`)
`DisablePort(1..2)` (SW power-down), then `power(false)`.

Gaps:
- **G5 — no `adin2111_UnInit()`.** The MAC state stays `READY`, so any later
  `SubmitTxBuffer` goes straight to `MAC_ProcessTxQueue()`, which does SPI.
  `UnInit` would set the MAC/PHY state to uninitialized, NULL the handles
  (later calls then return `ADI_ETH_INVALID_HANDLE` instead of doing SPI)
  and disable the EXTI8 NVIC line through `HAL_DisableIrq()`.
- **G6 — TX buffer leak.** Frames still in `pMacDevice->txQueue`
  (`TX_QUEUE_NUM_ENTRIES` = 4, `bm_malloc` + `aligned_malloc`) are never
  freed. The next `adin2111_Init()` → `MAC_Init()` `memset`s the device memory
  (`adi_mac.c:493`), so their pointers are lost. Flooded frames are queued
  twice with `refCount = 2` (`adin2111.c:760+`), so draining has to respect
  `refCount`.
- **G7 — stale IRQ callback.** `ADIN2111_MAC_INT_CALLBACK` and `LINK_CHANGE`
  are not cleared. If an `L2Irq` arrives, `adin2111_handle_interrupt()` still
  calls into the MAC driver.
- **G8 — partial failure leaves power on.** If `DisablePort` fails (for
  example the chip is already wedged), `power(false)` is skipped.

### 5.3 `bcl_power_callback(false)` (`bristlemouth_client.cpp:43`)
- **G9 — back-powering.** It only drops `ADIN_PWR`. `ADIN_CS` (PA15) and
  `ADIN_RST` (PA0) stay driven high, and SPI3 SCK/MOSI stay in AF push-pull.
- **G10 — power-up timing on a cold rail.** At boot `ADIN_PWR` has been high
  since `bspInit()`, so the 1 ms reset pulse plus 100 ms wait is enough.
  After a runtime power-off the rails start from 0 V. RST is released about
  1 ms after power-on, but internal power-good can take up to ~83 ms (ramp +
  t1), and PHY2 needs another 90 ms after that. `adin2111_Init()` only
  retries the PHY2 probe 200 times (`ADIN2111_PHY_POWERUP_MAX_RETRIES`,
  roughly milliseconds), so a re-power can fail with `BmENODEV`.

### 5.4 Things that must *not* be used to re-power
- `bcl_init()` / `bristlemouth_init()` / `adin2111_init()`. These
  re-register the metrics components and BCMP/L2 resources, create
  duplicate tasks, queues and timers, and re-run `device_init`.
- `bm_l2_deinit()` + `bm_l2_init()`. This deletes the L2 queue that the ISR
  and BCMP use, and the task that other modules expect to exist.
- `spiInit(&spi3)`. It creates a new mutex and asserts on the 4th DMA
  registration.

---

## 6. Required steps

### 6.1 Preconditions (app level, before requesting power-down)
1. **DFU idle.** No BM DFU should be in progress. bm_core has no public
   getter today (`get_current_state_enum()` is internal to `dfu_core.c`), so
   add something like `bool bm_dfu_is_idle(void)` in bm_core.
2. **Stress test stopped.** Call `stress_stop_tx()` (stops the TX and stats
   timers).
3. **Port monitor.** If `disableUnusedPortsTimeMs != 0`, either wait for its
   one-shot timer to fire before allowing power-down, or make
   `bm_l2_netif_enable_disable_port()` a no-op while powered down (see 6.2).
4. **Flush anything important.** Publish any pending data or logs and give
   the middleware/L2 queues a moment to drain. Anything still queued will be
   dropped (not leaked) once 6.2 is in place.

### 6.2 bm_core changes (`src/lib/bm_core`, upstream PR)

**a) L2: serialized power API with a powered flag** (`network/l2.c`/`l2.h`)

Add a power state to `BmL2Ctx` and run the power transition **inside the L2
thread** so it can't interleave with `send`/`handle_interrupt`:

```c
// l2.c
typedef enum { L2Tx, L2Rx, L2Irq, L2PowerOff, L2PowerOn } BmL2QueueType;
// BmL2Ctx: add  bool powered;  BmSemaphore power_done;  BmErr power_err;

static BmErr bm_l2_tx(void *buf, uint32_t length, uint16_t port_mask) {
  if (!CTX.powered) { bm_l2_free(buf); return BmENETDOWN /* or BmOK */; }
  ...
}

// In bm_l2_thread():
case L2Irq:
  if (CTX.powered) { CTX.network_device.trait->handle_interrupt(...); }
  break;
case L2Tx:
  if (CTX.powered) { bm_l2_process_tx_evt(&event); } else { bm_l2_free(event.buf); }
  break;
case L2PowerOff: {
  CTX.powered = false;
  for (uint8_t p = 1; p <= CTX.num_ports; p++) {
    bm_l2_stop_renegotiate_check(p);            // no SPI polling while off
    if (CTX.enabled_ports_mask & (1U << (p - 1))) {
      CTX.enabled_ports_mask &= ~(1U << (p - 1));
      L2LinkChangeData d = {p, false};
      ll_traverse(&CTX.link_change_callback_list, link_change_cbs, &d);
    }
  }
  CTX.power_err = CTX.network_device.trait->disable(CTX.network_device.self);
  bm_semaphore_give(CTX.power_done);
  break;
}
case L2PowerOn: {
  CTX.power_err = CTX.network_device.trait->enable(CTX.network_device.self);
  if (CTX.power_err == BmOK) {
    CTX.powered = true;
    for (uint8_t p = 1; p <= CTX.num_ports; p++) {
      bm_l2_start_renegotiate_check(p);         // same as at boot
    }
    // link-up events arrive via the normal IRQ -> link_change(true) path
  }
  bm_semaphore_give(CTX.power_done);
  break;
}
```

Public wrappers post the event and block on `power_done`:
`BmErr bm_l2_power_down(void)` / `BmErr bm_l2_power_up(void)`. Also:
- Make `bm_l2_netif_enable_disable_port()` return `BmENETDOWN` (and skip
  `link_change`) when `!CTX.powered`.
- Make `link_change()` ignore events while `!CTX.powered`.
- Make `bm_l2_netif_set_power()` delegate to these, and fix
  `bm_l2_set_netif()` to wrap the lwIP calls in
  `LOCK_TCPIP_CORE()/UNLOCK_TCPIP_CORE()` (G1). Calling `bm_l2_set_netif()`
  is optional for this use case because the L2 gate already stops TX. If you
  do use it, set the netif down *before* `L2PowerOff` and up *after*
  `L2PowerOn`. `netif_set_link_up()` re-issues MLD reports, which need a live
  link.
- Set `CTX.powered = true` in `bm_l2_init()` once `enable()` succeeds.

Handles G2, G3, G4 (IRQs from the SW power-down are ignored once
`powered=false`), and the renegotiate timer problem.

**b) ADIN2111 driver: real uninit** (`drivers/adin2111/bm_adin2111.c`)

```c
static void drain_tx_queue(void) {
  adi_mac_Device_t *mac = DEVICE_STRUCT.pMacDevice;
  if (!mac) return;
  while (!queueIsEmpty(&mac->txQueue)) {
    adi_eth_BufDesc_t *d = mac->txQueue.pEntries[mac->txQueue.tail].pBufDesc;
    queueRemove(&mac->txQueue);
    if (d && d->refCount && --d->refCount == 0) {
      free_tx_buffer(d);      // flooded frames appear twice (refCount 2)
    }
  }
}

static BmErr adin2111_netdevice_disable(void) {
  // Best effort: put PHYs in SW power-down so link partners see a clean drop.
  for (int i = 1; i <= ADIN2111_PORT_NUM; i++) {
    (void)adin2111_netdevice_disable_port(i);
  }
  ADIN2111_MAC_INT_CALLBACK = NULL;             // G7: handle_interrupt -> no SPI
  ADIN2111_MAC_INT_CALLBACK_PARAM = NULL;
  LINK_CHANGE.device_handle = NULL;
  LINK_CHANGE.port_mask = 0;
  drain_tx_queue();                             // G6
  (void)adin2111_UnInit(&DEVICE_STRUCT);        // G5: NVIC EXTI8 off, handles NULL
  if (NETWORK_DEVICE.callbacks->power) {
    NETWORK_DEVICE.callbacks->power(false);     // G8: always cut power
  }
  return BmOK;
}
```

Notes:
- Do the drain in the L2 thread (6.2a) so no OA transfer is mid-flight. In
  OA mode, also clear any partially-sent buffer the OA state machine is
  holding (`oaTxCurBufIdx` and friends) before `UnInit`. Inspect
  `adi_spi_oa.c` around the TX path when implementing.
- RX buffers are static (`RX_BUFFERS`) and get re-submitted by
  `adin2111_netdevice_enable()`, so they don't need handling.
- `adin2111_netdevice_enable()` is safe to run again. It re-runs `MAC_Init()`
  (fresh queues and callbacks), the PHY init, re-registers the link-change
  callback and resubmits RX. Boot already calls it twice.
- `adin2111_init()` must **not** be called again: it re-runs
  `adin2111_network_metrics_init()` and registers duplicate metrics
  components.

**c) DFU idle getter** (`bcmp/dfu_core.c`): expose `bm_dfu_is_idle()` (state
is `BmDfuStateIdle` or `BmDfuStateError`).

### 6.3 bm_protocol changes

**a) `bcl_power_callback()`** (`src/lib/bm_integration/bristlemouth_client.cpp:43`) — fixes G9 and G10:

```c
#define ADIN_POWER_SETTLE_MS  (100)   // tRAMP(40) + t1(43) + margin, verify on HW
#define ADIN_OFF_DISCHARGE_MS (50)    // let rails collapse for a clean POR, verify on HW

void bcl_power_callback(bool on) {
  if (on) {
    adin_spi_pins_restore();          // see below
    IOWrite(&ADIN_RST, 0);            // hold in reset while rails ramp
    IOWrite(&ADIN_CS, 1);
    IOWrite(&ADIN_PWR, 1);
    vTaskDelay(pdMS_TO_TICKS(ADIN_POWER_SETTLE_MS));
    IOWrite(&ADIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(AFTER_RESET_DELAY));   // >= 90 ms for PHY2
  } else {
    IOWrite(&ADIN_RST, 0);            // HW power-down first: ADIN I/Os tri-state
    IOWrite(&ADIN_CS, 0);             // don't drive into unpowered VDDIO
    adin_spi_pins_park();
    IOWrite(&ADIN_PWR, 0);
    vTaskDelay(pdMS_TO_TICKS(ADIN_OFF_DISCHARGE_MS));
  }
}
```

Keep the boot path's behaviour the same. The extra settle delay only costs
about 100 ms at boot (twice, because of the double enable). If that matters,
skip it when `ADIN_PWR` is already high.

**b) Park and restore SPI3** (add to the BSP, e.g. `bsp.cpp`):

```c
void adin_spi_pins_park(void) {
  xSemaphoreTake(spi3.mutex, portMAX_DELAY);  // no transfer in progress
  HAL_SPI_DeInit(&hspi3);   // MspDeInit: SPI3 clk off, DMA ch12/13 deinit,
                            // PB3/PB4/PB5 -> analog (Hi-Z)
  xSemaphoreGive(spi3.mutex);
}
void adin_spi_pins_restore(void) {
  xSemaphoreTake(spi3.mutex, portMAX_DELAY);
  MX_SPI3_Init();           // NOT spiInit(): that would recreate the mutex
                            // and overflow the DMA slot table (assert)
  xSemaphoreGive(spi3.mutex);
}
```

Notes:
- SPI3 is used only by the ADIN on this board (`adin_pins`), so deinit
  doesn't affect other drivers. `spi1` (TSYS01 `BM_CS`) and `spi2`
  (W25 flash) are separate.
- PB3/PB4 are JTDO/NJTRST. Analog mode is fine (SWD on PA13/PA14 is
  unaffected).
- `MX_SPI3_Init()` → `HAL_SPI_Init()` → `HAL_SPI_MspInit()` re-links DMA
  through `__HAL_LINKDMA`, so the `spiTxRxNonblocking` DMA path works again.
  The protected-SPI `dma_id` slot is unchanged.
- `ADIN_INT` (PB8) is already an input with no pull. While the ADIN is off,
  its pull-up has no supply, so the line reads low. Because `UnInit`
  disabled the NVIC line, the falling edge only leaves EXTI8 pending.
  `MAC_Init()` disables the NVIC line again on re-init, and the driver
  re-enables it itself. A stale pending edge then just causes one harmless
  `L2Irq`. For belt-and-braces you can clear the flag with
  `LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_8)` after power-up.

**c) Defensive guard in `HAL_SpiReadWrite()`** (`src/lib/drivers/adin2111/src/adi_hal.c:58`)

Add a `static volatile bool adin_spi_enabled` that is set and cleared by
park and restore. Have `HAL_SpiReadWrite()` return an error without touching
`ADIN_CS` when it is false. This catches any path missed above. Today a stray
call would drive CS high into the unpowered chip.

### 6.4 bm_soft_module app integration

Add a small power-control module, or put it in `user_code.cpp`, that the app
calls when it wants the network off:

```c
bool soft_network_power_down(void) {
  if (!bm_dfu_is_idle()) return false;                 // 6.1.1
#ifdef STRESS_TEST_ENABLE
  stress_stop_tx();                                    // 6.1.2
#endif
  // Optional: let pending pubs/logs drain (L2 queue is 32 deep)
  vTaskDelay(pdMS_TO_TICKS(50));
  return bm_l2_power_down() == BmOK;                   // 6.2a + 6.2b + 6.3a/b
}

bool soft_network_power_up(void) {
  // bm_l2_power_up() -> trait->enable() -> bcl_power_callback(true)
  // -> adin2111_Init() + RX resubmit + EnablePort(1,2); renegotiate timers restarted.
  for (int attempt = 0; attempt < 3; attempt++) {     // G10: allow a retry
    if (bm_l2_power_up() == BmOK) return true;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return false;
}
```

App behaviour to decide on:
- **Data while off:** `loop()` keeps sampling the TSYS01 and calling
  `bm_pub_wl()`. Those calls now fail fast and the data is dropped. If the
  readings matter, keep a ring buffer and publish it after power-up (and
  check the `bm_pub_wl()` return value).
- **Logs while off:** `spotter_log*` calls (including the SensorWatchdog and
  port_monitor messages) are dropped. Mirror them to the local console if
  needed.
- **Rejoin after power-up:** the link comes up in roughly 100 ms to a few
  seconds, depending on autonegotiation. `bcmp_link_change(true)` sends a
  heartbeat right away, and neighbors and topology rebuild. Allow a few
  seconds after `soft_network_power_up()` before publishing anything
  time-critical.
- **Low power:** `LPM_SPI3` is only asserted during transfers, so
  `lpmPeripheralActive/Inactive` needs no changes. With the ADIN off and L2
  gated, the 100 ms renegotiate timer and the ADIN IRQs no longer wake the
  MCU.
- **Watchdog (IWDG):** power-up blocks the calling task for about 200 ms
  plus init time, so don't call it from a task with a tight watchdog
  deadline.

### 6.5 Ordered power-down sequence (summary)

1. App checks preconditions (DFU idle, stress stopped, port_monitor done).
2. App calls `bm_l2_power_down()`. The rest runs in the L2 thread:
   1. `CTX.powered = false`. From here, TX events are freed and IRQ events
      ignored.
   2. Stop the renegotiate timers for all ports.
   3. Clear `enabled_ports_mask` and send `link_change(port, false)` to the
      subscribers.
   4. `trait->disable()`:
      1. `DisablePort(1)` and `DisablePort(2)` (SW power-down, best effort).
      2. Clear the INT callback and `LINK_CHANGE` state.
      3. Drain and free the MAC TX queue (respecting `refCount`).
      4. `adin2111_UnInit()`. MAC/PHY are uninitialized and NVIC EXTI8 is
         disabled.
      5. `bcl_power_callback(false)`: RST low, then CS low, then park SPI3
         pins (Hi-Z), then `ADIN_PWR` low, then the discharge delay.
3. (Optional) `bm_l2_set_netif(false)` under the TCP/IP core lock.

### 6.6 Ordered power-up sequence (summary)

1. (Optional) Nothing to do for lwIP yet.
2. App calls `bm_l2_power_up()`, which runs in the L2 thread:
   1. `trait->enable()` → `bcl_power_callback(true)`: restore SPI3, RST low,
      CS high, `ADIN_PWR` high, settle, release RST, wait ≥ 90 ms.
   2. `adin2111_Init()` (fresh MAC queues and callbacks, NVIC handled by the
      driver), register the link-change callback, resubmit the RX buffers,
      `SyncConfig`, `EnablePort(1,2)`.
   3. `CTX.powered = true` and restart the renegotiate timers for all ports.
3. Link-up IRQs arrive and `link_change(port, true)` runs: the port mask is
   set, the renegotiate timer for that port stops, BCMP sends a heartbeat,
   and port_monitor and other subscribers are notified.
4. (Optional) `bm_l2_set_netif(true)` under the core lock.
5. On failure, the driver has already called `power(false)`. Retry with a
   delay.

---

## 7. Why the existing bridge usage isn't a template

`bridgePowerController.cpp` calls `bm_l2_netif_set_power(false)` after
cutting the bus. It inherits G1–G9. In practice it may get away with some of
them because the bus power loss drops the links first, but it is not a safe
pattern to copy for `bm_soft_module`. The fixes in §6.2 would benefit the
bridge too.

---

## 8. Alternative: hardware power-down via `ADIN_RST` only

If the goal is power savings rather than a full rail cut, holding
`ADIN_RST` low puts the ADIN2111 into **hardware power-down**. Per the
datasheet: all analog and digital circuits are disabled, clocks gated, all
I/O tri-stated, and consumption is leakage only.

Advantages:
- No back-powering concern. VDDIO stays up, so the MCU pins can stay where
  they are (still set CS low and park SPI3 for good measure).
- INT keeps its pull-up, so the line stays high and there is no spurious
  edge.
- Faster and more deterministic wake: release RST and wait 50 ms (PHY1) or
  90 ms (PHY2). No rail ramp.

All of §6.1, §6.2 and the app integration still apply. Only
`bcl_power_callback()` changes: on `false`, drive `RST=0` and leave
`ADIN_PWR` high. On `true`, pulse RST and wait ≥ 90 ms. Measure the
leakage-only current against a full `ADIN_PWR` cut on real hardware to
decide. The remaining difference is whatever else sits on the switched rail
(regulator quiescent current, LEDs, magnetics bias, if any).

---

## 9. Open questions / verify on hardware

1. **What `ADIN_PWR` switches** (AVDD_H, AVDD_L, VDDIO, DVDD) and whether
   anything else sits on that rail. Get this from the schematic. It
   determines whether G9 (back-power) applies and how big the INT edge is.
2. **Rail discharge and ramp times** on the real board, to set
   `ADIN_OFF_DISCHARGE_MS` and `ADIN_POWER_SETTLE_MS`.
3. **The OA SPI driver's in-flight TX state** (`adi_spi_oa.c`) needs a
   careful look when implementing `drain_tx_queue()`, so no buffer is freed
   twice or leaked.
4. **G1 behaviour.** Confirm on target that `bm_l2_set_netif()` from an app
   task trips the core-locking assert. It does by code reading, and the
   bridge path should be checked as well.
5. **Upstream ownership.** §6.2 changes live in `bristlemouth/bm_core`.
   Coordinate the API names (`bm_l2_power_down/up`, `bm_dfu_is_idle`) with
   the bm_core maintainers.

---

## 10. Test plan

1. **Leak test:** run 1000 power-down/power-up cycles under publish load
   (stress test or a fast `softReadingPeriodMs`). Heap high-water and free
   heap should stay flat (`memory_metrics`, the debug `sys` CLI).
2. **No SPI while off:** put a logic analyzer on PB3/PB5/PA15/PA0. Expect
   zero edges from power-down until power-up, including across BCMP
   heartbeat periods (10 s) and port_monitor timeouts.
3. **Pin levels while off:** with a scope or DMM, check that
   CS/SCK/MOSI/RST are ≤ 0.3 V and that the ADIN VDDIO rail doesn't float up
   (no back-power).
4. **IRQ:** count EXTI8 entries across power-down (expect 0 or 1 stale) and
   check that no `L2Irq` reaches `handle_interrupt` while `powered == false`.
5. **Rejoin:** after power-up, check that both ports come back, the
   neighbors table repopulates (`bm neighbors` CLI), Spotter receives
   `bm_soft_temp` again, and UTC sync resumes.
6. **Negative cases:** request power-down during a DFU (must be refused),
   power-up with a port cable unplugged (renegotiate timer must run for that
   port), and power-up failure (simulate by holding RST; the retry path must
   recover with no leak).
7. **Boot regression:** cold boot still brings the network up with the
   modified `bcl_power_callback()`.
