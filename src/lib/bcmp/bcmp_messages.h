#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bcmp_protocol_caps.h"

typedef struct {
  uint16_t type;
  uint16_t checksum;
  uint8_t flags;
  uint8_t rsvd;
  uint8_t payload[0];
} __attribute__((packed)) bcmp_header_t;

typedef struct {
  // Microseconds since system has last reset/powered on.
  uint64_t time_since_boot_us;

  // How long to consider this node alive. 0 can mean indefinite, which could be useful for certain applications.
  uint32_t liveliness_lease_dur_s;
} __attribute__((packed)) bcmp_heartbeat_t;

typedef struct {
  // Node ID of the target node for which the request is being made. (Zeroed = all nodes)
  uint64_t target_node_id;

  // A "unique" ID correlating to a stream of ping requests/replies, usually tied to the process or task ID of the requester
  uint16_t id;

  // Monotonically increasing sequence number relating to the stream identified by id
  uint16_t seq_num;

  // Length of optional payload
  uint16_t payload_len;

  // Optional payload. If used, the reply to this request must send a matching payload to be considered valid
  uint8_t payload[0];
} __attribute__((packed)) bcmp_echo_request_t;

typedef struct {
   // Node ID of the responding node
  uint64_t node_id;

  // ID matching that of the request that triggered this reply
  uint16_t id;

  // Sequence number matching that of the request that triggered this reply
  uint16_t seq_num;

  // Length of optional payload
  uint16_t payload_len;

  // Optional payload. Must match with original request payload to be considered valid.
  uint8_t payload[0];
} __attribute__((packed)) bcmp_echo_reply_t;

typedef struct {
  // Node ID of the target node for which the request is being made. (Zeroed = all nodes)
  uint64_t target_node_id;
} __attribute__((packed)) bcmp_device_info_request_t;

typedef struct {
  // Node ID of the responding node
  uint64_t node_id;

  // Vendor ID of the hardware module implementing the BM Node functions
  uint16_t vendor_id;

  // Product ID for the hardware module implementing the BM Node functions
  uint16_t product_id;

  // Factory-flashed unique serial number
  uint8_t serial_num[16];

  // Last 4 bytes of git SHA
  uint32_t git_sha;

  // Major Version
  uint8_t ver_major;

  // Minor Version
  uint8_t ver_minor;

  // Revision/Patch Version
  uint8_t ver_rev;

  // Version of the product hardware (0 for don't care)
  uint8_t ver_hw;

  // Length of the full version string
  uint16_t ver_str_len;

  // Length of device name
  uint8_t dev_name_len;

  // ver_str is immediately followed by dev_name_len
  char strings[0];
} __attribute__((packed)) bcmp_device_info_reply_t;

typedef struct {
  // Node ID of the target node for which the request is being made. (Zeroed = all nodes)
  uint64_t target_node_id;

  // Length of requested caps list
  uint16_t caps_list_len;

  // List of Capability IDs requested from target node(s)
  uint16_t caps_list[0];
} __attribute__((packed)) bcmp_protocol_caps_request_t;

typedef struct {
  // Node ID of the responding node
  uint64_t node_id;

  // BCMP revision. Helper - doesn't necessarily provide actionable information about supported features. See attached TLVs
  uint16_t bcmp_rev;

  // Number of capability TLVs to expect in the remainder of this message
  uint16_t caps_count;

  // Sequence of capability TLVs (type, length, value). Points to bcmp_cap_tlv* elements. Each TLV value has a defined structure (defined separately)
  // TODO - Switch to bcmp_cap_tlv caps[0]; when implemented
  uint8_t caps[0];
} __attribute__((packed)) bcmp_protocol_caps_reply_t;

typedef struct {
  // Node ID of the target node for which the request is being made. (Zeroed = all nodes)
  uint64_t target_node_id;
} __attribute__((packed)) bcmp_neighbor_table_request_t;

typedef struct {
  // Node ID of the responding node
  uint64_t node_id;

  // Length of port list
  uint8_t port_len;

  // List of local BM ports and basic status information about them
  // TODO - Switch to bcmp_port_info port_list[0]; when defined
  uint8_t port_list[0];

  // Followed by neighbor info
  // bcmp_neighbor_tableReplyNeighborInfo_t
} __attribute__((packed)) bcmp_neighbor_table_reply_t;

typedef struct {
  // Length of neighbor list
  uint16_t neighbor_len;

  // List containing information about each discovered neighbor, including which port they are on, and their current status. Points to bcmp_neighbor_info*
  uint8_t neighbor_info[0];
} __attribute__((packed)) bcmp_neighbor_tableReplyNeighborInfo_t;

typedef struct {
  // Node ID of the target node for which the request is being made. (Zeroed = all nodes)
  uint64_t target_node_id;
} __attribute__((packed)) bcmp_resource_table_request_t;

typedef struct {
  // Node ID of the responding node
  uint64_t node_id;

  // Length of resource list
  uint16_t resource_len;

  // List of structures containing information for all resource interests known about in this node. Points to bcmp_resource_info*
  // TODO - Switch to bcmp_resource_info resource_list[0]; when defined
  uint8_t resource_list[0];
} __attribute__((packed)) bcmp_resource_table_reply_t;

typedef struct {
  // Node ID of the target node for which the request is being made. (Zeroed = all nodes)
  uint64_t target_node_id;

  // Number of option TLVs to expect in the remainder of this message
  uint16_t option_count;

  // TODO - switch to bcmp_proto_opt_tlv options[0]; when defined
  uint8_t options[0];
} __attribute__((packed)) bcmp_neighbor_proto_request_t;

typedef struct {
  // Node ID of the responding node
  uint64_t node_id;

  // Number of option TLVs to expect in the remainder of this message
  uint16_t option_count;

  // TODO - switch to bcmp_proto_opt_tlv option_list[0]; when defined
  uint8_t option_list[0];
} __attribute__((packed)) bcmp_neighbor_proto_reply_t;

typedef struct {
  // Node ID of the target node for which the request is being made. (Zeroed = all nodes)
  uint64_t target_node_id;
} __attribute__((packed)) bcmp_netstat_request_t;

typedef struct {
  // Node ID of the responding node
  uint64_t node_id;

  // TODO - add node info here
} __attribute__((packed)) bcmp_netstat_reply_t;

typedef struct {
  // Node ID of the target node for which the request is being made. (Zeroed = all nodes)
  uint64_t target_node_id;
} __attribute__((packed)) bcmp_power_state_request_t;

typedef struct {
  // Node ID of the responding node
  uint64_t node_id;

  // Length of port list
  uint8_t port_len;

  // List containing power state and information for each port.
  // TODO - switch to  bcmp_port_power port_list[0];
  uint8_t port_list[0];
} __attribute__((packed)) bcmp_power_state_reply_t;

typedef struct {
  // Node ID of the target node for which the request is being made. (Zeroed = all nodes)
  uint64_t target_node_id;

  // Bypass preventative measures to trigger a reboot (use with caution)
  uint8_t force;
} __attribute__((packed)) bcmp_reboot_request_t;

typedef struct {
  // Node ID of the responding node
  uint64_t node_id;

  // Request accepted or rejected
  uint8_t ack_nack;

} __attribute__((packed)) bcmp_reboot_reply_t;

typedef struct {
  // Node ID of the target node for which the request is being made. (Zeroed = all nodes)
  uint64_t target_node_id;
} __attribute__((packed)) bcmp_net_assert_quiet_t;

// TODO
#if 0
typedef struct {

} __attribute__((packed)) bcmp_config_get_t;

typedef struct {

} __attribute__((packed)) bcmp_config_value_t;

typedef struct {

} __attribute__((packed)) bcmp_config_set_t;

typedef struct {

} __attribute__((packed)) bcmp_config_commit_t;

typedef struct {

} __attribute__((packed)) bcmp_config_status_t;

// DFU stuff goes below
#endif


typedef enum {
  BCMP_ACK = 0x00,
  BCMP_HEARTBEAT = 0x01,

  BCMP_ECHO_REQUEST = 0x02,
  BCMP_ECHO_REPLY = 0x03,
  BCMP_DEVICE_INFO_REQUEST = 0x04,
  BCMP_DEVICE_INFO_REPLY = 0x05,
  BCMP_PROTOCOL_CAPS_REQUEST = 0x06,
  BCMP_PROTOCOL_CAPS_REPLY = 0x07,
  BCMP_NEIGHBOR_TABLE_REQUEST = 0x08,
  BCMP_NEIGHBOR_TABLE_REPLY = 0x09,
  BCMP_RESOURCE_TABLE_REQUEST = 0x0A,
  BCMP_RESOURCE_TABLE_REPLY = 0x0B,
  BCMP_NEIGHBOR_PROTO_REQUEST = 0x0C,
  BCMP_NEIGHBOR_PROTO_REPLY = 0x0D,

  BCMP_NET_STAT_REQUEST = 0xB0,
  BCMP_NET_STAT_REPLY = 0xB1,
  BCMP_POWER_STAT_REQUEST = 0xB2,
  BCMP_POWER_STAT_REPLY = 0xB3,

  BCMP_REBOOT_REQUEST = 0xC0,
  BCMP_REBOOT_REPLY = 0xC1,
  BCMP_NET_ASSERT_QUIET = 0xC2,

  BCMP_CONFIG_GET = 0xA0,
  BCMP_CONFIG_VALUE = 0xA1,
  BCMP_CONFIG_SET = 0xA2,
  BCMP_CONFIG_COMMIT = 0xA3,
  BCMP_CONFIG_STATUS = 0xA4,

  BCMP_DFU_START = 0xD0,
  BCMP_DFU_PAYLOAD_REQ = 0xD1,
  BCMP_DFU_PAYLOAD = 0xD2,
  BCMP_DFU_END = 0xD3,
  BCMP_DFU_ACK = 0xD4,
  BCMP_DFU_ABORT = 0xD5,
  BCMP_DFU_HEARTBEAT = 0xD6,
  BCMP_DFU_BEGIN_HOST = 0xD7,
} bcmp_message_type_t;
