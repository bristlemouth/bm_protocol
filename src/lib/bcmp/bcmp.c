#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

#include "bcmp.h"
#include "debug.h"

#include "bm_util.h"

#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/raw.h"

typedef struct {
    struct netif* netif;
    struct raw_pcb *_bcmp_pcb;
} bcmpContext_t;


static bcmpContext_t _ctx;

int32_t bmcp_process_packet(struct pbuf *pbuf, const ip_addr_t *src) {
  int32_t rval = 0;
  bcmpHeader_t *header = (bcmpHeader_t *)pbuf->payload;

  switch(header->type) {
    case BCMP_HEARTBEAT: {
      bcmp_process_heartbeat(header->payload, src);
      break;
    }

    default: {
      printf("Unsupported BMCP message %04X\n", header->type);
      break;
    }
  }

  return rval;
}

/* Ping using the raw ip */
static uint8_t bcmp_recv(void *arg, struct raw_pcb *pcb, struct pbuf *pbuf, const ip_addr_t *src) {
  (void)arg;
  (void)pcb;

  // don't eat the packet unless we process it
  uint8_t rval = 0;

  configASSERT(pbuf);

  do {
    if (pbuf->tot_len < (PBUF_IP_HLEN + sizeof(bcmpHeader_t))) {
      break;
    }

    if(pbuf_remove_header(pbuf, PBUF_IP_HLEN) != 0) {
      //  restore original packet
      pbuf_add_header(pbuf, PBUF_IP_HLEN);
      break;
    }

    // bcmpHeader_t *header = (bcmpHeader_t *)pbuf->payload;

    // TODO - Validate checksum

    // // Check for both link-local and unicast address match
    // if(ip6_addr_eq(header->dest, netif_ip_addr6(_ctx.netif, 0)) ||
    //    ip6_addr_eq(header->dest, netif_ip_addr6(_ctx.netif, 1))) {

    // }
    bmcp_process_packet(pbuf, src);

    // eat the packet
    pbuf_free(pbuf);
    rval = 1;

  } while (0);

  return rval;
}

void bcmp_init(struct netif* netif) {
  _ctx.netif = netif;
  _ctx._bcmp_pcb = raw_new(IP_PROTO_BCMP);
  configASSERT(_ctx._bcmp_pcb);

  raw_recv(_ctx._bcmp_pcb, bcmp_recv, NULL);
  configASSERT(raw_bind(_ctx._bcmp_pcb, IP_ADDR_ANY) == ERR_OK);
}

err_t bcmp_tx(const ip_addr_t *dst, bcmpMessaegType_t type, uint8_t *buff, uint16_t len) {
    struct pbuf *pbuf;

    configASSERT((uint32_t)len + sizeof(bcmpHeader_t) < UINT16_MAX);

    pbuf = pbuf_alloc(PBUF_IP, len + sizeof(bcmpHeader_t), PBUF_RAM);
    configASSERT(pbuf);

    bcmpHeader_t *header = (bcmpHeader_t *)pbuf->payload;
    header->type = (uint16_t)type;
    header->checksum = 0;
    header->flags = 0; // Unused for now
    header->rsvd = 0; // Unused for now

    memcpy(header->payload, buff, len);

    const ip_addr_t *src_ip = netif_ip_addr6(_ctx.netif, 0);

    header->checksum = ip6_chksum_pseudo( pbuf,
                                          IP_PROTO_BCMP,
                                          len + sizeof(bcmpHeader_t),
                                          src_ip, dst);

    err_t rval = raw_sendto_if_src( _ctx._bcmp_pcb,
                                    pbuf,
                                    dst,
                                    _ctx.netif,
                                    src_ip); // Using link-local address

    if(rval != ERR_OK) {
      pbuf_free(pbuf);
      printf("Error sending BMCP packet %d\n", rval);
    }

    return rval;
}
