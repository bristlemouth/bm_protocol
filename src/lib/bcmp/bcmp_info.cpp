#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#include "bcmp.h"
extern "C" {
#include "packet.h"
}
#include "bcmp_linked_list_generic.h"
#include "bcmp_neighbors.h"
#include "device_info.h"

static uint64_t _info_expect_node_id;

static BCMP_Linked_List_Generic _info_request_list;

/*!
  Set node id to expect information from. Will print out the information
  when it is received.

  \param node_id node id to expect device information from
  \return none
*/
void bcmp_expect_info_from_node_id(uint64_t node_id) {
  // TODO - add expiration and/or callback?
  _info_expect_node_id = node_id;
}

/*!
  Send a request for device information.

  \param target_node_id - node id of the target we want the information from (0 for all targets)
  \param *addr - ip address to send request to
  \return ERR_OK if successful
*/
err_t bcmp_request_info(uint64_t target_node_id, const ip_addr_t *addr, void (*cb)(void *)) {
  _info_request_list.add(target_node_id, cb);

  bcmp_device_info_request_t info_req = {.target_node_id = target_node_id};

  return bcmp_tx(addr, BcmpDeviceInfoRequestMessage, (uint8_t *)&info_req, sizeof(info_req));
}

#define VER_STR_MAX_LEN (255)

/*!
  Send current device information over the network

  \param dst - ip address to send the information to
  \return ERR_OK if successful
*/
static err_t bcmp_send_info(void *dst) {

  char *ver_str = static_cast<char *>(pvPortMalloc(VER_STR_MAX_LEN));
  configASSERT(ver_str);
  memset(ver_str, 0, VER_STR_MAX_LEN);

  uint8_t ver_str_len =
      snprintf(ver_str, VER_STR_MAX_LEN, "%s@%s", APP_NAME, getFWVersionStr());

  // TODO - use device name instead of UID str
  uint8_t dev_name_len = strlen(getUIDStr());

  uint16_t info_len = sizeof(bcmp_device_info_reply_t) + ver_str_len + dev_name_len;

  uint8_t *dev_info_buff = static_cast<uint8_t *>(pvPortMalloc(info_len));
  configASSERT(info_len);

  memset(dev_info_buff, 0, info_len);

  bcmp_device_info_reply_t *dev_info =
      reinterpret_cast<bcmp_device_info_reply_t *>(dev_info_buff);
  dev_info->info.node_id = getNodeId();

  // TODO - fill these with actual values
  dev_info->info.vendor_id = 0;
  dev_info->info.product_id = 0;
  memset(dev_info->info.serial_num, '0', sizeof(dev_info->info.serial_num));

  dev_info->info.git_sha = getGitSHA();
  getFWVersion(&dev_info->info.ver_major, &dev_info->info.ver_minor, &dev_info->info.ver_rev);

  // TODO - get actual hardware version
  dev_info->info.ver_hw = 0;

  dev_info->ver_str_len = ver_str_len;
  dev_info->dev_name_len = dev_name_len;

  memcpy(&dev_info->strings[0], ver_str, ver_str_len);
  memcpy(&dev_info->strings[ver_str_len], getUIDStr(), dev_name_len);

  err_t rval = bcmp_tx(dst, BcmpDeviceInfoReplyMessage, dev_info_buff, info_len);

  vPortFree(dev_info_buff);
  vPortFree(ver_str);

  return rval;
}

/*!
  Handle device information request (if any or addressed to this node id)

  \param *info_req - info request data
  \param *src - source ip of requester
  \param *dst - destination ip of request (used for responding to the correct multicast address)
  \return ERR_OK if successful
*/
static BmErr bcmp_process_info_request(BcmpProcessData data) {
  BcmpDeviceInfoRequest *request = (BcmpDeviceInfoRequest *)data.payload;

  if ((request->target_node_id == 0) || (getNodeId() == request->target_node_id)) {
    // Send info back on the same address we received it on
    // TODO - add unicast support, this will only work with multicast dst
    bcmp_send_info(data.dst);
  }
  return BmOK;
}

/*!
  Fill out device information in neigbhor structure

  \param *neighbor - neighbor structure to populate
  \param *dev_info - device info to populate with
  \return true if successful, false otherwise
*/
static bool _populate_neighbor_info(bm_neighbor_t *neighbor,
                                    bcmp_device_info_reply_t *dev_info) {
  bool rval = false;
  if (neighbor) {
    memcpy(&neighbor->info, &dev_info->info, sizeof(bcmp_device_info_t));

    neighbor->node_id = dev_info->info.node_id;

    if (dev_info->ver_str_len) {
      // Free old string if we're updating
      if (neighbor->version_str != NULL) {
        vPortFree(neighbor->version_str);
      }
      neighbor->version_str = static_cast<char *>(pvPortMalloc(dev_info->ver_str_len + 1));
      configASSERT(neighbor->version_str);
      memcpy(neighbor->version_str, &dev_info->strings[0], dev_info->ver_str_len);
      neighbor->version_str[dev_info->ver_str_len] = 0; // null terminate string
    }

    if (dev_info->dev_name_len) {
      // Free old string if we're updating
      if (neighbor->device_name != NULL) {
        vPortFree(neighbor->device_name);
      }
      neighbor->device_name = static_cast<char *>(pvPortMalloc(dev_info->dev_name_len + 1));
      configASSERT(neighbor->device_name);
      memcpy(neighbor->device_name, &dev_info->strings[dev_info->ver_str_len],
             dev_info->dev_name_len);
      neighbor->device_name[dev_info->dev_name_len] = 0; // null terminate string
    }
    rval = true;
  }

  return rval;
}

/*!
  Process device information reply

  \param *dev_info - device information data
  \return ERR_OK
*/
static BmErr bcmp_process_info_reply(BcmpProcessData data) {
  bcmp_device_info_reply_t *info = (bcmp_device_info_reply_t *)data.payload;

  bcmp_ll_element_t *element = _info_request_list.find(info->info.node_id);
  if ((element != NULL) && (element->fp != NULL)) {
    element->fp(info);
  } else {
    //
    // Find neighbor and add info to table if present
    // Only add neighbor info when received to link local multicast address
    //
    bm_neighbor_t *neighbor = bcmp_find_neighbor(info->info.node_id);
    if (neighbor) {
      // Update neighbor info
      _populate_neighbor_info(neighbor, info);
    } else if (_info_expect_node_id && (info->info.node_id == _info_expect_node_id)) {
      _info_expect_node_id = 0;

      // Create temporary neighbor struct that is used by the print function
      bm_neighbor_t *tmp_neighbor =
          static_cast<bm_neighbor_t *>(pvPortMalloc(sizeof(bm_neighbor_t)));
      memset(tmp_neighbor, 0, sizeof(bm_neighbor_t));

      _populate_neighbor_info(tmp_neighbor, info);
      bcmp_print_neighbor_info(tmp_neighbor);

      // Clean up
      configASSERT(bcmp_free_neighbor(tmp_neighbor));
    }
  }
  if (element != NULL) {
    _info_request_list.remove(element);
  }

  return BmOK;
}

BmErr bcmp_process_info_init(void) {
  BmErr err = BmOK;
  BcmpPacketCfg device_info_request = {
      false,
      false,
      bcmp_process_info_request,
  };
  BcmpPacketCfg device_info_reply = {
      false,
      false,
      bcmp_process_info_reply,
  };

  //TODO: handle this better please
  err = packet_add(&device_info_request, BcmpDeviceInfoRequestMessage);
  err = packet_add(&device_info_reply, BcmpDeviceInfoReplyMessage);
  return err;
}
