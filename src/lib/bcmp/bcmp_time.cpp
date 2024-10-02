#include "bcmp_time.h"
#include "app_util.h"
#include "bcmp.h"
#include "bm_util.h"
#include "stm32_rtc.h"

extern "C" {
#include "packet.h"
}

/*!
 @brief Sets the system time on a target node.

 This function sends a system time set message to the specified target node.

 @param[in] target_node_id The ID of the target node.
 @param[in] utc_us The UTC time in microseconds to set on the target node.
 @return BmOK if the message was sent successfully, BmEINVAL otherwise.
 */
BmErr bcmp_time_set_time(uint64_t target_node_id, uint64_t utc_us) {
  BmErr err = BmOK;
  uint64_t source_node_id = node_id();
  BcmpSystemTimeSet set_msg;
  set_msg.header.target_node_id = target_node_id;
  set_msg.header.source_node_id = source_node_id;
  set_msg.utc_time_us = utc_us;
  if (bcmp_tx(&multicast_ll_addr, BcmpSystemTimeSetMessage, (uint8_t *)&set_msg,
              sizeof(set_msg)) != BmOK) {
    printf("Failed to send system time response\n");
    err = BmEINVAL;
  }
  return err;
}

/*!
 @brief Gets the system time from a target node.

 This function sends a system time get message to the specified target node.

 @param[in] target_node_id The ID of the target node.
 @return BmErr if the message was sent successfully, BmEINVAL otherwise.
 */
BmErr bcmp_time_get_time(uint64_t target_node_id) {
  BmErr err = BmOK;
  uint64_t source_node_id = node_id();
  BcmpSystemTimeRequest get_msg;
  get_msg.header.target_node_id = target_node_id;
  get_msg.header.source_node_id = source_node_id;
  if (bcmp_tx(&multicast_ll_addr, BcmpSystemTimeRequestMessage, (uint8_t *)&get_msg,
              sizeof(get_msg)) != BmOK) {
    printf("Failed to send system time response\n");
    err = BmEINVAL;
  }
  return err;
}

/*!
 @brief Sends a system time response message.

 This function sends a system time response message to the specified target node.

 @param[in] target_node_id The ID of the target node.
 @param[in] utc_us The UTC time in microseconds to send in the response.
 */
static void bcmp_time_send_response(uint64_t target_node_id, uint64_t utc_us) {
  uint64_t source_node_id = node_id();
  BcmpSystemTimeResponse response;
  response.header.target_node_id = target_node_id;
  response.header.source_node_id = source_node_id;
  response.utc_time_us = utc_us;
  if (bcmp_tx(&multicast_ll_addr, BcmpSystemTimeResponseMessage, (uint8_t *)&response,
              sizeof(response)) != BmOK) {
    printf("Failed to send system time response\n");
  }
}

/*!
  @brief Processes a system time request message.

  This function processes a system time request message and sends a response with the current system time.

  @param[in] msg The system time request message to process.
*/
static void bcmp_time_process_time_request_msg(const BcmpSystemTimeRequest *msg) {
  do {
    RtcTimeAndDate time;
    // TODO: make this an abstraction
    if (bm_rtc_get(&time) != pdPASS) {
      printf("Failed to get time.\n");
      break;
    }
    bcmp_time_send_response(msg->header.source_node_id, bm_rtc_get_micro_seconds(&time));
  } while (0);
}

/*!
  @brief Processes a system time set message.

  This function processes a system time set message and sets the system time to the specified value.

  @param[in] msg The system time set message to process.
*/
static void bcmp_time_process_time_set_msg(const BcmpSystemTimeSet *msg) {
  UtcDateTime datetime;
  // TODO: make this an abstraction
  date_time_from_utc(msg->utc_time_us, &datetime);
  RtcTimeAndDate time = {// TODO: Consolidate the time functions into util.h
                           .year = datetime.year,       .month = datetime.month,
                           .day = datetime.day,         .hour = datetime.hour,
                           .minute = datetime.min,      .second = datetime.sec,
                           .ms = (datetime.usec / 1000)};
  if (bm_rtc_set(&time) == pdPASS) {
    bcmp_time_send_response(msg->header.source_node_id, msg->utc_time_us);
  } else {
    printf("Failed to set time.\n");
  }
}

/*!
    @brief Process all of the system time messages

    This function processes all of the system time messages. It will handle time requests, responses, and setting the time.
    It will also forward messages to the correct node if the target node ID is not the current node.

    @param[in] data The data to process.
    @return BmOK if the message was processed successfully, BmEINVAL otherwise.
*/
static BmErr bcmp_time_process_time_message(BcmpProcessData data) {
  BmErr err = BmEINVAL;
  bool should_forward = false;
  do {
    BcmpMessageType time_msg_type = (BcmpMessageType)data.header->type;
    BcmpSystemTimeHeader *msg_header = (BcmpSystemTimeHeader *)data.payload;
    if (msg_header->target_node_id != node_id() && msg_header->target_node_id != 0) {
      should_forward = true;
      break;
    }
    switch (time_msg_type) {
    case BcmpSystemTimeRequestMessage: {
      if (msg_header->target_node_id != node_id()) {
        break;
      }
      bcmp_time_process_time_request_msg((BcmpSystemTimeRequest *)data.payload);
      err = BmOK;
      break;
    }
    case BcmpSystemTimeResponseMessage: {
      if (msg_header->target_node_id != node_id()) {
        break;
      }
      BcmpSystemTimeResponse *resp = (BcmpSystemTimeResponse *)data.payload;
      UtcDateTime datetime;
      // TODO - make this an abstraction
      date_time_from_utc(resp->utc_time_us, &datetime);
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
    err = bcmp_ll_forward(data.header, data.payload, data.size, data.ingress_port);

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

  bm_err_check(err, packet_add(&time_request, BcmpSystemTimeRequestMessage));
  bm_err_check(err, packet_add(&time_request, BcmpSystemTimeResponseMessage));
  bm_err_check(err, packet_add(&time_request, BcmpSystemTimeSetMessage));
  return err;
}
