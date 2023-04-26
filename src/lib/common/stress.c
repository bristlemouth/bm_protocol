#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

// FreeRTOS+CLI includes.
#include "FreeRTOS_CLI.h"

#include <string.h>
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "safe_udp.h"
#include "task_priorities.h"
#include "bm_util.h"
#include "eth_adin2111.h"
#include "bm_l2.h"

#include "stress.h"

// Print out stats every 2 seconds
#define STATS_TIMER_S (2)

// Send out a message every 1ms
// NOTE: we actually send 2 messages every 2ms to half the interrupts
#define TX_TIMER_MS (1)

#define STRESS_QUEUE_LEN 32

typedef struct {
    struct netif* netif;
    struct udp_pcb* pcb;
    uint16_t port;
    TaskHandle_t task;
    xQueueHandle evt_queue;
    TimerHandle_t tx_timer;
    TimerHandle_t stats_timer;
    uint16_t tx_len;
    uint32_t tx_count;
    uint32_t rx_count;
    uint64_t total_bytes_received;
    uint64_t last_bytes_received;
    uint32_t last_bytes_ticks;
} stress_test_ctx_t;

typedef enum {
  STRESS_EVT_RX,
  STRESS_EVT_TX,
  // Print out stress stats
  STRESS_EVT_STATS,
} stress_evt_type_e;

typedef struct {
  stress_evt_type_e type;
  struct pbuf *pbuf;
} stress_test_queue_item_t;

static stress_test_ctx_t _ctx;
static void stress_test_task( void *parameters );
static void stress_test_rx_cb(void *arg, struct udp_pcb *upcb, struct pbuf *buf, const ip_addr_t *addr, u16_t port);

static BaseType_t cmd_stress_fn( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmd_stress = {
  // Command string
  "stress",
  // Help string
  "stress start <speed in Mbps (1-8)>\n"
  "stress stop\n",
  // Command function
  cmd_stress_fn,
  // Number of parameters
  -1
};


static BaseType_t cmd_stress_fn(char *writeBuffer,
                            size_t writeBufferLen,
                            const char *commandString) {
  (void) writeBuffer;
  (void) writeBufferLen;
  (void) commandString;

  do {
    const char *command;
    BaseType_t command_str_len;
    command = FreeRTOS_CLIGetParameter(
                    commandString,
                    1,
                    &command_str_len);

    if(strncmp("start", command, command_str_len) == 0) {
      uint32_t mbps = 1; // default to 1 Mbps
      BaseType_t mbps_str_len;
      const char *mbps_str = FreeRTOS_CLIGetParameter(
                      commandString,
                      2,
                      &mbps_str_len);
      if(mbps_str_len) {
        mbps = strtoul(mbps_str, NULL, 10);
        if((mbps > 8) || (mbps == 0)) {
          printf("Invalid speed! Defaulting to 1Mbps\n");
          mbps = 1;
        }
      }

      // NOTE: 8Mbps will only work when a single device is connected, not with two
      stress_start_tx((mbps * 1024)/8);
    } else if(strncmp("stop", command, command_str_len) == 0) {
      stress_stop_tx();
    } else {
      printf("Invalid arguments\n");
    }
  } while(0);


  return pdFALSE;
}

/*!
  Process received CLI commands from iridium. Split them up, if multiple,
  and send to CLI processor.
  \param[in] *netif - lwip network interface
  \param[in] port - UDP port to use for stress test
  \return None
*/
void stress_test_init(struct netif* netif, uint16_t port) {
  configASSERT(netif);
  configASSERT(port != 0);

  BaseType_t rval;

  _ctx.netif = netif;
  _ctx.port = port;

  _ctx.evt_queue = xQueueCreate(STRESS_QUEUE_LEN, sizeof(stress_test_queue_item_t));
  configASSERT(_ctx.evt_queue);

  FreeRTOS_CLIRegisterCommand( &cmd_stress );

  rval = xTaskCreate(
              stress_test_task,
              "stress",
              // TODO - verify stack size
              configMINIMAL_STACK_SIZE * 4,
              NULL,
              STRESS_TASK_PRIORITY,
              &_ctx.task);

  configASSERT(rval == pdTRUE);
}

/*!
  Stress test transmit function
  \param[in] *pbuf - data to send over UDP
  \return 0 if OK nonzero otherwise (see udp_send for error codes)
*/
int32_t stress_test_tx(struct pbuf *pbuf) {
  return safe_udp_sendto_if(_ctx.pcb, pbuf, &multicast_global_addr, _ctx.port, _ctx.netif);
}

/*!
  Stress test UDP rx callback
  \param[in] *arg - unused
  \param[in] *pcb - UDP PCB for middleware
  \param[in] *buf - data pbuf
  \param[in] *addr - Source address
  \param[in] port -
  \return None
*/
// cppcheck-suppress constParameter
static void stress_test_rx_cb(void *arg, struct udp_pcb *upcb, struct pbuf *buf,
                 const ip_addr_t *addr, u16_t port) {

  (void)arg;
  (void)port;
  (void)addr;

  // sanity check that we're processing middleware data
  configASSERT(upcb == _ctx.pcb);

  do {
    stress_test_queue_item_t item;

    if (buf) {
      item.pbuf = buf;
      item.type = STRESS_EVT_RX;

      pbuf_ref(item.pbuf);
      if(xQueueSend(_ctx.evt_queue, &item, 0) != pdTRUE) {
        printf("Error sending to Queue\n");
        // buf will be freed below
        break;
      }

      // Clear buf so we don't free it below
      buf = NULL;
    } else {
      printf("Error. Message has no payload buf\n");
    }
  } while(0);

  // Free buf if we haven't used it (and cleared it)
  if(buf) {
    pbuf_free(buf);
  }
}

// Send out a "stress test" buffer
static void stress_tx() {
  struct pbuf *pbuf = pbuf_alloc(PBUF_TRANSPORT, _ctx.tx_len, PBUF_RAM);
  configASSERT(pbuf);
  uint32_t *count = (uint32_t *)pbuf->payload;
  memset(pbuf->payload, 0, _ctx.tx_len);

  // Send the count as the first 4 bytes
  *count = _ctx.tx_count++;

  if (stress_test_tx(pbuf)) {
    printf("Error transmitting :(\n");
  }
  pbuf_free(pbuf);
}

// Callback function to print adin port stats
void _print_adin_port_stats(adin2111_DeviceHandle_t device_handle, adin2111_Port_e port, adin_port_stats_t *stats, void* args) {
  configASSERT(device_handle);
  configASSERT(stats);

  // Get the overall system port as an argument, since port is just the adin port
  uint32_t sys_port = (uint32_t)args;
  (void)port;
  printf("PORT %"PRIu32"\n", sys_port);
  printf("MSE_VAL:            %u\n", stats->mse_link_quality.mseVal);
  printf("Link Quality:       ");
  switch(stats->mse_link_quality.linkQuality) {
    case ADI_PHY_LINK_QUALITY_POOR:
      printf("POOR\n");
      break;
    case ADI_PHY_LINK_QUALITY_MARGINAL:
      printf("MARGINAL\n");
      break;
    case ADI_PHY_LINK_QUALITY_GOOD:
      printf("GOOD\n");
      break;
    default:
      printf("?\n");
  }
  printf("SQI:                %u\n", stats->mse_link_quality.sqi);

  printf("RX ERR CNT:         %u\n", stats->frame_check_rx_err_cnt);

  printf("LEN_ERR_CNT:        %u\n", stats->frame_check_err_counters.LEN_ERR_CNT);
  printf("ALGN_ERR_CNT:       %u\n", stats->frame_check_err_counters.ALGN_ERR_CNT);
  printf("SYMB_ERR_CNT:       %u\n", stats->frame_check_err_counters.SYMB_ERR_CNT);
  printf("OSZ_CNT:            %u\n", stats->frame_check_err_counters.OSZ_CNT);
  printf("USZ_CNT:            %u\n", stats->frame_check_err_counters.USZ_CNT);
  printf("ODD_CNT:            %u\n", stats->frame_check_err_counters.ODD_CNT);
  printf("ODD_PRE_CNT:        %u\n", stats->frame_check_err_counters.ODD_PRE_CNT);
  printf("FALSE_CARRIER_CNT:  %u\n", stats->frame_check_err_counters.FALSE_CARRIER_CNT);
}

// Print out the stress test statistics
// NOTE: adin stats printed on callback functions
void stress_print_stats() {
  float Mbps = ((_ctx.total_bytes_received - _ctx.last_bytes_received)/(xTaskGetTickCount() - _ctx.last_bytes_ticks)) * 8.0/ 1024.0;
  printf("\nBytes Received:    %"PRIu64"\n", _ctx.total_bytes_received);
  printf("Average Rx Speed:  %0.3f Mbps\n", Mbps);
  _ctx.last_bytes_received = _ctx.total_bytes_received;
  _ctx.last_bytes_ticks = xTaskGetTickCount();

  for(uint32_t port = 0; port < bm_l2_get_num_ports(); port++) {
    adin2111_DeviceHandle_t adin_handle;
    bm_netdev_type_t dev_type = BM_NETDEV_TYPE_NONE;
    uint32_t start_port_idx;
    if(!bm_l2_get_device_handle(port, (void **)&adin_handle, &dev_type, &start_port_idx)
        || (dev_type != BM_NETDEV_TYPE_ADIN2111)) {

      // Not an ADIN interface, skip it
      continue;
    }

    // Request both ports (if they're enabled)
    // The info will be printed on the callback funcion (if it is called)
    if(bm_l2_get_port_state(start_port_idx)) {
      adin2111_get_port_stats(adin_handle, ADIN2111_PORT_1, _print_adin_port_stats, (void *)(start_port_idx));
    }
    if(bm_l2_get_port_state(start_port_idx + 1)) {
      adin2111_get_port_stats(adin_handle, ADIN2111_PORT_2, _print_adin_port_stats, (void *)(start_port_idx + 1));
    }
  }
}

void stress_handle_rx(struct pbuf *pbuf) {
  configASSERT(pbuf);

  uint32_t *count = (uint32_t *)pbuf->payload;
  if(!xTimerIsTimerActive(_ctx.stats_timer)) {
    printf("Starting stress test!\n");
    _ctx.total_bytes_received = 0;
    _ctx.last_bytes_received = 0;
    _ctx.rx_count = *count;
    _ctx.last_bytes_ticks = xTaskGetTickCount();
    configASSERT(xTimerStart(_ctx.stats_timer, 10));
  } else {
    if (++_ctx.rx_count != *count) {
      printf("Missed packet(s)! Expected #%"PRIu32", received #%"PRIu32"\n", _ctx.rx_count, *count);
      _ctx.rx_count = *count;
    }
  }

  _ctx.total_bytes_received += pbuf->len;
}

/*!
  FreeRTOS timer handler for event scheduling

  \param tmr timer which generated the event
  \return none
*/
static void stress_timer_handler(TimerHandle_t tmr){
  stress_evt_type_e evt_type = (stress_evt_type_e)pvTimerGetTimerID(tmr);

  stress_test_queue_item_t item = {evt_type, NULL};

  configASSERT(xQueueSend(_ctx.evt_queue, &item, 0) == pdTRUE);
}

// Start stress test with buffer size tx_len
// assume buffer of size tx_len is sent every millisecond
void stress_start_tx(uint32_t tx_len) {
  if(!xTimerIsTimerActive(_ctx.tx_timer)) {
    _ctx.tx_len = tx_len;
    configASSERT(xTimerStart(_ctx.tx_timer, 10));
  } else {
    printf("Stress test tx already running\n");
  }
}

// Stop transmitting and stop stats timer printing
void stress_stop_tx() {
  configASSERT(xTimerStop(_ctx.tx_timer, 10));
  configASSERT(xTimerStop(_ctx.stats_timer, 10));
}

/*!
  Stress test task. Will receive stress test packets in queue from
  stress_test_rx_cb and process them. Will also handle tx events, stats
  print events, etc...
  \param[in] *parameters - unused
  \return None
*/
static void stress_test_task( void *parameters ) {
  // Don't warn about unused parameters
  (void) parameters;

  _ctx.pcb = udp_new_ip_type(IPADDR_TYPE_V6);
  udp_bind(_ctx.pcb, IP_ANY_TYPE, _ctx.port);
  udp_recv(_ctx.pcb, stress_test_rx_cb, NULL);

  _ctx.tx_timer = xTimerCreate("tx_timer", pdMS_TO_TICKS(TX_TIMER_MS * 2), pdTRUE, (void *)STRESS_EVT_TX, stress_timer_handler);
  configASSERT(_ctx.tx_timer);

  _ctx.stats_timer = xTimerCreate("stats_timer", pdMS_TO_TICKS(STATS_TIMER_S * 1000), pdTRUE, (void *)STRESS_EVT_STATS, stress_timer_handler);
  configASSERT(_ctx.stats_timer);

  for(;;) {
    stress_test_queue_item_t item;

    BaseType_t rval = xQueueReceive(_ctx.evt_queue, &item, portMAX_DELAY);
    configASSERT(rval == pdTRUE);

    switch(item.type) {
      case STRESS_EVT_RX: {
        stress_handle_rx(item.pbuf);
        break;
      }

      case STRESS_EVT_TX: {
        stress_tx();
        stress_tx();
        break;
      }

      case STRESS_EVT_STATS: {
        stress_print_stats();
        break;
      }
      default: {
        configASSERT(0);
      }
    }

    if(item.pbuf) {
      pbuf_free(item.pbuf);
    }

    //
    // Do stuff with item here
    //    printf("Received %u bytes from %s\n", item.pbuf->len, );

    // Free item
    if(item.pbuf) {
      pbuf_free(item.pbuf);
    }
  }
}
