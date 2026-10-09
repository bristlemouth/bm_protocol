# BM F+L mote firmware: part 1

This covers part 1 of `mote-bm-f+l_md_claude_prompt.md`: setting up the ADIN2111 on the mote (`bm_soft_module`) to put out a 1 PPS TS_TIMER pulse on P1_LED_0. That pin is wired to MCU pin PA1, so the mote can use the pulse to follow the GPS timebase. Parts 2–4 were out of scope for this session. Nothing has been committed yet.

The prompt's rules were: make the smallest change possible, follow existing patterns, STOP2 may be disabled, ignore spotter apps other than `spotter`, and never read `TS_SEC_CNT` (0x82) or `TS_NS_CNT` (0x83).

## Files changed

- **`src/lib/bm_core/drivers/adin2111/bm_adin2111.c`** (bm_core submodule): added a new compile-time option, `bm_adin2111_ts_timer_enabled`. It is off by default and is set up the same way as the existing `bm_adin2111_ptp_enabled`. The new code is all inside `#if (bm_adin2111_ts_timer_enabled != 0)`, so the bridge and other apps are unchanged.
- **`src/apps/bm_soft_module/CMakeLists.txt`:** added `bm_adin2111_ts_timer_enabled=1` to `APP_DEFINES`, so only the mote app turns the option on.

## Settings

| Name | Value | Meaning |
|---|---|---|
| `TS_TIMER_HI_NS` | 50000000 | 50 ms high time |
| `TS_TIMER_LO_NS` | 950000000 | Low time; HI + LO = 1 s, so 1 pulse per second |
| `TS_TIMER_START_NS` | 10000000 | First rising edge at 10 ms past the second |

All three values are divisible by 16, as the ADIN requires. `TS_TIMER_QE_CORR` is not written, as the prompt asked.

### Units fix

The prompt gave HI = 50000, LO = 999950000 and START = 10000, and the first version of the code used those values. The TS_TIMER registers count in nanoseconds (the driver's `startTimeNs` field and the datasheet both say so), so those values gave a 50 µs pulse at 10 µs past the second instead of the intended 50 ms pulse at 10 ms. The values were changed to the ones in the table above.

## Init sequence on the mote

The ADIN init runs these steps in order:

1. **`DIGIO_PINMUX`:** route TS_TIMER to LED_0. This happens during `adin2111_Init()` through the driver's existing `configureTsPins()`, because `DRIVER_CONFIG.tsTimerPin` is now `ADIN2111_TS_TIMER_MUX_LED_0` when the option is on (it stays `ADIN2111_TS_TIMER_MUX_NA` otherwise).
2. **Start the ADIN's 1588 clock:** call `adin2111_TsEnable(&DEVICE_STRUCT, ADI_MAC_TS_FORMAT_NONE)`. This only runs when `bm_adin2111_ptp_enabled == 0`.
3. **`LED_POLARITY`:** read the register, set `LED0_POLARITY` to `ENUM_LED_POLARITY_LED0_POLARITY_LED_ACTIVE_HI` (0x1) and write it back. Other bits are left alone.
4. **`LED_CNTRL`:** read the register, set `LED0_FUNCTION` to `ENUM_LED_CNTRL_LED0_FUNCTION_TS_TIMER` (23) and write it back. Other bits are left alone.
5. **`TS_TIMER_HI`** (0x85) = 50000000
6. **`TS_TIMER_LO`** (0x86) = 950000000
7. **`TS_TIMER_START`** (0x88) = 10000000. The output starts toggling when the nanoseconds counter reaches this value.

Steps 2–7 run in `adin2111_netdevice_init_` before `adin2111_SyncConfig()`, right after the existing `bm_adin2111_ptp_enabled` block. Any failure sets `err = BmENODEV` and jumps to `end`, the same as the code around it.

## Design decisions and changes made during the session

### Starting the ADIN's clock without packet timestamps

TS_TIMER compares against the ADIN's internal 1588 nanoseconds counter. That counter only runs when the `TS_EN` bit in `TS_CFG` is set. `MAC_TsTimerStart()` checks the same bit and returns `ADI_ETH_TS_COUNTERS_DISABLED` if it is clear.

`adin2111_TsEnable()` sets `TS_EN` and also controls frame timestamping through `CONFIG0`: `FTSE` turns it on and `FTSS` picks 32-bit or 64-bit stamps. With `ADI_MAC_TS_FORMAT_NONE`, it sets `TS_EN` and clears `FTSE`, so the clock runs but packets are not timestamped. Packet timestamping belongs to part 3.

When part 3 turns on `bm_adin2111_ptp_enabled` for the mote, the existing `TsEnable(ADI_MAC_TS_FORMAT_64B_1588)` call takes over and this call is skipped.

### Writing the TS_TIMER registers directly

The registers are written with `adin2111_WriteRegister()` rather than through the driver's `adin2111_TsTimerStart()`. That function builds HI and LO from a period and a `float` duty cycle, so float rounding could leave HI a few nanoseconds off. It also writes `TS_TIMER_DEF` and can write `TS_TIMER_QE_CORR`. Writing the registers directly gives exact values in the order the prompt specified.

### `DIGIO_PINMUX` (added after review)

At first only `LED_CNTRL` was set, because the datasheet's TS_TIMER sequence names only that register and says P1_LED_0 doesn't need muxing. You confirmed that `DIGIO_TSTIMER_PINMUX` must also be set to LED_0 to get the pulse out, so `tsTimerPin` is now set to LED_0. It uses the driver's existing pin config instead of a new register write, which means it gets written during `adin2111_Init()`, before the other steps.

### `LED0_POLARITY` (added after review)

The datasheet says that when P1_LED_0 carries TS_TIMER, the pin's default level depends on `LED0_POLARITY`. Its reset value is autosense, which could invert the pulse depending on the mote's circuit. You asked for it to be fixed at active high (0x1). It is written before `LED_CNTRL`, so the polarity is set before the pin switches to TS_TIMER.

## Verification

- The `soft` preset (`APP=bm_soft_module`, `BSP=bm_mote_spi_v1_0`) builds with no errors and no new warnings from `bm_adin2111.c`. `compile_commands.json` confirms `bm_adin2111_ts_timer_enabled=1` is passed to the build.
- **Not tested on hardware.** On the bench, check P1_LED_0 / PA1 with a scope for:
  - a rising edge once per second
  - the high time
  - the offset from the second boundary
  - an active-high pulse

  Expect a 50 ms pulse starting 10 ms past each ADIN second. That only lines up with GPS time after the ADIN clock is set or servoed, which is part 4.
