#include "user_code.h"

#include "FreeRTOS.h"
#include "task.h"

#include "bsp.h"
#include "debug.h"
#include "l2.h" // bm_l2_handle_device_interrupt (has its own extern "C")
#include "memfault/core/reboot_tracking.h"
#include "memfault/panics/arch/arm/cortex_m.h" // sMfltRegState layout
#include "memfault/panics/coredump.h" // memfault_platform_fault_handler override
#include "memfault_platform_core.h" // memfault_get_pc / memfault_get_lr
#include "uptime.h"

extern "C" {
#include "adi_hal.h"        // HAL_DisableIrq / HAL_EnableIrq
#include "adin_phy_report.h" // TEMPORARY DEBUG - PHY register dump
#include "bm_adin2111.h"    // adin2111_init, adin2111_network_device
#include "network_device.h" // NetworkDevice, NetworkDeviceCallbacks
}

extern adin_pins_t adin_pins;

/*
  ADIN2111 power-cycle / init-timing harness.

  The bring-up here mirrors the core stack. Two places in core define it:

    - bcl_power_callback() / bcl_init() in
      src/lib/bm_integration/bristlemouth_client.cpp - the cold-boot path.
    - the "adin init" CLI command in src/lib/debug/debug_adin_raw.cpp - core's
      own re-bring-up-after-power-off path, which is exactly what this app
      repeats.

  Both funnel the rail through NetworkDeviceCallbacks::power, which
  adin2111_netdevice_enable() invokes at the top of adin2111_init():

    ADIN_PWR high (ADIN_RST already low) -> ADIN_CS high
      -(ADIN_RESET_HOLD_MS, covers the rail ramp)-> ADIN_RST high
      -(ADIN_SETTLE_MS)-> adin2111_Init -> RegisterCallback ->
      SubmitRxBuffer xN -> SyncConfig -> EnablePort 1,2

  adin_power_callback() below is bcl_power_callback() with ADIN_SETTLE_MS
  substituted for core's fixed AFTER_RESET_DELAY (100 ms) - that delay is the
  knob this harness exists to sweep. Installing it as the device's power
  callback (rather than driving ADIN_PWR by hand and leaving the callback
  NULL) is what keeps this app on the core sequence:

    - the ADIN_RST pulse actually happens. Driving ADIN_PWR alone leaves RST
      high through the power ramp and relies solely on the chip's internal
      POR, which is not how any production bring-up works.
    - ADIN_CS is re-asserted high before the first SPI transaction.
    - the power(false) rollback that adin2111_netdevice_enable() performs on
      its four early-exit paths still cuts the rail, instead of leaving a
      half-initialized PHY powered.

  Two pins are brought out as scope traces:
    ADIN_PWR (PH1) - brackets each cycle.
    PACKET_MARKER  - driven low at the start of every cycle, latched high on
                     the first Ethernet frame received during that cycle.

  The interval between the ADIN_PWR rising edge and the PACKET_MARKER rising
  edge is the measurement this app exists to produce.
*/

// --- Cycle timing -----------------------------------------------------------
// NOTE: the USER task adds bm_delay(10) after every loop() return
// (bmdk_common/app_main.cpp:333), and that idle time lands in the power-OFF
// phase. Set ADIN_OFF_MS to 90 if you need the off window to be exactly 100 ms.
#define ADIN_OFF_MS        (500)   // ADIN_PWR low, ADIN unpowered
// ADIN_PWR enable -> ADIN_RST release. RST is already low (parked in the off
// phase), so this is not a pulse width - it is how long reset stays asserted
// while the rail ramps. It MUST exceed the supply's ramp time: releasing reset
// on an invalid VDD latches the PHY into a state no amount of driver polling
// recovers from, which surfaces as adin2111_init() burning its full 25000-retry
// budget (~800 ms) and returning BmENODEV.
//
// Core uses 1 ms here (RESET_DELAY) and gets away with it because bspInit()
// raises ADIN_PWR long before bcl_init() runs, so its pulse lands on an
// already-powered chip. This harness enables the rail and releases reset in the
// same breath, so it has to cover the ramp itself.
//
// Also note vTaskDelay(pdMS_TO_TICKS(N)) sleeps (N-1, N] ms at the 1 kHz tick
// (configTICK_RATE_HZ), so budget one extra tick beyond the true minimum.
// 10 ms is a conservative start - scope the ADIN supply rail itself (not the
// ADIN_PWR enable GPIO) and trim to ramp + 2 ticks.
#define ADIN_RESET_HOLD_MS (1)
#define ADIN_SETTLE_MS     (22)   // ADIN_RST high -> init. Core: AFTER_RESET_DELAY (100)
#define ADIN_ON_MS         (8000)  // init -> de-init
// TEMPORARY: widened from 2000 to characterize the tail of the link-up time
// distribution - 38% of cycles at 2000ms never saw link-up at all. Revert to
// 2000 (or whatever the data says is the real minimum-safe value) once the
// tail is measured.
// Time to let the L2 task (priority 7) drain any queued interrupt events while
// the PHY is still powered and healthy, after the gate below is closed but
// before the rail is cut. A few ticks is plenty - L2 preempts this task.
#define ADIN_L2_DRAIN_MS   (5)

#define ADIN_LOG_ENABLE    (1)    // printf a per-cycle summary (after power-off)
// Re-print the boot banner for this many cycles so it survives USB CDC
// re-enumeration after a reset. At ~2.6 s/cycle this is ~30 s of visibility.
#define ADIN_BOOT_BANNER_CYCLES (12)
// Stack sampling interval inside the ON window.
#define ADIN_STACK_SAMPLE_MS (100)

// TEMPORARY DEBUG. Dump the PHY's own registers alongside the timing summary,
// in the same format as adin-oot-drivers/phy-tools/adin_phy_dump on the RPi, so
// both ends of the 10BASE-T1L link can be compared register by register.
#define ADIN_PHY_REPORT_ENABLE (1)

// PA1. Physically this is the TCA9546A i2c-mux reset, so holding it low keeps
// the Bristlefin i2c sensor bus in reset for most of each cycle.
#define PACKET_MARKER      I2C_MUX_RESET

static NetworkDevice s_net_dev;
static void (*s_orig_receive)(uint8_t port_num, uint8_t *data, size_t length);
static void (*s_orig_link_change)(uint8_t port_index, bool is_up);
static uint64_t s_driver_start_ms;

// t0 for this cycle (ADIN_PWR rising edge, same instant the scope triggers
// on), and the first link-up / first-packet timestamps relative to it. Reset
// at the top of every loop() so a stale value from a prior cycle can never
// leak into this one's log line. 0 means "did not happen this cycle".
#define LINK_TIME_SLOTS (3) // port_index is small; array beats a bounds bug
static uint64_t s_cycle_t0_ms;
static uint64_t s_link_up_ms[LINK_TIME_SLOTS];
static uint64_t s_packet_ms;

// AN-status polling (link-up stall investigation, see
// linkup-issue-brainstorm.md). First-seen timestamps, same t0-relative basis
// as s_link_up_ms - 0 means "not observed yet this cycle". Port 2 only: that
// is the port the multi-second stalls were measured on.
static uint64_t s_an_complete_ms;
static uint64_t s_an_resolved_ms;
static adi_phy_AnMsResolution_e s_an_last_role;

// True only while the PHY is powered and fully initialized. Gates ADIN_INT so
// no L2 event can be queued for a device that is powered down or mid-init.
static volatile bool s_device_live = false;

/*!
  @brief Gated replacement for bcl_init()'s network_device_interrupt.

  @details Same job - hand the pin interrupt to the L2 task - but drops it
           unless the harness says the PHY is live.

           Without this gate the harness has a genuine data race, and it is the
           most likely source of intermittent BmENODEV (19) from
           adin2111_init(). HAL_DisableIrq() masks the EXTI at the NVIC, but it
           does nothing to the 32-entry L2 event queue (evt_queue_len in l2.c).
           Any L2Irq already queued when the rail is cut is still serviced
           afterwards, and the L2 task runs at priority 7 against this task's
           priority 1 (task_priorities.h:27) - so it preempts the moment we
           block. It then calls trait->handle_interrupt() ->
           adin2111_handle_interrupt() -> the MAC IRQ handler, driving SPI
           against DEVICE_STRUCT either while the rail is down or while
           adin2111_Init() is halfway through configuring it. Garbage register
           reads follow, waitDeviceReady() never matches MAC_PHYID, and it
           burns its full 25000-retry budget (~814 ms) before returning
           ADI_ETH_COMM_TIMEOUT.

           This is why the failures track traffic rather than any delay knob: a
           link partner flooding the interface keeps that queue non-empty.
           Production never trips it because bcl_init() initializes once, at
           boot, before any traffic exists.

  @return true if the event was queued to L2
 */
static bool adin_int_gate(const void *pin_handle, uint8_t value, void *args) {
  (void)pin_handle;
  (void)value;
  (void)args;
  if (!s_device_live) {
    return false;
  }
  return bm_l2_handle_device_interrupt() == BmOK;
}

/*!
  @brief Raise the packet marker, then chain to the real L2 receive handler.

  @details Installed over NetworkDeviceCallbacks::receive, which is invoked from
           exactly one place - receive_callback() in bm_adin2111.c - once per
           received frame. Runs in the L2 task (priority 7), so IOWrite is safe.
*/
static void rx_marker_shim(uint8_t port_num, uint8_t *data, size_t length) {
  IOWrite(&PACKET_MARKER, 1);
  if (s_packet_ms == 0) {
    // First frame of the cycle only - later frames would overwrite the
    // number we actually care about with a less interesting one.
    const uint64_t elapsed = uptimeGetMs() - s_cycle_t0_ms;
    s_packet_ms = (elapsed == 0) ? 1 : elapsed; // 0 is the sentinel, not a time
  }
  if (s_orig_receive) {
    s_orig_receive(port_num, data, length);
  }
}

/*!
  @brief Stamp first link-up time relative to this cycle's t0, then chain.

  @details Installed over NetworkDeviceCallbacks::link_change the same way
           rx_marker_shim is installed over receive - preserves whatever
           bcl_init() wired up (l2's netif/enabled_ports_mask bookkeeping),
           which adin_shutdown() already depends on still running when it
           calls this on the way down.

           Splits the previously-unmeasured "~935 ms of scope-only time"
           (adin-init-hackery.md S6) into link-up-after-driver-ready vs
           first-frame-after-link, instead of only ever seeing their sum.
*/
static void link_change_shim(uint8_t port_index, bool is_up) {
  if (is_up && port_index < LINK_TIME_SLOTS && s_link_up_ms[port_index] == 0) {
    const uint64_t elapsed = uptimeGetMs() - s_cycle_t0_ms;
    s_link_up_ms[port_index] = (elapsed == 0) ? 1 : elapsed;
  }
  if (s_orig_link_change) {
    s_orig_link_change(port_index, is_up);
  }
}

/*!
  @brief Short label for an adi_phy_AnMsResolution_e value, for the log line.
*/
static const char *an_role_str(adi_phy_AnMsResolution_e role) {
  switch (role) {
  case ADI_PHY_AN_MS_RESOLUTION_MASTER:
    return "MST";
  case ADI_PHY_AN_MS_RESOLUTION_SLAVE:
    return "SLV";
  case ADI_PHY_AN_MS_RESOLUTION_FAULT:
    return "FLT";
  default:
    return "NR";
  }
}

/*!
  @brief Poll port 2's AN status once and latch first-seen transition times.

  @details Distinguishes linkup-issue-brainstorm.md's hypothesis 1 (AN itself
           is slow/variable to resolve master/slave) from hypothesis 2 (AN
           resolves promptly but link_change(up) fires late): if
           s_an_resolved_ms lands in the normal ~300-500ms window on a cycle
           where link2= is multi-second, that is hypothesis 2. If
           s_an_resolved_ms itself is late (or 0 for the whole ON window),
           that is hypothesis 1.

           adin2111_debug_get_an_status() reads AN_STATUS, which clears its
           own latched bits on read - harmless here since this is the only
           reader in the ON window and we only care about current state, not
           one-shot latched events.
*/
static void sample_an_status(void) {
  adi_phy_AnStatus_t status;
  if (adin2111_debug_get_an_status(2, &status) != BmOK) {
    return;
  }
  s_an_last_role = status.anMsResolution;

  const uint64_t elapsed = uptimeGetMs() - s_cycle_t0_ms;
  const uint64_t stamp = (elapsed == 0) ? 1 : elapsed;
  if (status.anComplete && s_an_complete_ms == 0) {
    s_an_complete_ms = stamp;
  }
  if ((status.anMsResolution == ADI_PHY_AN_MS_RESOLUTION_MASTER ||
       status.anMsResolution == ADI_PHY_AN_MS_RESOLUTION_SLAVE) &&
      s_an_resolved_ms == 0) {
    s_an_resolved_ms = stamp;
  }
}

/*!
  @brief Power the ADIN2111 rail up or down.

  @details bcl_power_callback() (bristlemouth_client.cpp) on the way up, with
           ADIN_SETTLE_MS in place of AFTER_RESET_DELAY, and reset held
           asserted across the rail ramp rather than for core's fixed 1 ms -
           see ADIN_RESET_HOLD_MS. Called by adin2111_netdevice_enable() /
           adin2111_netdevice_disable(), so the device driver owns the rail
           here exactly as it does in production.

           The down path additionally parks ADIN_RST and ADIN_CS low. Core has
           no need for that - it never powers the ADIN down - but leaving two
           push-pull outputs high into an unpowered die back-feeds it through
           the ESD diodes. Low is also the state gpio.c leaves both pins in at
           cold boot (MX_GPIO_Init resets ADIN_RST_Pin and ADIN_CS_Pin on
           GPIOA), so each cycle starts from the same pin state a real reset
           does.

  @param on true to power up and release reset, false to power down
 */
static void adin_power_callback(bool on) {
  IOWrite(&ADIN_PWR, on);
  if (on) {
    IOWrite(&ADIN_CS, 1);
    // Redundant - the off path already parked it low - but keep it explicit so
    // the invariant "reset is asserted before the rail comes up" is local.
    IOWrite(&ADIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(ADIN_RESET_HOLD_MS));
    IOWrite(&ADIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(ADIN_SETTLE_MS));
    // Stamped after the settle delay so the per-cycle log can separate the
    // fixed reset/settle overhead from the driver's own init time.
    s_driver_start_ms = uptimeGetMs();
  } else {
    IOWrite(&ADIN_RST, 0);
    IOWrite(&ADIN_CS, 0);
  }
}

/*!
  @brief Disable the device, then guarantee both the EXTI and the rail are off.

  @details trait->disable() is adin2111_netdevice_disable(): it disables both
           ports over SPI and only then calls power(false), and only if every
           port disabled cleanly. Two reasons the belt-and-braces calls after
           it are not redundant:

             - a failed port disable skips power(false) entirely, leaving the
               rail up.
             - the vendor driver brackets its register access with
               ADI_HAL_DISABLE_IRQ/ADI_HAL_ENABLE_IRQ (adi_mac.c), so the EXTI
               is generally unmasked again by the time disable() returns.
               Masking has to happen after, not before.
 */
static void adin_shutdown(void) {
  // Close the gate first, then yield long enough for L2 to finish any event it
  // has already queued - while the PHY is still powered, so those SPI
  // transactions complete against a healthy device instead of a dead or
  // half-initialized one. See adin_int_gate().
  s_device_live = false;
  vTaskDelay(pdMS_TO_TICKS(ADIN_L2_DRAIN_MS));

  // Gate L2's renegotiate-check timer off before the link_change(false) calls
  // below (re)start it. That timer polls PHY registers over SPI from Tmr Svc
  // every 100ms with no idea whether the chip is even powered - about to cut
  // ADIN_PWR, so it would otherwise poll a de-energized/still-resetting
  // device for the whole off window, which is exactly what produced the
  // "Renegotiation failed, 0x13" / eHdr=0 spam this session traced (see
  // claude-progress.txt). bm_l2_set_device_live(true) re-enables it once
  // adin2111_init() actually succeeds, below.
  bm_l2_set_device_live(false);

  // Now tell L2 the ports are down, which the gate itself prevents it from
  // learning: cutting the rail produces a link-change interrupt that
  // adin_int_gate() drops, so CTX.enabled_ports_mask would otherwise stay set
  // for the whole power cycle.
  //
  // That mask is L2's TX gate - bm_l2_tx() ands it into the port mask (l2.c:195)
  // and bm_l2_process_tx_evt() only calls send_to_port() for bits still set.
  // Leave it set and the upper layers (BCMP heartbeats, lwIP) keep handing
  // frames to trait->send() while the rail is down and while adin2111_Init()
  // is reconfiguring DEVICE_STRUCT underneath them. That is a second writer
  // into the MAC TX queue during init, and it is how you end up dereferencing
  // a NULL pBufDesc in oaSpiProcess (adi_spi_oa.c:951) -> HardFault (0x9400).
  //
  // The real link-up interrupt sets the mask again once the PHY links, so
  // there is no matching call on the way back up.
  if (s_net_dev.callbacks->link_change) {
    const uint8_t ports = s_net_dev.trait->num_ports();
    for (uint8_t port_idx = 0; port_idx < ports; port_idx++) {
      s_net_dev.callbacks->link_change(port_idx, false);
    }
  }

  (void)s_net_dev.trait->disable(s_net_dev.self);
  HAL_DisableIrq();
  adin_power_callback(false);
}

/*!
  @brief Find the task closest to overflowing its stack.

  @details The Memfault FreeRTOS port owns vApplicationStackOverflowHook
           (memfault_panics_freertos.c:25) and the app's own copy in
           freertos_support.c is commented out, so an overflow records the
           offending task in the coredump and asserts - it never names the task
           on the console. Sampling the margin every cycle shows which task is
           draining, and how fast, before it dies.

           Units are WORDS (4 bytes), matching xTaskCreate's stack depth and
           the `debug tasks` CLI output.

  @param name_out receives the worst task's name
  @return that task's remaining stack headroom in words
 */
// Survives reset. Memfault's vApplicationStackOverflowHook discards pcTaskName
// (MEMFAULT_UNUSED, memfault_panics_freertos.c:25) and its log buffer is plain
// static RAM that is zeroed on reboot, so the name of the task that overflowed
// is otherwise only recoverable from a coredump upload. Latching the worst
// observer here - in the same .noinit RAM that carries the reboot reason -
// makes it readable from the next boot's banner.
//
// usStackHighWaterMark is itself a running minimum: FreeRTOS never revises it
// upward. So sampling once per cycle cannot miss a spike, it can only report it
// late, which is exactly what is wanted here.
#define STACK_WATCH_MAGIC (0x57415444UL) // "WATD" - bumped, layout changed
#define STACK_WATCH_NAME_LEN (16)
#define STACK_WATCH_SLOTS (4)

typedef struct {
  uint32_t magic;
  struct {
    char task[STACK_WATCH_NAME_LEN];
    uint32_t words;
  } slot[STACK_WATCH_SLOTS];
} stack_watch_t;

static stack_watch_t s_stack_watch __attribute__((section(".noinit")));

// TEMPORARY DEBUG - REVERT BEFORE COMMIT. See sample_stacks().
static uint32_t s_l2_free_words = UINT32_MAX;

// TEMPORARY DEBUG - REVERT BEFORE COMMIT. Peak nesting of the OA-SPI state
// machine's synchronous self-re-entry (adi_spi_oa.c) - the stack consumer
// behind the L2 overflow.
extern "C" {
extern volatile uint32_t g_oa_sm_depth_max;
}

// TEMPORARY DEBUG - REVERT BEFORE COMMIT.
// The banner below is re-printed from loop() (not just setup()) so it survives
// USB CDC re-enumeration - but by the time the first loop() runs, sample_stacks()
// has already overwritten s_stack_watch with THIS boot's numbers, so the
// "before reset" line was silently reporting the live values instead. Snapshot
// the carried-over .noinit copy once, at the top of setup(), before anything
// samples, and print from the snapshot.
static stack_watch_t s_stack_watch_prev;

// TEMPORARY DEBUG - REVERT BEFORE COMMIT.
// Memfault's reboot tracking only carries prior_reason + pc + lr, and on the
// crash being chased those read pc=0x00000000 lr=0xA5A5A5A5 - i.e. the stacked
// exception frame itself is garbage (0xA5 is FreeRTOS's tskSTACK_FILL_BYTE), so
// the frame tells us nothing about where we were. The fault status registers do:
// CFSR's STKERR/MSTKERR bits distinguish "could not stack the frame because SP
// was already invalid" (stack overflow / SP corruption) from an ordinary
// bad-pointer bus fault, and MMFAR/BFAR give the address. Latch all of it, plus
// which task was running, into the same .noinit RAM the reason survives in.
#define FAULT_INFO_MAGIC (0x464C5431UL) // "FLT1"

typedef struct {
  uint32_t magic;
  uint32_t reason;
  uint32_t cfsr;  // Configurable Fault Status  (SCB->CFSR,  0xE000ED28)
  uint32_t hfsr;  // HardFault Status           (SCB->HFSR,  0xE000ED2C)
  uint32_t mmfar; // MemManage Fault Address    (SCB->MMFAR, 0xE000ED34)
  uint32_t bfar;  // BusFault Address           (SCB->BFAR,  0xE000ED38)
  uint32_t icsr;  // Interrupt Control/State    (SCB->ICSR,  0xE000ED04) - VECTACTIVE
  uint32_t frame_addr; // where the HW stacked (or tried to stack) the frame
  uint32_t exc_return;
  uint32_t pc;
  uint32_t lr;
  uint32_t xpsr;
  uint32_t task_free_words; // interrupted task's stack headroom, at fault time
  char task[STACK_WATCH_NAME_LEN];
} fault_info_t;

static fault_info_t s_fault_info __attribute__((section(".noinit")));
static fault_info_t s_fault_info_prev; // snapshot, same reason as above

/*!
  @brief Latch fault status registers before Memfault reboots the part.

  @details memfault_platform_fault_handler() is a MEMFAULT_WEAK no-op called as
           the very first statement of memfault_fault_handler()
           (memfault_fault_handling_arm.c:117), before any coredump work, with
           the full register state in hand. Defining it here overrides the weak
           symbol. The u5 port does not define it (only the unused l4/wb ports
           do), so there is no collision.

           Everything here must be safe in exception context: raw SCB reads, a
           pcTaskGetName() that just dereferences pxCurrentTCB, and
           uxTaskGetStackHighWaterMark() which only walks the fill pattern. No
           scheduler calls, no printf, no uxTaskGetSystemState() (it suspends
           the scheduler).

           regs->exception_frame may point at unmapped or garbage memory when
           the fault IS a stacking error - that is the case being chased - so
           the frame fields are recorded for reference only and CFSR is what
           should be believed.
 */
extern "C" void memfault_platform_fault_handler(const sMfltRegState *regs,
                                                eMemfaultRebootReason reason) {
  volatile uint32_t *const scb_icsr = (uint32_t *)0xE000ED04;
  volatile uint32_t *const scb_cfsr = (uint32_t *)0xE000ED28;
  volatile uint32_t *const scb_hfsr = (uint32_t *)0xE000ED2C;
  volatile uint32_t *const scb_mmfar = (uint32_t *)0xE000ED34;
  volatile uint32_t *const scb_bfar = (uint32_t *)0xE000ED38;

  s_fault_info.magic = FAULT_INFO_MAGIC;
  s_fault_info.reason = (uint32_t)reason;
  s_fault_info.cfsr = *scb_cfsr;
  s_fault_info.hfsr = *scb_hfsr;
  s_fault_info.mmfar = *scb_mmfar;
  s_fault_info.bfar = *scb_bfar;
  s_fault_info.icsr = *scb_icsr;

  if (regs != NULL) {
    s_fault_info.frame_addr = (uint32_t)regs->exception_frame;
    s_fault_info.exc_return = regs->exc_return;
    if (regs->exception_frame != NULL) {
      s_fault_info.pc = regs->exception_frame->pc;
      s_fault_info.lr = regs->exception_frame->lr;
      s_fault_info.xpsr = regs->exception_frame->xpsr;
    }
  }

  const char *name = pcTaskGetName(NULL);
  strncpy(s_fault_info.task, name ? name : "?", STACK_WATCH_NAME_LEN - 1);
  s_fault_info.task[STACK_WATCH_NAME_LEN - 1] = '\0';
  s_fault_info.task_free_words = (uint32_t)uxTaskGetStackHighWaterMark(NULL);
}

/*!
  @brief Snapshot the STACK_WATCH_SLOTS tasks with the least stack headroom.

  @details Recording only the single tightest task was useless: IDLE has a
           128-word stack (configMINIMAL_STACK_SIZE) and sits near 98 words
           free forever, so it wins "lowest absolute headroom" every time while
           using barely a quarter of its stack. The task actually heading for an
           overflow was ranked second or third and never recorded. Keeping
           several slots makes the one that is draining visible.

           No min-tracking is needed: usStackHighWaterMark is itself the
           lowest-ever free count for that task and FreeRTOS never revises it
           upward, so the most recent snapshot is already the running minimum.
 */
static void sample_stacks(void) {
  static TaskStatus_t status[24];

  UBaseType_t count =
      uxTaskGetSystemState(status, sizeof(status) / sizeof(status[0]), NULL);
  if (count == 0) {
    return;
  }

  // TEMPORARY DEBUG - REVERT BEFORE COMMIT. L2 is the task that overflows, so
  // track it by name too: once its stack is grown it drops off the "tightest
  // three" list entirely and its margin becomes invisible, which is exactly
  // when the number matters most.
  for (UBaseType_t i = 0; i < count; i++) {
    if (status[i].pcTaskName != NULL && strcmp(status[i].pcTaskName, "L2") == 0) {
      if (status[i].usStackHighWaterMark < s_l2_free_words) {
        s_l2_free_words = status[i].usStackHighWaterMark;
      }
      break;
    }
  }

  s_stack_watch.magic = STACK_WATCH_MAGIC;
  for (uint32_t s = 0; s < STACK_WATCH_SLOTS; s++) {
    uint32_t best = UINT32_MAX;
    UBaseType_t best_i = count;
    for (UBaseType_t i = 0; i < count; i++) {
      if (status[i].usStackHighWaterMark < best) {
        best = status[i].usStackHighWaterMark;
        best_i = i;
      }
    }
    if (best_i == count) {
      s_stack_watch.slot[s].task[0] = '\0';
      s_stack_watch.slot[s].words = 0;
      continue;
    }
    strncpy(s_stack_watch.slot[s].task,
            status[best_i].pcTaskName ? status[best_i].pcTaskName : "?",
            STACK_WATCH_NAME_LEN - 1);
    s_stack_watch.slot[s].task[STACK_WATCH_NAME_LEN - 1] = '\0';
    s_stack_watch.slot[s].words = best;
    // Consume this entry so the next pass finds the following-lowest.
    status[best_i].usStackHighWaterMark = UINT16_MAX;
  }
}

/*!
  @brief Print why the previous boot ended.

  @details Repeated from loop() for the first ADIN_BOOT_BANNER_CYCLES cycles.
           Printing it once from setup() is not enough: on a USB CDC console
           this lands before the port re-enumerates and the terminal
           reattaches, so the one line that matters is exactly the one you
           never see.

           Note "memfault reset" from the `info` CLI command is NOT this. That
           string is RESET_REASON_MEM_FAULT, written by memfault_platform_reboot()
           (memfault_platform_core_u5.c:179) - the SDK's common exit path after
           it has already handled an assert, a watchdog, or a fault. It tells
           you Memfault rebooted the part, not why.
 */
static void print_boot_reason(void) {
  sMfltRebootReason reboot = {};
  if (memfault_reboot_tracking_get_reboot_reason(&reboot) == 0) {
    printf("boot: prior_reason=0x%04X reg_reason=0x%04X crashes=%u "
           "pc=0x%08lX lr=0x%08lX\n",
           (unsigned)reboot.prior_stored_reason,
           (unsigned)reboot.reboot_reg_reason,
           (unsigned)memfault_reboot_tracking_get_crash_count(),
           (unsigned long)memfault_get_pc(),
           (unsigned long)memfault_get_lr());
  } else {
    printf("boot: reboot reason unavailable\n");
  }

  // s_stack_watch_prev, NOT s_stack_watch: the live copy has already been
  // overwritten by this boot's sampling by the time loop() reprints this.
  if (s_stack_watch_prev.magic == STACK_WATCH_MAGIC) {
    printf("boot: tightest stacks before reset =");
    for (uint32_t s = 0; s < STACK_WATCH_SLOTS; s++) {
      if (s_stack_watch_prev.slot[s].task[0]) {
        printf(" %s:%uw", s_stack_watch_prev.slot[s].task,
               (unsigned)s_stack_watch_prev.slot[s].words);
      }
    }
    printf("\n");
  } else {
    printf("boot: no stack watermark carried over (cold boot)\n");
  }

  // TEMPORARY DEBUG - REVERT BEFORE COMMIT. See memfault_platform_fault_handler().
  if (s_fault_info_prev.magic == FAULT_INFO_MAGIC) {
    printf("boot: fault task=%s free=%uw reason=0x%04X\n",
           s_fault_info_prev.task,
           (unsigned)s_fault_info_prev.task_free_words,
           (unsigned)s_fault_info_prev.reason);
    printf("boot: fault cfsr=0x%08lX hfsr=0x%08lX mmfar=0x%08lX bfar=0x%08lX "
           "icsr=0x%08lX\n",
           (unsigned long)s_fault_info_prev.cfsr,
           (unsigned long)s_fault_info_prev.hfsr,
           (unsigned long)s_fault_info_prev.mmfar,
           (unsigned long)s_fault_info_prev.bfar,
           (unsigned long)s_fault_info_prev.icsr);
    printf("boot: fault frame=0x%08lX exc_return=0x%08lX pc=0x%08lX lr=0x%08lX "
           "xpsr=0x%08lX\n",
           (unsigned long)s_fault_info_prev.frame_addr,
           (unsigned long)s_fault_info_prev.exc_return,
           (unsigned long)s_fault_info_prev.pc,
           (unsigned long)s_fault_info_prev.lr,
           (unsigned long)s_fault_info_prev.xpsr);
  } else {
    printf("boot: no fault detail carried over\n");
  }
}

void setup(void) {
  // Why did the previous boot end? "a memfault reset" on its own does not
  // discriminate between the IWDG (fed only from vApplicationIdleHook in
  // watchdog.c, so it fires when any task hogs the CPU for ~5 s), the Memfault
  // LPTIM software watchdog, a task stack overflow, or a CPU fault - and those
  // have completely different fixes.
  //
  // prior_stored_reason is the interesting field. Codes worth recognising:
  //   0x8001 Assert          0x8005 HardwareWatchdog  0x8006 SoftwareWatchdog
  //   0x800A OutOfMemory     0x800B StackOverflow     0x9100 BusFault
  //   0x9200 MemFault        0x9300 UsageFault        0x9400 HardFault
  //   0x9401 Lockup
  // pc/lr point at the offending instruction for fault and assert reasons.
  //
  // Snapshot the two .noinit blocks FIRST: sample_stacks() overwrites
  // s_stack_watch every 100ms from the first cycle onwards, and a later fault
  // would overwrite s_fault_info, so the carried-over values have to be copied
  // out before anything in this boot can touch them. Clearing s_fault_info's
  // magic afterwards keeps a stale fault from being re-reported as if it had
  // just happened on a subsequent clean boot.
  s_stack_watch_prev = s_stack_watch;
  s_fault_info_prev = s_fault_info;
  s_fault_info.magic = 0;

  print_boot_reason();

  IOWrite(&PACKET_MARKER, 0);

  s_net_dev = adin2111_network_device();

  // Chain the per-frame RX callback installed by bm_l2_init (bm_l2_rx).
  // bm_l2_init must never be called again or this shim is overwritten, which
  // is why each cycle re-runs adin2111_init() alone rather than the
  // bm_l2_deinit() + bm_l2_init() pair debug_adin_raw.cpp uses. Nothing is
  // lost by skipping it: everything bcl_init() stood up - the L2 task, its
  // event queue, and the ADIN_INT callback registered via
  // IORegisterCallback(adin_pins.interrupt, ...) - is independent of the
  // device and survives the power cycle. Only the ADIN2111 itself is
  // re-initialized. (HAL_Init_Hook(), which debug_adin_raw.cpp also re-runs,
  // is a no-op in adi_hal.c.)
  s_orig_receive = s_net_dev.callbacks->receive;
  s_net_dev.callbacks->receive = rx_marker_shim;

  // Same chaining approach as receive, for the link-up timestamp.
  s_orig_link_change = s_net_dev.callbacks->link_change;
  s_net_dev.callbacks->link_change = link_change_shim;

  // Swap bcl_power_callback for ours: same sequence, tunable settle time.
  // Safe to hold across cycles - create_network_device() re-points
  // NETWORK_DEVICE.callbacks at a function-static that is zero-initialized
  // once at load, so it never clears what we install here.
  s_net_dev.callbacks->power = adin_power_callback;

  // Take over the ADIN_INT pin callback that bcl_init() installed.
  // STM32IORegisterCallback stores a single callback per pin (stm32_io.c:54),
  // so this replaces network_device_interrupt rather than chaining to it.
  IORegisterCallback(adin_pins.interrupt, adin_int_gate, NULL);

  // Tear down what bcl_init() brought up and park the PHY powered off.
  adin_shutdown();

  printf("adin_init_time_testing: off=%dms reset_hold=%dms settle=%dms on=%dms\n",
         ADIN_OFF_MS, ADIN_RESET_HOLD_MS, ADIN_SETTLE_MS, ADIN_ON_MS);
}

void loop(void) {
  // Keep re-announcing the previous boot's exit reason until a terminal has
  // plausibly had time to attach. See print_boot_reason().
  static uint32_t banner_cycles = 0;
  if (banner_cycles < ADIN_BOOT_BANNER_CYCLES) {
    banner_cycles++;
    print_boot_reason();
  }

  // --- OFF ---
  // setup() and adin_shutdown() both leave the rail down, so this is just a
  // guard against a future early return skipping the shutdown.
  IOWrite(&PACKET_MARKER, 0); // reset the per-cycle marker
  adin_power_callback(false);
  vTaskDelay(pdMS_TO_TICKS(ADIN_OFF_MS));

  // --- POWER ON + INIT ---
  // adin_power_callback(true) runs inside here, from
  // adin2111_netdevice_enable(), so ADIN_PWR rises at t0 and the reset pulse
  // and settle delay are part of the measured interval.
  const uint64_t t0 = uptimeGetMs();
  s_cycle_t0_ms = t0;
  for (uint32_t s = 0; s < LINK_TIME_SLOTS; s++) {
    s_link_up_ms[s] = 0;
  }
  s_packet_ms = 0;
  s_an_complete_ms = 0;
  s_an_resolved_ms = 0;
  s_an_last_role = ADI_PHY_AN_MS_RESOLUTION_NOT_RUN;
#if ADIN_PHY_REPORT_ENABLE
  adin_phy_report_reset();
#endif
  const BmErr err = adin2111_init();
  const uint64_t now = uptimeGetMs();
  const uint64_t total_ms = now - t0;
  const uint64_t driver_ms = now - s_driver_start_ms;

  // adin2111_init() already re-enables the EXTI on the success path, deep in
  // the vendor driver: adin2111_SyncConfig -> MAC_SyncConfig (adi_mac.c:2432),
  // "CONFIG0.SYNC is set, we can now enable the IRQ". We re-enable explicitly
  // anyway - NVIC_EnableIRQ is idempotent, it documents intent at the call
  // site, and it survives that side effect being moved.
  //
  // Gated on success on purpose. adin2111_netdevice_enable() has four early
  // exits before SyncConfig is reached, and unmasking the EXTI for a PHY that
  // failed to initialize would let the L2 task drive a half-configured device.
  if (err == BmOK) {
    // Order matters. HAL_DisableIrq() only masks at the NVIC - the EXTI still
    // latches its pending bit on a falling edge, so an ADIN_INT that arrived
    // during init is delivered the instant NVIC_EnableIRQ runs. If the gate
    // were still closed at that moment we would drop that interrupt AND clear
    // the pending bit, and since EXTI8 is falling-edge only (gpio.c:149) no
    // further edge would arrive while INT stayed asserted - the cycle would
    // silently never receive. Open the gate first.
    s_device_live = true;
    bm_l2_set_device_live(true);
    HAL_EnableIrq();
  }

  // --- RUN: traffic arriving here raises PACKET_MARKER via rx_marker_shim ---
  // Sampled in slices rather than one long delay: the overflow develops under
  // RX load, and one sample per ~2.6 s cycle is too coarse to catch the task
  // that is draining before it dies.
  for (uint32_t elapsed = 0; elapsed < ADIN_ON_MS;
       elapsed += ADIN_STACK_SAMPLE_MS) {
    vTaskDelay(pdMS_TO_TICKS(ADIN_STACK_SAMPLE_MS));
    sample_stacks();
    if (err == BmOK) {
      sample_an_status();
#if ADIN_PHY_REPORT_ENABLE
      // Change-triggered, so a healthy cycle costs a handful of lines and a
      // stalled one shows every state the PHY passes through.
      adin_phy_report_line(2, (uint32_t)(uptimeGetMs() - s_cycle_t0_ms));
#endif
    }
  }

#if ADIN_PHY_REPORT_ENABLE
  // One full dump per cycle, taken at the end of the ON window so it is still
  // before adin_shutdown() cuts the rail. Comparable directly against
  // `sudo phy-tools/adin_phy_dump --bare -i eth2` on the RPi.
  if (err == BmOK) {
    adin_phy_report_dump(2, s_link_up_ms[1] ? "linked" : "STALLED");
    // Whole-register-space sweep once per boot, for diffing against
    // `adin_phy_dump --sweep`. Too much serial traffic to repeat per cycle.
    static bool s_swept = false;
    if (!s_swept) {
      s_swept = true;
      adin_phy_report_sweep(2);
    }
  }
#endif

  // --- DE-INIT + POWER OFF ---
  adin_shutdown();

#if ADIN_LOG_ENABLE
  uint8_t got_packet = 0;
  IORead(&PACKET_MARKER, &got_packet);
  // Free heap is logged per cycle because this harness re-runs init endlessly:
  // anything in the bring-up path that allocates without a matching free shows
  // up here as a monotonic decline long before it becomes a mystery reset.
  printf("adin init: err=%d, pwr_to_ready=%llums (driver=%llums), "
         "link1=%llums link2=%llums packet=%u@%llums, "
         "an_complete=%llums an_resolved=%llums an_role=%s, "
         "heap=%u (min %u), l2=%uw oa_depth=%u, stacks=%s:%uw %s:%uw %s:%uw\n",
         (int)err, (unsigned long long)total_ms, (unsigned long long)driver_ms,
         (unsigned long long)s_link_up_ms[0], (unsigned long long)s_link_up_ms[1],
         got_packet, (unsigned long long)s_packet_ms,
         (unsigned long long)s_an_complete_ms, (unsigned long long)s_an_resolved_ms,
         an_role_str(s_an_last_role),
         (unsigned)xPortGetFreeHeapSize(),
         (unsigned)xPortGetMinimumEverFreeHeapSize(),
         (unsigned)s_l2_free_words, (unsigned)g_oa_sm_depth_max,
         s_stack_watch.slot[0].task, (unsigned)s_stack_watch.slot[0].words,
         s_stack_watch.slot[1].task, (unsigned)s_stack_watch.slot[1].words,
         s_stack_watch.slot[2].task, (unsigned)s_stack_watch.slot[2].words);
#endif
}
