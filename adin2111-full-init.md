# ADIN2111 Power-On Initialization — `bm_soft_module`

This document traces, end to end, how the ADIN2111 2-port 10BASE-T1L Ethernet switch is brought up when a `bm_soft_module` board powers on. It covers every layer from `main()` down to individual register writes, with file/line references.

> Scope: `APP=bm_soft_module`, `BSP=bm_mote_spi_v1_0` (the only BSP this app is built/released for — see `tools/scripts/release/configs/default.yml:63` and `tools/scripts/test/configs/mote.yaml:146`). Driver code lives in the `src/lib/bm_core` submodule (v0.13.12).

---

## 1. TL;DR

1. **Boot (`main`)**: MCU clocks set up (SYSCLK = 160 MHz). The FreeRTOS scheduler starts and runs `defaultTask`.
2. **`mxInit()` → `MX_GPIO_Init()`**: `ADIN_PWR`=0 (off), `ADIN_RST`=0 (held in reset), `ADIN_CS`=0. `ADIN_INT` (PB8) is set up as a falling-edge EXTI and its NVIC line is enabled.
3. **`bspInit()`**: SPI3 is initialized (20 MHz, mode 0, software CS, GPDMA ch12/13). Then `ADIN_PWR`=1 turns the ADIN2111 on, but it stays in reset.
4. **`bcl_init()`**: registers the `ADIN_INT` ISR callback and calls `bristlemouth_init()` → `adin2111_init()`.
5. **`adin2111_init()`** allocates 8 RX buffers and runs **`adin2111_netdevice_enable()` (pass #1)**:
   - Power callback: CS high, RESET pulsed low for about 1 ms, then a 100 ms wait.
   - **MAC**: software reset (`RESET.SWRESET`). Poll `PHYID` until it reads `0x0283BCA1`, wait for `STATUS0.RESETC`, then program IMASK0/1, CONFIG0 (FCS validation off) and CONFIG2 (CRC append on).
   - **Switch**: CONFIG2 is set to forward unknown-DA frames to the host on both ports, with port-to-port cut-through off.
   - Wait for PHY 2 to come out of reset.
   - **PHY 1 & PHY 2** (over the SPI→MDIO bridge): check the ID, enter software power-down, set the IRQ masks, clear IRQ status, apply the rev-0 static config (if needed) and enable auto-negotiation.
   - Unmask PHY interrupts in the MAC, configure the TS pins (unassigned), and enable the PHY *link-status-change* interrupt on both PHYs.
   - Submit the 8 RX buffers. **`SyncConfig`** sets `CONFIG0.SYNC` and enables the MCU's EXTI8 IRQ.
   - **Exit software power-down** on port 1, then port 2. Auto-negotiation starts.
6. **`bm_l2_init()`** sets up the L2 callbacks and task, then calls `trait->enable()` again. That runs **pass #2, an exact repeat of step 5** (another hardware reset and a full re-init).
7. **Runtime**: `ADIN_INT` falling edge → ISR queues an `L2Irq` → the L2 thread runs the ADI OPEN Alliance IRQ state machine. Link changes reach L2. A 100 ms renegotiation timer per port re-triggers auto-negotiation until a link comes up.

---

## 2. Hardware and build context

### 2.1 MCU ↔ ADIN2111 wiring (`src/bsp/bm_mote_spi_v1_0/Core/Inc/main.h`)

| Signal | MCU pin | Direction | Notes |
|---|---|---|---|
| `ADIN_PWR` | PH1 | out, push-pull | Load switch / regulator enable for the ADIN2111 |
| `ADIN_RST` | PA0 | out, push-pull | ADIN2111 `RESET` (active low, > 10 µs pulse required) |
| `ADIN_CS` | PA15 | out, push-pull | SPI chip select, driven manually by firmware (not by SPI NSS) |
| `ADIN_SCK` | PB3 | AF6 (SPI3_SCK) | |
| `ADIN_MISO` | PB4 | AF6 (SPI3_MISO) | ADIN2111 `SDO/SPI_CFG0` |
| `ADIN_MOSI` | PB5 | AF6 (SPI3_MOSI) | |
| `ADIN_INT` | PB8 | in, no pull, EXTI8 falling | ADIN2111 `INT` (open-drain, active low; needs external pull-up) |

The pin handles are bundled for the driver in `src/bsp/bm_mote_spi_v1_0/bsp.cpp:58`:

```c
adin_pins_t adin_pins = {&spi3, &ADIN_CS, &ADIN_INT, &ADIN_RST};
```

### 2.2 SPI3 configuration (`Core/Src/spi.c:138`, MSP at `:322`)

| Parameter | Value |
|---|---|
| Mode | Master, full duplex, 8-bit, MSB first |
| CPOL / CPHA | Low / 1st edge (SPI mode 0) |
| NSS | Software (CS is a GPIO) |
| Kernel clock | SYSCLK = 160 MHz (MSI 48 MHz → PLL M=3, N=10, R=1), see `Core/Src/main.c:145-192` |
| Prescaler | /8 → **20 MHz SCLK** (ADIN2111 max is 25 MHz) |
| DMA | GPDMA1 ch13 (TX), ch12 (RX), byte width |
| Max transfer | 1023 bytes per HAL call (SPI3 counter limit). `HAL_SpiReadWrite` splits longer transfers while holding CS low (`src/lib/drivers/adin2111/src/adi_hal.c:58-98`) |

SPI3 is wrapped in a `PROTECTED_SPI` (FreeRTOS mutex + low-power-manager hook `LPM_SPI3`), `bsp.cpp:51`.

### 2.3 ADIN2111 hardware straps (must match firmware)

The ADIN2111 latches its SPI protocol from `SPI_CFG1` (`P1_LED_0`) and `SPI_CFG0` (`SDO`) when `RESET` is released (datasheet Table 22):

| SPI protocol | SPI_CFG1 | SPI_CFG0 |
|---|---|---|
| **OPEN Alliance with protection** ← firmware assumes this | 0 | 0 |
| OPEN Alliance without protection | 0 | 1 |
| Generic SPI with CRC8 | 1 | 0 |
| Generic SPI without CRC8 | 1 | 1 |

The firmware is compiled for OA SPI with protection (`src/lib/bm_core/drivers/adin2111/adi_config.h`):

```c
#define CONFIG_ETH_ADIN2111 1
#define CONFIG_SPI_OA_EN    1   // OPEN Alliance TC6 SPI protocol
#define CONFIG_SPI_PROT_EN  1   // each control word is followed by its bitwise complement
```

`CONFIG0.PROTE` is read-only and set from the strap. The driver never writes it. If the board straps don't select OA with protection, every control transaction fails (the echoed-header check or the complement check fails), and init ends in `ADI_ETH_SW_RESET_TIMEOUT` or `ADI_ETH_COMM_TIMEOUT`.

### 2.4 Compile-time driver parameters

| Symbol | Value | Where |
|---|---|---|
| `RX_QUEUE_NUM_ENTRIES` | 8 | `adi_mac.h:45` |
| `TX_QUEUE_NUM_ENTRIES` | 4 | `adi_mac.h:41` |
| `MAX_FRAME_SIZE` | 1518 | `adi_mac.h:129` |
| RX buffer size | 1518 + 4 (FCS) + 2 (frame hdr) = 1524 B, 4-byte aligned | `bm_adin2111.c:10` |
| `ADI_MAC_SPI_ACCESS_SIZE` | 4 bytes | `adi_mac.h:92` |
| `ADI_MAC_IF_UP_MAX_RETRIES` / `ADI_MAC_INIT_MAX_RETRIES` | 25000 | `adi_mac.h:120,124` |
| `ADI_MAC_MDIO_MAX_RETRIES` | 10 | `adi_mac.h:105` |
| `ADIN2111_PHY_POWERUP_MAX_RETRIES` | 200 | `adin2111.h:65` |
| `ADI_PHY_SYS_RDY_ITER` | 700 ms / 1.25 ms = 560 | `adi_phy.h:279-284` |
| `ADI_PHY_SOFT_PD_ITER` | 10 | `adi_phy.h:287` |
| `MIN_SIZE_FOR_DMA` | 16 bytes | `adi_hal_port_specific.h` |
| `ADI_EDGE_SENSITIVE_IRQ` | 1 | `adi_hal_port_specific.h:62` |
| `fcsCheckEn` | `false` (MAC appends the FCS on TX) | `bm_adin2111.c:31` |
| `tsTimerPin` / `tsCaptPin` | `..._MUX_NA` (no timestamp pins) | `bm_adin2111.c:32-33` |

---

## 3. Boot timeline

### Phase 0 — `main()` (`src/apps/bm_soft_module/app_main.cpp:165`)

```
HAL_Init() → SystemClock_Config() → SystemPower_Config_ext()
xTaskCreate(defaultTask, prio 2) → vTaskStartScheduler()
```

The ADIN2111 is not touched here. All of its init happens in `defaultTask` (priority 2).

### Phase 1 — GPIO defaults: `mxInit()` → `MX_GPIO_Init()` (`app_main.cpp:341`, `bsp.cpp:113`, `Core/Src/gpio.c:42`)

| Pin | State after `MX_GPIO_Init` | Effect on ADIN2111 |
|---|---|---|
| `ADIN_PWR` | **0** (`gpio.c:58`) | Unpowered |
| `ADIN_RST` | **0** (`gpio.c:61`) | Held in reset |
| `ADIN_CS` | **0** (`gpio.c:61-62`) | CS asserted (harmless while the chip is unpowered or in reset) |
| `ADIN_INT` | input, no pull. EXTI8 set to falling edge, NVIC prio 6, **enabled** (`gpio.c:136-162`) | — |

### Phase 2 — `bspInit()` (`app_main.cpp:369`, `bsp.cpp:61`)

```c
osStarted = true; HAL_SuspendTick();   // HAL_Delay/HAL_GetTick now use the FreeRTOS tick
spiInit(&spi1); spiInit(&spi2); spiInit(&spi3);   // MX_SPI3_Init + DMA + mutex
i2cInit(&i2c1);
IOWrite(&ADIN_PWR, 1);                 // bsp.cpp:72 — ADIN2111 powered on (RESET still low)
```

From here until Phase 4 the ADIN2111 is **powered but held in hardware reset**. Several other subsystems initialize in between (USB, debug CLIs, W25 flash NVM partitions, …), so the chip sits in reset for a variable but non-trivial time.

### Phase 3 — `bcl_init()` (`app_main.cpp:393`, `src/lib/bm_integration/bristlemouth_client.cpp:54`)

```c
IORegisterCallback(adin_pins.interrupt, network_device_interrupt, NULL); // ADIN_INT ISR hook
HAL_Init_Hook();          // no-op (adi_hal.c:52)
config_init();
port_monitor_init();      // optional timer to disable ports that never link (config "disableUnusedPortsTimeMs", default 0 = off)
device_init(device);      // node id, fw version, etc.
bristlemouth_init(bcl_power_callback);
```

`bristlemouth_init()` (`src/lib/bm_core/middleware/bristlemouth.c:12`):

```c
NetworkDevice network_device = adin2111_network_device();  // builds trait/callback tables
network_device.callbacks->power = net_power_cb;             // = bcl_power_callback
bm_err_check(err, adin2111_init());          // ← Phase 4 (pass #1)
bm_err_check(err, bm_l2_init(network_device)); // ← Phase 5 (pass #2)
bm_err_check(err, bm_ip_init());
bm_err_check(err, bcmp_init(network_device));
... topology, services, pubsub, middleware, metrics
```

`bm_err_check` only runs the next step if every earlier step returned `BmOK`. A failure in `adin2111_init()` skips everything after it, and `bcl_init()` prints `Failed to initialize bristlemouth`.

### Phase 4 — `adin2111_init()` (`src/lib/bm_core/drivers/adin2111/bm_adin2111.c:758`)

```c
// once per boot (static bool initialized)
for i in 0..7: RX_BUFFERS[i].pBuf = aligned_malloc(4, 1524); bufSize = 1524; cbFunc = receive_callback;
create_network_device();
err = adin2111_netdevice_enable();      // ← detailed below
adin2111_network_metrics_init();        // registers metrics components only, no HW access
```

#### 4.1 `adin2111_netdevice_enable()` (`bm_adin2111.c:294`)

```
power(true)                              → 4.2
adin2111_Init(&DEVICE_STRUCT, &DRIVER_CONFIG)   → 4.3 – 4.9
adin2111_RegisterCallback(link_change_callback_, ADI_MAC_EVT_LINK_CHANGE)  → 4.10
for 8 RX buffers: adin2111_SubmitRxBuffer()     → 4.11
adin2111_SyncConfig()                    → 4.12
adin2111_EnablePort(1); adin2111_EnablePort(2)  → 4.13
on any failure: power(false)            → ADIN_PWR = 0
```

#### 4.2 Power and hardware reset — `bcl_power_callback(true)` (`bristlemouth_client.cpp:43`)

```c
IOWrite(&ADIN_PWR, 1);              // already 1 from bspInit on the first pass
IOWrite(&ADIN_CS, 1);               // deassert CS before releasing reset
IOWrite(&ADIN_RST, 0);
vTaskDelay(pdMS_TO_TICKS(1));       // RESET low ≥ 0–1 tick (1 kHz tick) — datasheet needs > 10 µs
IOWrite(&ADIN_RST, 1);              // straps latched here (SPI_CFG0/1, Px_SWPD_EN, Px_TX2P4_EN)
vTaskDelay(pdMS_TO_TICKS(100));     // datasheet: registers accessible ≤ 90 ms after RESET deassert
```

#### 4.3 MAC driver init — `MAC_Init()` (`adi_mac.c:478`)

Called from `adin2111_Init()` (`adin2111.c:79`) after it carves `DEVICE_MEMORY` into a MAC region and two PHY regions (PHY MDIO addresses **1** and **2**, `enableIrq = false`).

1. `HAL_DisableIrq()` → `NVIC_DisableIRQ(EXTI8_IRQn)`. The MCU stops taking ADIN interrupts until SyncConfig.
2. `memset` the MAC device memory. Reset state, callbacks, status shadows and error stats. Init the TX (5 slots) and RX-LP (9 slots) queues.
3. `HAL_SpiRegisterCallback(spiCallback, hDevice)`. `HAL_SpiReadWrite` calls `spiCallback` after each transfer, which advances the OA state machine (`adi_hal.c:102`).
4. OA state: `oaTxc = 31` (TX credits), `oaRca = 0`, **`oaCps = 6` (64-byte chunks)**, `oaMaxChunkCount = 16`.
5. **`MAC_Reset(ADI_ETH_RESET_TYPE_MAC_PHY)`** (`adi_mac.c:738`):
   - Disable the IRQ again.
   - Write `RESET (0x03) = SWRESET (bit 0)`, retrying up to 25 000× until the SPI write succeeds. The first accesses can fail while the chip is still coming up, because the OA echoed-header check fails on all-0 data.
   - **`waitDeviceReady()`** (`adi_mac.c:1107`):
     - Poll `PHYID (0x01)` until it equals `0x0283BCA1` (up to 25 000 tries).
     - Poll `STATUS0 (0x08)` until `RESETC (bit 6)` is set, then write 1 to clear it.
   - `configSync = false`.
   - **`macInit()`** (`adi_mac.c:581`), whose return value `MAC_Reset` ignores:

     | Step | Register | Value / action |
     |---|---|---|
     | a | `IMASK0 (0x0C)` | `0xFFFFE780`: unmask TXPE, TXBOE, TXBUE, RXBOE, LOFE, HDRE, RESETC, TXFCSE, CDPE (PHYINT still masked) |
     | b | `IMASK1 (0x0D)` | `0xFEFFE2FF`: unmask P1_RX_IFG_ERR, SPI_ERR, RX_ECC_ERR, TX_ECC_ERR, P2_TXFCSE. RX_RDY/TX_RDY stay masked because OA mode uses chunk footers instead |
     | c | — | `HAL_RegisterCallback(macCallback)`: saves the INT handler pointer in `bm_adin2111.c:52` |
     | d | `STATUS0`, `STATUS1` | Read (clear-on-read latch flush) |
     | e | `CONFIG0 (0x04)` | Read-modify-write: clear `TXFCSVE` (bit 14), so the host doesn't supply the FCS |
     | f | `CONFIG2 (0x06)` | Read-modify-write: set `CRC_APPEND` (bit 5), so the MAC appends the FCS on TX |
6. `state = ADI_MAC_STATE_READY`.

#### 4.4 Dynamic forwarding table (`adin2111.c:128-137`)

- Register `cbRx` for `ADI_MAC_EVT_DYN_TBL_UPDATE`. On RX it learns the source MAC → port mapping in a 16-entry software table.
- Clear the table (`age = 0`). Set `portEnabled[0..1] = false`.

#### 4.5 Switch forwarding policy — `CONFIG2` (`adin2111.c:150-164`)

Read-modify-write of `CONFIG2`:

| Bit | Field | Value | Meaning |
|---|---|---|---|
| 2 | `P1_FWD_UNK2HOST` | **1** | Port-1 frames with an unknown DA go to the SPI host |
| 12 | `P2_FWD_UNK2HOST` | **1** | Port-2 frames with an unknown DA go to the SPI host |
| 11 | `PORT_CUT_THRU_EN` | **0** | Port-to-port cut-through disabled (reset value is 1) |
| 13/14 | `P1_FWD_UNK2P2` / `P2_FWD_UNK2P1` | left at reset (0). The ADI defaults are commented out | No hardware port-to-port forwarding |

So the ADIN2111 does **not** bridge the two ports in hardware. Every frame goes to the MCU, and Bristlemouth L2 (`l2.c`, `l2_policy`) decides in software whether to forward it out the other port.

Expected `CONFIG2` after init: `0x00001024` (CRC_APPEND | P1_FWD_UNK2HOST | P2_FWD_UNK2HOST), assuming no other non-reset bits.

#### 4.6 Wait for PHY 2 (`adin2111.c:169-180`)

PHY 2 comes out of reset after PHY 1. Poll PHY 2's `CRSM_IRQ_MASK (0x1E0020)` over MDIO until it reads non-zero, up to 200 tries. On timeout the result is `ADI_ETH_COMM_ERROR`.

#### 4.7 PHY init ×2 — `PHY_Init()` → `phyInit()` (`adi_phy.c:87`, `:192`)

This runs for PHY 1 and then PHY 2. All accesses go through the SPI→MDIO bridge (see §4).

| Step | Register (MMD.addr) | Action |
|---|---|---|
| 1 | — | `HAL_DisableIrq()`. No PHY IRQ callback, because `enableIrq = false` |
| 2 | `MMD1_DEV_ID1` / `MMD1_DEV_ID2` | Must read `0x0283`, and the OUI field must be `0x2F`. Extract model and revision. Otherwise `ADI_ETH_UNSUPPORTED_DEVICE` |
| 3 | `CRSM_SFT_PD_CNTRL (0x1E8812)` | Write `0x0001` to enter **software power-down** |
| 4 | `CRSM_STAT` | Poll `CRSM_SFT_PD_RDY` up to 560× (the timeout isn't checked, see §7) |
| 5 | `CRSM_IRQ_MASK (0x1E0020)` | `0x3BFF` = HW-error bits (`0x2BFF`) + `CRSM_HRD_RST_IRQ_EN` (`0x1000`) |
| 6 | `PHY_SUBSYS_IRQ_MASK (0x1F0021)` | `0x0000` for now (no `cbEvents`) |
| 7 | `CRSM_IRQ_STATUS (0x1E0010)` | Read to clear. If any HW-error bit is set, the result is `ADI_ETH_HW_ERROR` |
| 8 | `PHY_SUBSYS_IRQ_STATUS (0x1F0011)` | Read to clear |
| 9 | `phyStaticConfig()` | **Only if silicon rev == 0**: about 20 ADI-specified errata writes (`0x1E8C81`, `0x1E8C80`, `0x1E881F`, `0x018154`, `0x1E8C40`, `0x018008`/`9`, `0x018167`/`68`/`6B`, `0x0181BD`/`BE`/`C2`/`DB`, …). Not applied on later revisions |
| 10 | `AN_CONTROL (0x070200)` | `AN_EN = 1` (auto-negotiation enabled; `AN_RESTART` = 0) |
| 11 | — | PHY state = `SOFTWARE_POWERDOWN` |

#### 4.8 Enable PHY interrupts at the MAC level (`adin2111.c:197-214`)

| Register | Change | Final value |
|---|---|---|
| `IMASK0` | clear `PHYINTM` (bit 7) | `0xFFFFE700` |
| `IMASK1` | clear `P2_PHYINT_MASK` (bit 19) | `0xFEF7E2FF` |

The driver shadow `phyIrqMask` is set to `0x3BFF`.

#### 4.9 Timestamp pins — `configureTsPins()` (`adin2111.c:1972`)

Read-modify-write of PHY 1 `DIGIO_PINMUX (0x1E8C56)` with `TS_TIMER` and `TS_CAPT` set to *not assigned*. The return value is ignored by `adin2111_Init`.

#### 4.10 Link-change interrupt — `adin2111_RegisterCallback(..., ADI_MAC_EVT_LINK_CHANGE)` (`adin2111.c:1386`)

- PHY 1 and PHY 2: read-modify-write `PHY_SUBSYS_IRQ_MASK |= LINK_STAT_CHNG_IRQ_EN (0x0002)`.
- `phyIrqMask |= 0x0002 << 16`.
- `MAC_RegisterCallback` stores `link_change_callback_`. On ADIN2111 no extra IMASK change is needed, because link change arrives through PHYINT.

#### 4.11 Prime the RX queue

All 8 RX descriptors are submitted with `adin2111_SubmitRxBuffer()`. This is pure driver bookkeeping with no SPI traffic. The OA RX path fills these buffers as frames arrive.

#### 4.12 `adin2111_SyncConfig()` → `MAC_SyncConfig()` (`adi_mac.c:2412`)

- Read-modify-write `CONFIG0 |= SYNC (bit 15)`. Until SYNC is set the MAC-PHY won't TX or RX frames. Expected `CONFIG0 = 0x00008006` (CPS = 6 → 64-byte chunks, SYNC).
- **`HAL_EnableIrq()` → `NVIC_EnableIRQ(EXTI8_IRQn)`**. ADIN interrupts now reach the MCU.
- `configSync = true`.

#### 4.13 Bring the ports up — `adin2111_EnablePort(1)`, then `(2)` (`adin2111.c:316`)

`PHY_ExitSoftwarePowerdown()` → `setSoftwarePowerdown(false)` (`adi_phy.c:726`):

- Write `CRSM_SFT_PD_CNTRL = 0x0000`.
- Poll `CRSM_STAT.CRSM_SFT_PD_RDY` until it clears (≤ 10 reads, otherwise `ADI_ETH_READ_STATUS_TIMEOUT`).
- PHY state = `OPERATION`, and `portEnabled[port] = true`.

Port 1 must come up before port 2. The PHYs then start **10BASE-T1L auto-negotiation**, including leader/follower resolution.

### Phase 5 — `bm_l2_init()` → **pass #2** (`src/lib/bm_core/network/l2.c:557`)

```c
network_device.callbacks->receive     = bm_l2_rx;
network_device.callbacks->link_change = link_change;
CTX.evt_queue = bm_queue_create(32, ...);
bm_task_create(bm_l2_thread, "L2", 2048, NULL, bm_l2_tx_task_priority /*7*/, ...);
bm_err_check(err, CTX.network_device.trait->enable(CTX.network_device.self));   // l2.c:571
```

`trait->enable` is `adin2111_netdevice_enable_()`, the **same function as Phase 4.1**. So the whole sequence from §4.2 to §4.13 runs a second time:

- `bcl_power_callback(true)` pulses `ADIN_RST` again (1 ms low, 100 ms wait), which is a full hardware reset of the chip.
- `MAC_Init` re-zeroes driver memory, then: software reset, `waitDeviceReady`, `macInit`, CONFIG2, PHY 1/2 init, IMASK, TS pins, link-change IRQ, re-submit the same 8 RX buffers, SyncConfig, and enable ports 1 and 2.

The second pass is the configuration that's actually live at runtime. It's also the first time `callbacks->receive` and `callbacks->link_change` are set. During pass #1 they were `NULL`, so any frames or link events from that window were dropped.

Because the L2 task has priority 7 and the default task has priority 2, `bm_l2_thread` runs right after it's created, before pass #2 starts. It immediately starts a 100 ms auto-reload **renegotiation timer** for ports 1 and 2 (`l2.c:424-430`, `:111`).

After `bm_l2_init`, `bm_ip_init`, `bcmp_init` and the other steps don't touch the ADIN2111 hardware directly.

### Phase 6 — Steady-state runtime behaviour

**Interrupt path**

```
ADIN INT_N falls
 → EXTI8_IRQHandler (Core/Src/stm32u5xx_it.c:98)
 → STM32IOHandleInterrupt(ADIN_INT) → network_device_interrupt()        [ISR context]
 → bm_l2_handle_device_interrupt(): queue L2Irq to FRONT of L2 queue
 → bm_l2_thread: trait->handle_interrupt() → adin2111_handle_interrupt()  [L2 task, prio 7]
 → macCallback → oaIrqHandler: HAL_DisableIrq(); state = IRQ_START; oaStateMachine()
     (reads STATUS0/1, PHY IRQ status via MDIO, services RX/TX chunks, re-enables IRQ)
 → if link_change_callback_ flagged a port: adin2111_GetLinkStatus() → l2 link_change()
```

**Link change** (`l2.c:493`): updates `enabled_ports_mask`. Link up stops that port's renegotiation timer, and link down restarts it. Registered upper-layer callbacks are notified, for example the port monitor and BCMP.

**Renegotiation timer** (every 100 ms per port without a link, `l2.c:87`, `bm_adin2111.c:418`): reads the AN status. If AN is complete but leader/follower resolution is neither leader nor follower (a fault), it re-triggers auto-negotiation.

**Port monitor** (only if `disableUnusedPortsTimeMs` ≠ 0): one-shot timer. When it fires, it disables port 2 if that port never linked. Port 1 can't be disabled alone, because powering down PHY 1 also powers down PHY 2.

---

## 4. How a register access works

Everything above reduces to **MAC register reads and writes** over SPI. PHY registers are reached through the MAC's MDIO bridge.

### 4.1 MAC control transaction (OA TC6 with protection)

`MAC_ReadRegister` and `MAC_WriteRegister` (`adi_mac.c:901`, `:992`):

1. Enter a critical section, note whether EXTI8 was enabled, disable it, and set `pendingCtrl = true`.
2. Spin until `spiState == READY` (≤ 100 000 iterations).
3. Fill `wnr`, `regAddr` and `cnt = 1`, set `state = CONTROL_START`, and call `oaStateMachine()`.
4. **`CONTROL_START`** (`adi_spi_oa.c:116`): `oaCtrlSetup()` (`:1170`) builds a 16-byte frame:

   ```
   [0..3]   Control header  (DNC=0, HDRB=0, WNR, AID=0, MMS, ADDR, LEN=0, P=odd parity), big-endian
   [4..7]   write: data           | read: don't care
   [8..11]  write: ~data (prot.)  | read: don't care
   [12..15] don't care
   ```

   `MMS = 1` if `addr ≥ 0x30` (MAC/frame registers). Otherwise `MMS = 0` (standard OA control/status registers).

   Because the length is 16 bytes and `MIN_SIZE_FOR_DMA` is 16, this goes out as a **DMA transfer** through `HAL_SpiReadWrite(..., useDma = true)`.
5. `HAL_SpiReadWrite` (`adi_hal.c:58`): set CS low, call `spiTxRxNonblocking` (`protected_spi.c:143`), then set CS high. That function takes the SPI3 mutex, starts `HAL_SPI_TransmitReceive_DMA`, and **blocks the calling task** on `ulTaskNotifyTake` until DMA completes, with a 100 ms timeout. After that, `spiCallback()` is called **synchronously**, which runs **`CONTROL_END`**.
6. **`CONTROL_END`**: the RX bytes `[4..7]` must equal the TX header (echoed header). If they don't, `spiErr = 1` and the caller gets `ADI_ETH_SPI_ERROR`. For reads, `oaCtrlCmdReadData` checks that `rx[8..11] == ~rx[12..15]`. If it doesn't match, the result is `ADI_ETH_PROTECTION_ERROR`.
7. Restore the IRQ enable state.

Because the SPI wait blocks on a task notification, **all ADIN init must run in task context**. In this app it runs in `defaultTask`.

### 4.2 PHY register access (MDIO Clause 45 over SPI)

`MAC_PhyRead` and `MAC_PhyWrite` (`adi_mac.c:1169`, `:1208`). The 24-bit address is `DEVTYPE(addr)` (MMD) plus `REGADDR(addr)`:

1. Write `MDIOACC_0 (0x20)`: Clause 45 ADDRESS op, PRTAD = PHY address (1 or 2), DEVAD = MMD, DATA = register address.
2. Write `MDIOACC_1 (0x21)`: READ or WRITE op (with data for a write).
3. Poll `MDIOACC_1.MDIO_TRDONE` (≤ 10 reads).
4. For a read, read `MDIOACC_1` again and take `MDIO_DATA`.

So each PHY register access costs **at least 4 SPI control transactions** of 16 bytes each.

---

## 5. Expected register state after init

| Register | Expected value | Set by |
|---|---|---|
| MAC `CONFIG0` | `0x00008006`: CPS = 6 (64-byte chunks), TXFCSVE = 0, SYNC = 1. PROTE reflects the strap | macInit, SyncConfig |
| MAC `CONFIG2` | `0x00001024`: CRC_APPEND, P1/P2_FWD_UNK2HOST, cut-through off | macInit, adin2111_Init |
| MAC `IMASK0` | `0xFFFFE700` | macInit, adin2111_Init |
| MAC `IMASK1` | `0xFEF7E2FF` | macInit, adin2111_Init |
| PHY 1/2 `CRSM_IRQ_MASK` | `0x3BFF` | phyInit |
| PHY 1/2 `PHY_SUBSYS_IRQ_MASK` | `0x0002` (link-status change) | adin2111_RegisterCallback |
| PHY 1/2 `AN_CONTROL` | `AN_EN = 1` | phyInit |
| PHY 1/2 `CRSM_SFT_PD_CNTRL` | `0` (out of software power-down) | EnablePort |
| PHY 1 `DIGIO_PINMUX` | TS_TIMER/TS_CAPT = N/A | configureTsPins |
| MCU EXTI8 (ADIN_INT) | enabled, falling edge | SyncConfig |

No MAC address filters are programmed (`adin2111_AddAddressFilter` is never called). Promiscuous and port-forward modes aren't changed. Unknown-DA forwarding to the host is what delivers all traffic to the MCU.

To verify on hardware, use the CLI exposed by `debug_adin_raw.cpp` and `bcmp_cli`, or the `adin_*` GPIO entries in `debugGpioPins` (`app_main.cpp:281-284`).

---

## 6. Timing budget (per pass)

| Step | Typical / worst-case |
|---|---|
| RESET low | 0–1 ms (`vTaskDelay(1)` at a 1 kHz tick; can be < 1 ms, still ≫ 10 µs) |
| Post-reset wait | 100 ms fixed (datasheet max is 90 ms) |
| MAC soft reset + `waitDeviceReady` | About 10 ms per the datasheet, bounded by up to 25 000 polls |
| PHY 2 ready poll | ≤ 200 MDIO reads |
| Each PHY init | About 30 MDIO ops on rev > 0, about 50 on rev 0 (≈ 4 SPI transactions each) |
| SyncConfig + port enables | A handful of MDIO and register ops |

Because init runs **twice**, the ADIN portion of boot costs roughly **2 × (≥ 101 ms + SPI/MDIO traffic)**.

---

## 7. Failure handling

| Failure point | Result |
|---|---|
| Any step in `adin2111_netdevice_enable` | `BmENODEV`; `power(false)` drives `ADIN_PWR = 0` |
| Pass #1 fails | `bristlemouth_init` stops: no L2, IP, BCMP or services. `bcl_init` prints `Failed to initialize bristlemouth`. There's no retry |
| Pass #2 fails (in `bm_l2_init`) | Same, but the L2 task and queue already exist |
| SPI transfer error | `HAL_SpiReadWrite` prints `Network SPI Read/Write Failed` |
| RX buffer allocation fails | `BmENOMEM` before any hardware access |

---

## 8. Observations and potential issues

These came up while tracing the code. They aren't confirmed bugs on hardware but are worth a look.

1. **Double initialization.** `adin2111_init()` fully enables the device, then `bm_l2_init()` calls `trait->enable()` again (`l2.c:571`), which does another hardware reset and full re-init. This costs more than 100 ms of boot time and doubles SPI/MDIO traffic. Interrupts or frames from pass #1 are lost, because the L2 queue doesn't exist yet and `bm_l2_handle_device_interrupt` returns `BmENOMEM`. If one pass is enough, either `adin2111_init()` should skip `adin2111_netdevice_enable()` or L2 shouldn't call `enable`.
2. **Possible race between pass #2 and the renegotiation timers.** `bm_l2_thread` (prio 7) starts 100 ms auto-reload timers before `bm_l2_init` calls `enable()`. The FreeRTOS timer task has priority 2, the same as `defaultTask`, so it can run while `defaultTask` sleeps in the 100 ms post-reset delay or during MDIO polling. `adin2111_netdevice_renegotiate` would then do MDIO accesses on a device that's mid-reset or mid-re-init, sharing `DEVICE_STRUCT` and its control buffers, which have no lock above the SPI mutex. Likely outcomes: spurious "Renegotiation failed" logs at best, and corrupted init-time register sequences at worst.
3. **`MAC_Init` uses `hDevice` before it's assigned.** `ADI_HAL_DISABLE_IRQ(hDevice->adinDevice)` runs before `hDevice = *phDevice` (`adi_mac.c:490` vs `:495`). It's harmless only because the macro drops its first argument (`adi_hal.h:93`).
4. **Unchecked timeouts.**
   - `phyInit` doesn't check whether the software-power-down poll timed out (`adi_phy.c:233-241`).
   - `MAC_Reset` ignores `macInit()`'s return value.
   - `adin2111_Init` ignores `configureTsPins()`'s return value.
   - `waitDeviceReady` reuses one `retryCount` across its PHYID and RESETC loops, so a slow PHYID phase shortens the RESETC budget.
5. **`ADIN_CS` is low from `MX_GPIO_Init` until the first power callback.** The chip is unpowered or in reset for that whole window, so this is probably benign. Defaulting it high in the CubeMX GPIO config would be cleaner.
6. **Rev-0 errata writes** (`phyStaticConfig`) only apply when `MMD1_DEV_ID2.REV_NUM == 0`. If boards ever mix silicon revisions, PHY behaviour can differ.
7. **Unused config.** `ADIN_GPIO_TASK_PRIORITY` (15) and `DEFAULT_BOOT_TASK_PRIORITY` (16) in `task_priorities.h` aren't referenced. `main()` creates `defaultTask` with a literal priority 2, and interrupt work runs in the L2 task (prio 7).
8. **Strap dependency.** The firmware is hard-wired to OA SPI with protection. A board-level strap change on `P1_LED_0/SPI_CFG1` or `SDO/SPI_CFG0` makes bring-up fail silently, with only timeouts as a symptom.

---

## 9. Call graph reference

```
main()                                         apps/bm_soft_module/app_main.cpp:165
└─ defaultTask()                               app_main.cpp:338
   ├─ mxInit() → MX_GPIO_Init()                bsp/bm_mote_spi_v1_0/bsp.cpp:113, Core/Src/gpio.c:42
   ├─ bspInit()                                bsp.cpp:61
   │   ├─ spiInit(&spi3) → MX_SPI3_Init()      lib/drivers/protected/protected_spi.c:23, Core/Src/spi.c:138
   │   └─ IOWrite(ADIN_PWR, 1)                 bsp.cpp:72
   └─ bcl_init()                               lib/bm_integration/bristlemouth_client.cpp:54
      ├─ IORegisterCallback(ADIN_INT, network_device_interrupt)
      └─ bristlemouth_init(bcl_power_callback) lib/bm_core/middleware/bristlemouth.c:12
         ├─ adin2111_init()                    lib/bm_core/drivers/adin2111/bm_adin2111.c:758
         │   └─ adin2111_netdevice_enable()    bm_adin2111.c:294          ── PASS #1
         │       ├─ bcl_power_callback(true)   bristlemouth_client.cpp:43
         │       ├─ adin2111_Init()            adin2111.c:79
         │       │   ├─ MAC_Init()             adi_mac.c:478
         │       │   │   └─ MAC_Reset()        adi_mac.c:738
         │       │   │       ├─ waitDeviceReady()  adi_mac.c:1107
         │       │   │       └─ macInit()      adi_mac.c:581
         │       │   ├─ CONFIG2 fwd policy     adin2111.c:150
         │       │   ├─ wait PHY2              adin2111.c:169
         │       │   ├─ PHY_Init(1), PHY_Init(2)   adi_phy.c:87 → phyInit():192
         │       │   ├─ IMASK0/1 PHYINT unmask adin2111.c:197
         │       │   └─ configureTsPins()      adin2111.c:1972
         │       ├─ adin2111_RegisterCallback(LINK_CHANGE)  adin2111.c:1386
         │       ├─ adin2111_SubmitRxBuffer() ×8
         │       ├─ adin2111_SyncConfig() → MAC_SyncConfig()  adi_mac.c:2412
         │       └─ adin2111_EnablePort(1), (2) → PHY_ExitSoftwarePowerdown()  adi_phy.c:800
         └─ bm_l2_init()                       lib/bm_core/network/l2.c:557
             ├─ create L2 task (starts renegotiation timers)
             └─ trait->enable() = adin2111_netdevice_enable()          ── PASS #2 (identical)
```
