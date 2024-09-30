#include "bcmp_time.h"
#include "app_util.h"
#include "bcmp.h"
#include "bm_util.h"
#include "device_info.h"
#include "stm32_rtc.h"

extern "C" {
#include "packet.h"
}

bool bcmp_time_set_time(uint64_t target_node_id, uint64_t utc_us) {
  bool ret = true;
  uint64_t source_node_id = getNodeId();
  BcmpSystemTimeSet set_msg;
  set_msg.header.target_node_id = target_node_id;
  set_msg.header.source_node_id = source_node_id;
  set_msg.utc_time_us = utc_us;
  if (bcmp_tx(&multicast_ll_addr, BcmpSystemTimeSetMessage, (uint8_t *)&set_msg,
              sizeof(set_msg)) != BmOK) {
    printf("Failed to send system time response\n");
    ret = false;
  }
  return ret;
}

bool bcmp_time_get_time(uint64_t target_node_id) {
  bool ret = true;
  uint64_t source_node_id = getNodeId();
  BcmpSystemTimeRequest get_msg;
  get_msg.header.target_node_id = target_node_id;
  get_msg.header.source_node_id = source_node_id;
  if (bcmp_tx(&multicast_ll_addr, BcmpSystemTimeRequestMessage, (uint8_t *)&get_msg,
              sizeof(get_msg)) != BmOK) {
    printf("Failed to send system time response\n");
    ret = false;
  }
  return ret;
}

static void bcmp_time_send_response(uint64_t target_node_id, uint64_t utc_us) {
  uint64_t source_node_id = getNodeId();
  BcmpSystemTimeResponse response;
  response.header.target_node_id = target_node_id;
  response.header.source_node_id = source_node_id;
  response.utc_time_us = utc_us;
  if (bcmp_tx(&multicast_ll_addr, BcmpSystemTimeResponseMessage, (uint8_t *)&response,
              sizeof(response)) != BmOK) {
    printf("Failed to send system time response\n");
  }
}

static void bcmp_time_process_time_request_msg(const BcmpSystemTimeRequest *msg) {
  configASSERT(msg);
  do {
    RTCTimeAndDate_t time;
    if (rtcGet(&time) != pdPASS) {
      printf("Failed to get time.\n");
      break;
    }
    bcmp_time_send_response(msg->header.source_node_id, rtcGetMicroSeconds(&time));
  } while (0);
}

static void bcmp_time_process_time_set_msg(const BcmpSystemTimeSet *msg) {
  configASSERT(msg);
  utcDateTime_t datetime;
  dateTimeFromUtc(msg->utc_time_us, &datetime);
  RTCTimeAndDate_t time = {// TODO: Consolidate the time functions into util.h
                           .year = datetime.year,       .month = datetime.month,
                           .day = datetime.day,         .hour = datetime.hour,
                           .minute = datetime.min,      .second = datetime.sec,
                           .ms = (datetime.usec / 1000)};
  if (rtcSet(&time) == pdPASS) {
    bcmp_time_send_response(msg->header.source_node_id, msg->utc_time_us);
  } else {
    printf("Failed to set time.\n");
  }
}

/*!
    \return true if the caller should forward the message, false if the message was handled
*/
BmErr bcmp_time_process_time_message(BcmpProcessData data) {
  BmErr err = BmEINVAL;
  bool should_forward = false;
  do {
    BcmpMessageType time_msg_type = (BcmpMessageType)data.header->type;
    BcmpSystemTimeHeader *msg_header = (BcmpSystemTimeHeader *)data.payload;
    if (msg_header->target_node_id != getNodeId() && msg_header->target_node_id != 0) {
      should_forward = true;
      break;
    }
    switch (time_msg_type) {
    case BcmpSystemTimeRequestMessage: {
      if (msg_header->target_node_id != getNodeId()) {
        break;
      }
      bcmp_time_process_time_request_msg((BcmpSystemTimeRequest *)data.payload);
      err = BmOK;
      break;
    }
    case BcmpSystemTimeResponseMessage: {
      if (msg_header->target_node_id != getNodeId()) {
        break;
      }
      BcmpSystemTimeResponse *resp = (BcmpSystemTimeResponse *)data.payload;
      utcDateTime_t datetime;
      // TODO - make this an abstraction
      dateTimeFromUtc(resp->utc_time_us, &datetime);
      printf("Response time node ID: %016" PRIx64 " to %d/%d/%d %02d:%02d:%02d.%03" PRIu32 "\n",
             resp->header.source_node_id, datetime.year, datetime.month, datetime.day,
             datetime.hour, datetime.min, datetime.sec, datetime.usec);
      err = BmOK;
      break;
    }
    case BcmpSystemTimeSetMessage: {
      bcmp_time_process_time_set_msg((BcmpSystemTimeSet *)data.payload);
      err = BmOK;
      break;
    }
    default:
      printf("Invalid system time msg\n");
      break;
    }
  } while (0);

  if (should_forward) {
    bcmp_ll_forward(data.header, data.payload, data.size, data.ingress_port);
    err = BmOK;
  }

  return err;
}

BmErr time_init(void) {
  BmErr err = BmOK;
  BcmpPacketCfg time_request = {
      false,
      false,
      bcmp_time_process_time_message,
  };

  // TODO - handle the errors better
  err = packet_add(&time_request, BcmpSystemTimeRequestMessage);
  if (err != BmOK) {
    return err;
  }
  err = packet_add(&time_request, BcmpSystemTimeResponseMessage);
  if (err != BmOK) {
    return err;
  }
  err = packet_add(&time_request, BcmpSystemTimeSetMessage);
  return err;
}
