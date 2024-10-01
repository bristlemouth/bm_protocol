#include "bcmp_info.h"
#include <string.h>

#include "bcmp.h"
extern "C" {
#include "bm_os.h"
#include "packet.h"
}
#include "bcmp_linked_list_generic.h"
#include "bcmp_neighbors.h"

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
  uint8_t version_string_len = strlen(version_string());
  uint8_t device_name_len = strlen(device_name());
  uint16_t info_len = sizeof(BcmpDeviceInfoReply) + version_string_len + device_name_len;
  BcmpDeviceInfoReply *dev_info = (BcmpDeviceInfoReply *)bm_malloc(info_len);

  if (dev_info) {
    memset(dev_info, 0, info_len);
    dev_info->info.node_id = node_id();
    dev_info->info.vendor_id = vendor_id();
    dev_info->info.product_id = product_id();
    dev_info->info.git_sha = git_sha();
    dev_info->info.ver_hw = hardware_revision();
    dev_info->ver_str_len = version_string_len;
    dev_info->dev_name_len = device_name_len;

    memcpy(&dev_info->strings[0], version_string(), version_string_len);
    memcpy(&dev_info->strings[version_string_len], device_name(), device_name_len);

    err = serial_number(dev_info->info.serial_num, member_size(BcmpDeviceInfo, serial_num));
    bm_err_check(err, firmware_version(&dev_info->info.ver_major, &dev_info->info.ver_minor,
                                       &dev_info->info.ver_rev));
    bm_err_check(err, bcmp_tx(dst, BcmpDeviceInfoReplyMessage, (uint8_t *)dev_info, info_len));

    bm_free(dev_info);
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

  if ((request->target_node_id == 0) || (node_id() == request->target_node_id)) {
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
static bool populate_neighbor_info(BcmpNeighbor *neighbor, BcmpDeviceInfoReply *dev_info) {
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
      if (neighbor->version_str) {
        memcpy(neighbor->version_str, &dev_info->strings[0], dev_info->ver_str_len);
        neighbor->version_str[dev_info->ver_str_len] = 0; // null terminate string
        rval = true;
      }
    }

    if (dev_info->dev_name_len) {
      // Free old string if we're updating
      if (neighbor->device_name != NULL) {
        bm_free(neighbor->device_name);
      }
      neighbor->device_name = (char *)bm_malloc(dev_info->dev_name_len + 1);
      if (neighbor->device_name) {
        memcpy(neighbor->device_name, &dev_info->strings[dev_info->ver_str_len],
               dev_info->dev_name_len);
        neighbor->device_name[dev_info->dev_name_len] = 0; // null terminate string
        rval = true;
      }
    }
  }

  return rval;
}

/*!
  @brief Process device information reply

  @param *dev_info - device information data

  @return BmOK on success
  @return BmErr on failure
*/
static BmErr bcmp_process_info_reply(BcmpProcessData data) {
  BcmpDeviceInfoReply *info = (BcmpDeviceInfoReply *)data.payload;
  BmErr err = BmEBADMSG;

  bcmp_ll_element_t *element = INFO_REQUEST_LIST.find(info->info.node_id);
  if ((element != NULL) && (element->fp != NULL)) {
    element->fp(info);
  } else {
    //
    // Find neighbor and add info to table if present
    // Only add neighbor info when received to link local multicast address
    //
    BcmpNeighbor *neighbor = bcmp_find_neighbor(info->info.node_id);
    if (neighbor) {
      // Update neighbor info
      populate_neighbor_info(neighbor, info);
    } else if (INFO_EXPECT_NODE_ID && (info->info.node_id == INFO_EXPECT_NODE_ID)) {
      INFO_EXPECT_NODE_ID = 0;
      err = BmENOMEM;

      // Create temporary neighbor struct that is used by the print function
      BcmpNeighbor *tmp_neighbor = (BcmpNeighbor *)bm_malloc(sizeof(BcmpNeighbor));
      if (tmp_neighbor) {
        memset(tmp_neighbor, 0, sizeof(BcmpNeighbor));

        populate_neighbor_info(tmp_neighbor, info);
        bcmp_print_neighbor_info(tmp_neighbor);

        // Clean up
        err = bcmp_free_neighbor(tmp_neighbor) ? BmOK : err;
      }
    }
  }
  if (element != NULL) {
    INFO_REQUEST_LIST.remove(element);
  }

  return err;
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

/*!
  @brief Initialize BCMP Device Info Module

  @return BmOK on success
  @return BmErr on fail
 */
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
