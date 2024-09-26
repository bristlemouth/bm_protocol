#include "bcmp_info.h"
#include <string.h>

#include "bcmp.h"
extern "C" {
#include "bm_os.h"
#include "packet.h"
}
#include "bcmp_linked_list_generic.h"
#include "bcmp_neighbors.h"
#include "device_info.h"

#define ver_str_max_len (255)

static uint64_t INFO_EXPECT_NODE_ID;
static BCMP_Linked_List_Generic INFO_REQUEST_LIST;

/*!
  Send current device information over the network

  @param dst - ip address to send the information to

  @return BmOK if successful
  @return BmErr if unsuccessful
*/
static BmErr bcmp_send_info(void *dst) {

  BmErr err = BmENOMEM;
  char *ver_str = (char *)bm_malloc(ver_str_max_len);

  if (ver_str) {
    memset(ver_str, 0, ver_str_max_len);

    uint8_t ver_str_len =
        snprintf(ver_str, ver_str_max_len, "%s@%s", APP_NAME, getFWVersionStr());

    // TODO - use device name instead of UID str
    uint8_t dev_name_len = strlen(getUIDStr());
    uint16_t info_len = sizeof(BcmpDeviceInfoReply) + ver_str_len + dev_name_len;
    uint8_t *dev_info_buff = (uint8_t *)bm_malloc(info_len);

    if (dev_info_buff) {
      memset(dev_info_buff, 0, info_len);

      BcmpDeviceInfoReply *dev_info = (BcmpDeviceInfoReply *)dev_info_buff;
      dev_info->info.node_id = getNodeId();

      // TODO - fill these with actual values
      dev_info->info.vendor_id = 0;
      dev_info->info.product_id = 0;
      memset(dev_info->info.serial_num, '0', sizeof(dev_info->info.serial_num));

      dev_info->info.git_sha = getGitSHA();
      getFWVersion(&dev_info->info.ver_major, &dev_info->info.ver_minor,
                   &dev_info->info.ver_rev);

      // TODO - get actual hardware version
      dev_info->info.ver_hw = 0;

      dev_info->ver_str_len = ver_str_len;
      dev_info->dev_name_len = dev_name_len;

      memcpy(&dev_info->strings[0], ver_str, ver_str_len);
      memcpy(&dev_info->strings[ver_str_len], getUIDStr(), dev_name_len);

      err = bcmp_tx(dst, BcmpDeviceInfoReplyMessage, dev_info_buff, info_len);

      bm_free(dev_info_buff);
    }
    bm_free(ver_str);
  }

  return err;
}

/*!
  Handle device information request (if any or addressed to this node id)

  @param *info_req - info request data
  @param *src - source ip of requester
  @param *dst - destination ip of request (used for responding to the correct multicast address)

  @return BmOK if successful
  @return BmErr if unsuccessful
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

  @param *neighbor - neighbor structure to populate
  @param *dev_info - device info to populate with
  @return true if successful, false otherwise
*/
static bool populate_neighbor_info(bm_neighbor_t *neighbor, BcmpDeviceInfoReply *dev_info) {
  bool rval = false;
  if (neighbor) {
    memcpy(&neighbor->info, &dev_info->info, sizeof(BcmpDeviceInfo));

    neighbor->node_id = dev_info->info.node_id;

    if (dev_info->ver_str_len) {
      // Free old string if we're updating
      if (neighbor->version_str != NULL) {
        bm_free(neighbor->version_str);
      }
      neighbor->version_str = (char *)bm_malloc(dev_info->ver_str_len + 1);
      configASSERT(neighbor->version_str);
      memcpy(neighbor->version_str, &dev_info->strings[0], dev_info->ver_str_len);
      neighbor->version_str[dev_info->ver_str_len] = 0; // null terminate string
    }

    if (dev_info->dev_name_len) {
      // Free old string if we're updating
      if (neighbor->device_name != NULL) {
        bm_free(neighbor->device_name);
      }
      neighbor->device_name = (char *)bm_malloc(dev_info->dev_name_len + 1);
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
  BcmpDeviceInfoReply *info = (BcmpDeviceInfoReply *)data.payload;

  bcmp_ll_element_t *element = INFO_REQUEST_LIST.find(info->info.node_id);
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
      populate_neighbor_info(neighbor, info);
    } else if (INFO_EXPECT_NODE_ID && (info->info.node_id == INFO_EXPECT_NODE_ID)) {
      INFO_EXPECT_NODE_ID = 0;

      // Create temporary neighbor struct that is used by the print function
      bm_neighbor_t *tmp_neighbor =
          static_cast<bm_neighbor_t *>(pvPortMalloc(sizeof(bm_neighbor_t)));
      memset(tmp_neighbor, 0, sizeof(bm_neighbor_t));

      populate_neighbor_info(tmp_neighbor, info);
      bcmp_print_neighbor_info(tmp_neighbor);

      // Clean up
      configASSERT(bcmp_free_neighbor(tmp_neighbor));
    }
  }
  if (element != NULL) {
    INFO_REQUEST_LIST.remove(element);
  }

  return BmOK;
}

/*!
  Set node id to expect information from. Will print out the information
  when it is received.

  \param node_id node id to expect device information from
  \return none
*/
void bcmp_expect_info_from_node_id(uint64_t node_id) {
  // TODO - add expiration and/or callback?
  INFO_EXPECT_NODE_ID = node_id;
}

/*!
  Send a request for device information.

  @param target_node_id - node id of the target we want the information from (0 for all targets)
  @param *addr - ip address to send request to

  @return BmOK if successful
  @return BmErr if unsuccessful
*/
BmErr bcmp_request_info(uint64_t target_node_id, const void *addr, void (*cb)(void *)) {
  INFO_REQUEST_LIST.add(target_node_id, cb);

  BcmpDeviceInfoRequest info_req = {.target_node_id = target_node_id};

  return bcmp_tx(addr, BcmpDeviceInfoRequestMessage, (uint8_t *)&info_req, sizeof(info_req));
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

  bm_err_check(err, packet_add(&device_info_request, BcmpDeviceInfoRequestMessage));
  bm_err_check(err, packet_add(&device_info_reply, BcmpDeviceInfoReplyMessage));
  return err;
}
