# UART SBC Transport — Design Plan

## Goal

Enable transparent Bristlemouth communication over PLUART between a dev kit
mote and a Linux SBC (Raspberry Pi). PLUART becomes a third Bristlemouth
network port alongside the two ADIN2111 PHY ports. BCMP neighbor discovery,
heartbeats, pub-sub, and multicast forwarding work automatically.

---

## Why a new app, not modifying `serial_bridge`

The existing `serial_bridge` (in `bm_devkit/`) uses `bm_serial` — a
higher-level application protocol that handles pub/sub commands, DFU, config,
etc. over COBS-framed messages. What we need is fundamentally different:
**raw L2 Ethernet frame transport**. The new app lives in `src/apps/` (not
`bm_devkit/`) since it's a highly customized app with its own `app_main.cpp`,
not a community-facing dev kit example.

---

## Wire Format (matching bm_sbc)

Must be identical to `bm_sbc/src/transports/uart_l2/`:

```
[COBS-encoded payload] [0x00 delimiter]
```

Payload (before COBS encoding):
```
[len_hi] [len_lo] [L2 frame ...] [CRC-32C, 4 bytes, big-endian]
```

- Length: 2-byte big-endian, equals the L2 frame size.
- CRC-32C (Castagnoli, polynomial 0x82F63B78 reflected): computed over
  length + L2 frame bytes.
- 8N1, no flow control, 115200 baud default.

COBS: Use the existing `src/third_party/cobs-c` library (different API from
bm_sbc's minimal COBS — returns `cobs_encode_result` / `cobs_decode_result`
structs with status codes).

CRC-32C: Port the bm_sbc nibble-table implementation (~24 lines of C) since
the firmware has CRC-32 IEEE in bm_core but not Castagnoli.

---

## Architecture

### Current init chain

```
bcl_init()   [src/lib/bm_integration/bristlemouth_client.cpp]
  → adin2111_init()
  → bristlemouth_init(power_cb)   [bm_core/middleware/bristlemouth.c]
      → adin2111_network_device()  → 2-port NetworkDevice
      → bm_l2_init(adin_device)
      → bm_ip_init()
      → bcmp_init(adin_device)
      → topology_init(2)
      → bm_service_init(), bm_pubsub_init(), bm_middleware_init()
```

### Problem

`bristlemouth_init()` hardcodes `adin2111_network_device()` and passes it
to `bm_l2_init` / `bcmp_init`. We need a 3-port composite device instead.
There's no way to inject a third port after the fact since `bcmp_init()` and
`bm_ip_init()` lack deinit functions.

### Solution: custom app_main.cpp that bypasses bcl_init / bristlemouth_init

The new app has its own `app_main.cpp` (based on `mote_bristlefin/app_main.cpp`
pattern, not the shared `bmdk_common` one). In `defaultTask()`, instead of
calling `bcl_init()`, it:

1. Initializes ADIN as usual (`adin2111_init()`, `adin2111_network_device()`)
2. Initializes PLUART as a UART network device
3. Creates a composite NetworkDevice (3 ports total)
4. Calls `bm_l2_init(composite)`, `bm_ip_init()`, `bcmp_init(composite)`,
   `topology_init(3)`, `bm_service_init()`, `bm_pubsub_init()`,
   `bm_middleware_init()` directly

This avoids modifying bm_core and keeps everything in the app layer.

### Composite NetworkDevice

```
CompositeDevice { adin_device, uart_device }

Port mapping:
  Global port 1 → ADIN port 1
  Global port 2 → ADIN port 2
  Global port 3 → UART port 1

num_ports() → 3

send(self, data, len, port):
  port 0 (flood) → adin.send(0) + uart.send(1)
  port 1-2       → adin.send(port)
  port 3         → uart.send(1)

callbacks->receive(port_num, data, len):
  ADIN calls with port_num 1 or 2 → forwarded as-is to composite callback
  UART calls with port_num 1      → remapped to port_num 3

callbacks->link_change(port_idx, is_up):
  ADIN calls with port_idx 0 or 1 → forwarded as-is
  UART calls with port_idx 0      → remapped to port_idx 2
```

### UART Network Device

Implements `NetworkDeviceTrait` for a single-port UART device:

| Trait function       | Implementation |
|----------------------|----------------|
| `send(port=1)`      | frame_encode (len+CRC+COBS), PLUART::write |
| `enable`             | PLUART::init/enable, start RX task, signal link up |
| `disable`            | PLUART::disable |
| `num_ports`          | 1 |
| `enable_port`        | no-op (always enabled) |
| `disable_port`       | no-op |
| `retry_negotiation`  | NULL |
| `port_stats`         | NULL |
| `handle_interrupt`   | no-op (UART has its own ISR) |

**RX path:** A FreeRTOS task continuously reads bytes from PLUART
byte-stream buffer, accumulates until `0x00` delimiter, calls
`frame_decode()`, then invokes `callbacks->receive(1, l2_frame, len)`.

**Link state:** Report link-up immediately on enable. BCMP heartbeats
handle liveness detection.

---

## File Layout

```
src/apps/native_serial_bridge/
  CMakeLists.txt
  app_main.cpp              # Custom init: composite device → bm_l2_init etc.
  FreeRTOSConfig.h          # Copied from mote_bristlefin or bmdk_common
  memfault_platform_config.h
  task_priorities.h
  sensors/
    sensors.cpp             # Minimal / empty sensor init
  crc32c.h                  # CRC-32C (ported from bm_sbc, with TODO note)
  crc32c.c
  frame_codec.h             # frame_encode / frame_decode (ported, with TODO)
  frame_codec.c
  uart_network_device.h     # UART NetworkDeviceTrait implementation
  uart_network_device.cpp
  composite_device.h        # Wraps ADIN + UART into one NetworkDevice
  composite_device.cpp
```

The app is selected with `-DAPP=native_serial_bridge` (no `CMAKE_APP_TYPE`
needed — the `else()` branch in `src/CMakeLists.txt` resolves
`src/apps/native_serial_bridge/` directly).

---

## Implementation Steps

### Step 1: Port frame codec from bm_sbc

Copy `crc32c.c/h` and `frame_codec.c/h` from bm_sbc. Adapt frame_codec to
use the firmware's `cobs-c` third_party API (`cobs_encode_result` structs).
Add TODO comments noting the duplication with bm_sbc.

### Step 2: UART Network Device

Create `uart_network_device.cpp/h`:
- `NetworkDeviceTrait` vtable with send/enable/disable/num_ports
- TX: frame_encode → PLUART::write
- RX: FreeRTOS task polling PLUART byte stream, accumulating into buffer,
  decoding on 0x00 delimiter, calling callbacks->receive
- Link-up signaled immediately on enable

### Step 3: Composite Network Device

Create `composite_device.cpp/h`:
- Stores references to both ADIN and UART NetworkDevices
- Port remapping logic in send()
- Wrapper callbacks that remap port numbers from sub-devices before
  forwarding to the composite's own callbacks (which L2 sets)

### Step 4: App scaffolding

- Create `app_main.cpp` based on `mote_bristlefin/app_main.cpp` structure
- Replace `bcl_init()` with custom init sequence using composite device
- Create CMakeLists.txt listing all source files
- Copy supporting files (FreeRTOSConfig.h, task_priorities.h, etc.)

### Step 5: Build verification

- Build with `-DAPP=native_serial_bridge`
- Fix any compilation issues

---

## Risks / Notes

- **PLUART buffer sizes:** TX stream buffer is 128 bytes, but
  `PLUART::write()` streams through. Should handle ~1522-byte COBS frames.
- **Frame loss is acceptable** — no flow control, CRC catches corruption,
  BCMP heartbeats handle liveness. UDP-like by design.
- **Topology display will break** — CLI expects 2-port daisy chain. Deferred.
- **bm_serial not used:** PLUART is exclusively for L2 frame transport.
