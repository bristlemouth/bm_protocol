// TEMPORARY DEBUG for src/apps/adin_init_time_testing - REVERT BEFORE COMMIT.
//
// Mote-side PHY register reporter. See adin_phy_report.h.
//
// The register list, the address encoding and the printed format deliberately
// mirror adin-oot-drivers/phy-tools/adin_phy_dump.cpp on the RPi, so that a
// dump from each end of the link can be diffed line for line. Register
// addresses come from ADI's own ADIN2111_phy_addr_rdef.h rather than being
// re-spelled here.

#include "adin_phy_report.h"

#include <stdio.h>

#include "ADIN2111_phy_addr_rdef.h"
#include "bm_adin2111.h"

namespace {

// Flags mirror the RPi-side table.
enum {
  kPlain = 0,
  kClearsOnRead = 1 << 0, // read has side effects; skipped by the dump
  kAnCore = 1 << 1,       // included in the one-line sample
};

struct Reg {
  uint32_t addr;
  const char *name;
  unsigned flags;
};

// Same set, same order as kAdinRegs in phy-tools/adin_phy_regs.h.
const Reg kRegs[] = {
    {ADDR_PMA_PMD_CNTRL1, "PMA_PMD_CNTRL1", kPlain},
    {ADDR_PMA_PMD_STAT1, "PMA_PMD_STAT1", kPlain},
    {ADDR_PMA_PMD_CNTRL2, "PMA_PMD_CNTRL2", kPlain},
    {ADDR_PMA_PMD_STAT2, "PMA_PMD_STAT2", kPlain},
    {ADDR_PMA_PMD_TX_DIS, "PMA_PMD_TX_DIS", kPlain},
    {ADDR_PMA_PMD_EXT_ABILITY, "PMA_PMD_EXT_ABILITY", kPlain},
    {ADDR_PMA_PMD_BT1_ABILITY, "PMA_PMD_BT1_ABILITY", kPlain},
    {ADDR_PMA_PMD_BT1_CONTROL, "PMA_PMD_BT1_CONTROL", kPlain},
    {ADDR_B10L_PMA_CNTRL, "B10L_PMA_CNTRL", kPlain},
    {ADDR_B10L_PMA_STAT, "B10L_PMA_STAT", kPlain},
    {ADDR_B10L_TEST_MODE_CNTRL, "B10L_TEST_MODE_CNTRL", kPlain},
    {ADDR_CR_STBL_CHK_FOFFS_SAT_THR, "CR_STBL_CHK_FOFFS_SAT_THR", kPlain},
    {ADDR_SLV_FLTR_ECHO_ACQ_CR_KP, "SLV_FLTR_ECHO_ACQ_CR_KP", kPlain},
    {ADDR_B10L_PMA_LINK_STAT, "B10L_PMA_LINK_STAT", kAnCore},
    {ADDR_MSE_VAL, "MSE_VAL", kAnCore},

    {ADDR_PCS_CNTRL1, "PCS_CNTRL1", kPlain},
    {ADDR_PCS_STAT1, "PCS_STAT1", kPlain},
    {ADDR_PCS_STAT2, "PCS_STAT2", kPlain},
    {ADDR_B10L_PCS_CNTRL, "B10L_PCS_CNTRL", kPlain},
    {ADDR_B10L_PCS_STAT, "B10L_PCS_STAT", kPlain},

    {ADDR_AN_CONTROL, "AN_CONTROL", kAnCore},
    {ADDR_AN_STATUS, "AN_STATUS", kAnCore},
    {ADDR_AN_ADV_ABILITY_L, "AN_ADV_ABILITY_L", kPlain},
    {ADDR_AN_ADV_ABILITY_M, "AN_ADV_ABILITY_M", kAnCore},
    {ADDR_AN_ADV_ABILITY_H, "AN_ADV_ABILITY_H", kPlain},
    {ADDR_AN_LP_ADV_ABILITY_L, "AN_LP_ADV_ABILITY_L", kAnCore},
    {ADDR_AN_LP_ADV_ABILITY_M, "AN_LP_ADV_ABILITY_M", kAnCore},
    {ADDR_AN_LP_ADV_ABILITY_H, "AN_LP_ADV_ABILITY_H", kAnCore},
    {ADDR_AN_B10_ADV_ABILITY, "AN_B10_ADV_ABILITY", kAnCore},
    {ADDR_AN_B10_LP_ADV_ABILITY, "AN_B10_LP_ADV_ABILITY", kPlain},
    {ADDR_AN_FRC_MODE_EN, "AN_FRC_MODE_EN", kPlain},
    {ADDR_AN_STATUS_EXTRA, "AN_STATUS_EXTRA", kAnCore},
    {ADDR_AN_PHY_INST_STATUS, "AN_PHY_INST_STATUS", kAnCore},

    {ADDR_MMD1_DEV_ID1, "MMD1_DEV_ID1", kPlain},
    {ADDR_MMD1_DEV_ID2, "MMD1_DEV_ID2", kPlain},
    {ADDR_CRSM_IRQ_STATUS, "CRSM_IRQ_STATUS", kClearsOnRead},
    {ADDR_CRSM_IRQ_MASK, "CRSM_IRQ_MASK", kPlain},
    {ADDR_CRSM_SFT_PD_CNTRL, "CRSM_SFT_PD_CNTRL", kPlain},
    {ADDR_CRSM_STAT, "CRSM_STAT", kPlain},
    {ADDR_MGMT_PRT_PKG, "MGMT_PRT_PKG", kPlain},

    {ADDR_PHY_SUBSYS_IRQ_STATUS, "PHY_SUBSYS_IRQ_STATUS", kClearsOnRead},
    {ADDR_PHY_SUBSYS_IRQ_MASK, "PHY_SUBSYS_IRQ_MASK", kPlain},
    {ADDR_RX_ERR_CNT, "RX_ERR_CNT", kClearsOnRead},
};

constexpr int kRegCount = (int)(sizeof(kRegs) / sizeof(kRegs[0]));

// Registers in the one-line sample, in print order. Kept as an explicit list
// rather than derived from kRegs so the column order is stable and obvious.
const uint32_t kSampleRegs[] = {
    ADDR_B10L_PMA_LINK_STAT, ADDR_AN_STATUS,          ADDR_AN_STATUS_EXTRA,
    ADDR_AN_PHY_INST_STATUS, ADDR_AN_ADV_ABILITY_M,   ADDR_AN_LP_ADV_ABILITY_M,
};
constexpr int kSampleCount = (int)(sizeof(kSampleRegs) / sizeof(kSampleRegs[0]));

uint16_t s_prev[kSampleCount];
bool s_have_prev;

const char *ms_name(uint16_t v) {
  static const char *n[4] = {"notrun", "FAULT", "SLV", "MST"};
  return n[v & 3];
}

const char *tx_name(uint16_t v) {
  static const char *n[4] = {"notrun", "RSVD", "1.0V", "2.4V"};
  return n[v & 3];
}

} // namespace

namespace {

// Must stay in sync with kSweep[] in phy-tools/adin_phy_dump.cpp, or the two
// sweeps will not diff cleanly.
struct SweepRange {
  uint8_t mmd;
  uint16_t first;
  uint16_t last;
};

const SweepRange kSweep[] = {
    {1, 0x0000, 0x0020},  {1, 0x0800, 0x0840},  {1, 0x08F0, 0x0900},
    {1, 0x8000, 0x8030},  {1, 0x8140, 0x8200},  {1, 0x8300, 0x8320},
    {3, 0x0000, 0x0020},  {3, 0x08E0, 0x0900},
    {7, 0x0000, 0x0010},  {7, 0x0200, 0x0220},  {7, 0x8000, 0x8040},
    {30, 0x0000, 0x0030}, {30, 0x8800, 0x8830}, {30, 0x8C00, 0x8C90},
    {31, 0x0000, 0x0030}, {31, 0x8000, 0x8060},
};

constexpr int kSweepCount = (int)(sizeof(kSweep) / sizeof(kSweep[0]));

} // namespace

void adin_phy_report_sweep(uint8_t port_num) {
  printf("sweep # side=mote port=%u begin\n", port_num);
  unsigned n = 0, printed = 0;

  for (int i = 0; i < kSweepCount; i++) {
    const SweepRange &r = kSweep[i];
    for (uint32_t reg = r.first; reg <= r.last; reg++) {
      uint16_t v = 0;
      n++;
      if (adin2111_debug_phy_read(port_num, ((uint32_t)r.mmd << 16) | reg, &v) !=
          BmOK) {
        continue;
      }
      if (v == 0) {
        continue; // matches the RPi sweep's default, keeps the diff readable
      }
      printf("sweep %2u.%04X = 0x%04X\n", (unsigned)r.mmd, (unsigned)reg, v);
      printed++;
    }
  }
  printf("sweep # swept %u registers, %u non-zero\n", n, printed);
}

void adin_phy_report_reset(void) { s_have_prev = false; }

void adin_phy_report_dump(uint8_t port_num, const char *tag) {
  printf("phyreg # side=mote port=%u tag=%s\n", port_num, tag ? tag : "");

  for (int i = 0; i < kRegCount; i++) {
    const Reg &r = kRegs[i];
    if (r.flags & kClearsOnRead) {
      // Skipped for the same reason as on the RPi: reading these steals a
      // latched interrupt/count from the driver that owns the PHY.
      continue;
    }
    uint16_t v = 0;
    BmErr err = adin2111_debug_phy_read(port_num, r.addr, &v);
    if (err != BmOK) {
      printf("phyreg %2u.%04X  %-26s = ERR(%d)\n",
             (unsigned)((r.addr >> 16) & 0x1F), (unsigned)(r.addr & 0xFFFF),
             r.name, (int)err);
      continue;
    }
    printf("phyreg %2u.%04X  %-26s = 0x%04X\n",
           (unsigned)((r.addr >> 16) & 0x1F), (unsigned)(r.addr & 0xFFFF),
           r.name, v);
  }
}

void adin_phy_report_line(uint8_t port_num, uint32_t t_ms) {
  uint16_t v[kSampleCount];

  for (int i = 0; i < kSampleCount; i++) {
    if (adin2111_debug_phy_read(port_num, kSampleRegs[i], &v[i]) != BmOK) {
      return; // rail down / mid-reset: a partial sample is worse than none
    }
  }

  bool changed = !s_have_prev;
  for (int i = 0; i < kSampleCount && !changed; i++) {
    // MSE is not sampled here, so every column is a genuine state change.
    if (v[i] != s_prev[i]) {
      changed = true;
    }
  }
  if (!changed) {
    return;
  }
  for (int i = 0; i < kSampleCount; i++) {
    s_prev[i] = v[i];
  }
  s_have_prev = true;

  const uint16_t pma = v[0], anst = v[1], anx = v[2], inst = v[3];
  const uint16_t adv_m = v[4], lp_m = v[5];

  printf("phy %6lu %04X %04X %04X %04X %04X %04X  %s%s%s%s |%s%s%s%s%s%s | "
         "hcd=%u %s/%s | adv[%s] lp[%s]\n",
         (unsigned long)t_ms, pma, anst, anx, inst, adv_m, lp_m,
         (pma & BITM_B10L_PMA_LINK_STAT_B10L_DSCR_STAT_OK) ? "dscr " : "---- ",
         (pma & BITM_B10L_PMA_LINK_STAT_B10L_LOC_RCVR_STAT_OK) ? "loc " : "--- ",
         (pma & BITM_B10L_PMA_LINK_STAT_B10L_REM_RCVR_STAT_OK) ? "rem " : "--- ",
         (pma & BITM_B10L_PMA_LINK_STAT_B10L_LINK_STAT_OK) ? "LINK" : "----",
         (anst & BITM_AN_STATUS_AN_PAGE_RX) ? " pagerx" : "",
         (anst & BITM_AN_STATUS_AN_COMPLETE) ? " ANCOMPL" : "",
         (anst & BITM_AN_STATUS_AN_LINK_STATUS) ? " anlink" : "",
         (inst & BITM_AN_PHY_INST_STATUS_IS_AN_TX_EN) ? " TXAN" : "",
         (anx & BITM_AN_STATUS_EXTRA_AN_INC_LINK) ? " INC_LINK" : "",
         (anx & BITM_AN_STATUS_EXTRA_AN_LINK_GOOD) ? " LINKGOOD" : "",
         (unsigned)((anx & BITM_AN_STATUS_EXTRA_AN_HCD_TECH) >>
                    BITP_AN_STATUS_EXTRA_AN_HCD_TECH),
         ms_name((anx & BITM_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN) >>
                 BITP_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN),
         tx_name((anx & BITM_AN_STATUS_EXTRA_AN_TX_LVL_RSLTN) >>
                 BITP_AN_STATUS_EXTRA_AN_TX_LVL_RSLTN),
         (adv_m & BITM_AN_ADV_ABILITY_M_AN_ADV_B10L) ? "B10L" : "!B10L",
         (lp_m & BITM_AN_ADV_ABILITY_M_AN_ADV_B10L) ? "B10L" : "!B10L");
}
