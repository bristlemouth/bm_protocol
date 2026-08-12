// TEMPORARY DEBUG for src/apps/adin_init_time_testing - REVERT BEFORE COMMIT.
//
// Mote-side counterpart to adin-oot-drivers/phy-tools/adin_phy_dump. Dumps the
// ADIN2111's integrated PHY registers in the same "MMD.REG NAME = 0xVVVV"
// format the RPi tool emits, so both ends of the 10BASE-T1L link can be
// captured and diffed directly.

#ifndef ADIN_PHY_REPORT_H
#define ADIN_PHY_REPORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
  @brief Print a full decoded PHY register dump for one port.

  @details ~45 SPI register reads plus one printf per register. Far too slow to
           call from the sampling loop - use it once per cycle at a chosen
           moment (e.g. just after link-up, or at a fixed offset into a stalled
           cycle). Lines are prefixed "phyreg " so they are easy to grep out of
           the surrounding harness log.

  @param port_num ADIN2111 port, 1 or 2
  @param tag      short label recorded in the header line, e.g. "linkup"
*/
void adin_phy_report_dump(uint8_t port_num, const char *tag);

/*!
  @brief Print one compact line of autonegotiation state.

  @details Six register reads. Cheap enough to call from the harness's existing
           100 ms polling loop. Format matches the columns of the RPi tool's
           --watch output so the two traces can be lined up.

  @param port_num ADIN2111 port, 1 or 2
  @param t_ms     milliseconds since this power cycle's t0
*/
void adin_phy_report_line(uint8_t port_num, uint32_t t_ms);

/*!
  @brief Sweep every implemented register cluster and print the non-zero ones.

  @details Counterpart to `adin_phy_dump --sweep` on the RPi, printing the same
           "MMD.REG = 0xVVVV" lines (prefixed "sweep ") so the two ends can be
           diffed across the whole register space, not just the documented
           subset. ~960 reads and a few hundred printfs, so it is called once
           per boot rather than per cycle.

           Note this does read the clear-on-read registers, unlike
           adin_phy_report_dump(). Acceptable for a one-shot configuration
           comparison; do not put it in a loop.

  @param port_num ADIN2111 port, 1 or 2
*/
void adin_phy_report_sweep(uint8_t port_num);

/*!
  @brief Reset the change-detection state used by adin_phy_report_line().

  @details Call at the start of each power cycle so the first sample of the
           cycle always prints.
*/
void adin_phy_report_reset(void);

#ifdef __cplusplus
}
#endif

#endif // ADIN_PHY_REPORT_H
