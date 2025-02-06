#include "port_monitor.h"
#include "bm_config.h"
#include "bm_os.h"
#include "configuration.h"
#include "device.h"
#include "l2.h"
#include "spotter.h"
#include <inttypes.h>
#include <string.h>

#define port_monitor_time_ms_key "disableUnusedPortsTimeMs"
#define port_monitor_time_ms_default (0U)
#define port_monitor_log "port_monitor.log"

static uint16_t ENABLED_PORT_MASK = 0;

/*!
  @brief Link change callback to set ports which have come online

  @details Will set a masked value of the ports which have come online.

  @param port_num port that has had a link change
  @param state state of the port
 */
static void port_monitor_link_change(uint8_t port_num, bool state) {
  // If the port is online, update the mask
  if (state) {
    ENABLED_PORT_MASK |= 1 << (port_num - 1);
  }
}

/*!
  @brief Timer callback handler to determine if all ports have come online

  @details Checks if all ports on the node have come online, if a port is
           not online, disable the unused port.
 
  @param timer unused
 */
static void port_monitor_timer_handler(BmTimer timer) {
  (void)timer;
  uint8_t ports = bm_l2_get_port_count();

  for (uint8_t i = 0; i < ports; i++) {
    uint16_t enabled = (1 << i) & ENABLED_PORT_MASK;
    uint8_t port_num = i + 1;
    if (enabled == 0) {
      if (port_num != 1) {
        const char *fmt_disable_str =
            "Disabling network port %d on node: %" PRIx64 ", it never came online\n";
        spotter_log(0, port_monitor_log, USE_TIMESTAMP, fmt_disable_str, port_num);
        spotter_log_console(0, fmt_disable_str, node_id(), port_num);
        bm_l2_netif_enable_disable_port(port_num, false);
      } else {
        // Port 1 cannot be solely disabled, it will also disable port 2 please
        // refer to page 25 of the Rev B datasheet for the ADIN2111
        // https://www.analog.com/media/en/technical-documentation/data-sheets/adin2111.pdf
        const char *fmt_err_str = "Could not disable network port %d on node: %" PRIx64
                                  ", unable to disable due to hardware restrictions\n";
        spotter_log(0, port_monitor_log, USE_TIMESTAMP, fmt_err_str, node_id(), port_num);
        spotter_log_console(0, fmt_err_str, port_num);
      }
    }
  }
}

/*!
  @brief Obtain configuration for port monitor module

  @details If configuration does not exist, set it to the default
           value and save.

  @param ms pointer to variable which will hold the configuration value
            for port monitor
 */
static inline void port_monitor_get_configs(uint32_t *ms) {
  if (!get_config_uint(BM_CFG_PARTITION_SYSTEM, port_monitor_time_ms_key,
                       strlen(port_monitor_time_ms_key), ms)) {
    set_config_uint(BM_CFG_PARTITION_SYSTEM, port_monitor_time_ms_key,
                    strlen(port_monitor_time_ms_key), port_monitor_time_ms_default);
    save_config(BM_CFG_PARTITION_SYSTEM, false);
  }
}

/*!
 @brief Initialize port monitor

 @details The port monitor will turn off unused ports on the device. A
          configuration value is used to turn on the port monitor and
          determine the time necessary before turning off that port.

 @return BmOK on success or if not configured
 @return BmErr on failure
 */
BmErr port_monitor_init(void) {
  BmErr err = BmOK;
  uint32_t ms = port_monitor_time_ms_default;
  BmTimer port_monitor_timer = NULL;

  port_monitor_get_configs(&ms);

  if (ms) {
    bm_l2_register_link_change_callback(port_monitor_link_change);
    port_monitor_timer =
        bm_timer_create("port_monitor_timer", ms, false, NULL, port_monitor_timer_handler);
    err = bm_timer_start(port_monitor_timer, 0);
  }

  return err;
}
