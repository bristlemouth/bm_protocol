#include "ncp_config.h"
#include "FreeRTOS.h"
#include "bm_serial.h"
#include "bridgeLog.h"
#include "device_info.h"
#include "messages/config.h"

static uint8_t *alloc_ncp_key_buffer(uint8_t num_keys, const ConfigKey *keys, size_t &len);

// These are the callback functions that BCMP req/rep will call when we get a response
// from the target node.
BmErr _cfg_get_bcmp_cb(uint8_t *payload);
BmErr _cfg_set_bcmp_cb(uint8_t *payload);
BmErr _cfg_status_request_bcmp_cb(uint8_t *payload);
BmErr _cfg_key_del_bcmp_cb(uint8_t *payload);

bool ncp_cfg_get_cb(uint64_t node_id, BmConfigPartition partition, size_t key_len,
                    const char *key) {
  bool rval = false;
  uint8_t *valueBuf = NULL;
  do {
    if (node_id == getNodeId() || node_id == 0) {
      valueBuf = reinterpret_cast<uint8_t *>(pvPortMalloc(MAX_CONFIG_BUFFER_SIZE_BYTES));
      configASSERT(valueBuf);
      size_t value_len = MAX_CONFIG_BUFFER_SIZE_BYTES;
      if (!get_config_cbor(partition, key, key_len, valueBuf, &value_len)) {
        printf("Failed to get config.\n");
        break;
      }
      if (bm_serial_cfg_value(node_id, partition, value_len, valueBuf) != BM_SERIAL_OK) {
        printf("Failed to send cfg\n");
        break;
      }
    } else {
      BmErr err = BmOK;
      if (!bcmp_config_get(node_id, (BmConfigPartition)partition, key_len, key, &err,
                           _cfg_get_bcmp_cb)) {
        bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Failed to issue bcmp cfg get, err: %" PRIi8 "\n", err);
        break;
      }
    }
    rval = true;
  } while (0);
  if (valueBuf != NULL) {
    vPortFree(valueBuf);
  }
  return rval;
}

bool ncp_cfg_set_cb(uint64_t node_id, BmConfigPartition partition, size_t key_len,
                    const char *key, size_t value_size, void *val) {
  bool rval = false;
  do {
    if (node_id == getNodeId() || node_id == 0) {
      if (!set_config_cbor(partition, key, key_len, reinterpret_cast<uint8_t *>(val),
                           value_size)) {
        printf("Failed to set Config\n");
        break;
      }
      if (bm_serial_cfg_value(node_id, partition, value_size,
                              reinterpret_cast<uint8_t *>(val)) != BM_SERIAL_OK) {
        printf("Failed to send cfg\n");
        break;
      }
    } else {
      BmErr err = BmOK;
      if (!bcmp_config_set(node_id, (BmConfigPartition)partition, key_len, key, value_size, val,
                           &err, _cfg_get_bcmp_cb)) {
        bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Failed to issue bcmp cfg set, err: %" PRIi8 "\n", err);
        break;
      }
    }
    rval = true;
  } while (0);
  return rval;
}

bool ncp_cfg_commit_cb(uint64_t node_id, BmConfigPartition partition) {
  bool rval = false;
  do {
    if (node_id == getNodeId() || node_id == 0) {
      if (!save_config(partition, true)) {
        printf("Failed to save config!\n");
        break;
      }
    } else {
      BmErr err = BmOK;
      if (!bcmp_config_commit(node_id, (BmConfigPartition)partition, &err)) {
        bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Failed to issue bcmp cfg commit, err: %" PRIi8 "\n", err);
        break;
      }
    }
    rval = true;
  } while (0);
  return rval;
}

bool ncp_cfg_status_request_cb(uint64_t node_id, BmConfigPartition partition) {
  bool rval = false;
  do {
    if (node_id == getNodeId() || node_id == 0) {
      uint8_t num_keys;
      const ConfigKey *keys = get_stored_keys(partition, &num_keys);
      size_t buffer_size;
      uint8_t *keyBuf = (num_keys) ? alloc_ncp_key_buffer(num_keys, keys, buffer_size) : NULL;
      if (bm_serial_cfg_status_response(node_id, partition, needs_commit(partition), num_keys,
                                        keyBuf) != BM_SERIAL_OK) {
        printf("Failed to send status resp\n");
        break;
      }
      vPortFree(keyBuf);
    } else {
      BmErr err = BmOK;
      if (!bcmp_config_status_request(node_id, (BmConfigPartition)partition, &err,
                                      _cfg_status_request_bcmp_cb)) {
        bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Failed to issue bcmp cfg status request, err: %" PRIi8 "\n", err);
        break;
      }
    }
    rval = true;
  } while (0);
  return rval;
}

bool ncp_cfg_key_del_request_cb(uint64_t node_id, BmConfigPartition partition, size_t key_len,
                                const char *key) {
  bool rval = false;
  do {
    if (node_id == getNodeId() || node_id == 0) {
      bool success = true;
      if (!remove_key(partition, key, key_len)) {
        success = false;
      }
      if (bm_serial_cfg_delete_response(node_id, partition, key_len, key, success) !=
          BM_SERIAL_OK) {
        printf("Failed to send key del response\n");
        break;
      }
      rval = true;
    } else {
      if (!bcmp_config_del_key(node_id, (BmConfigPartition)partition, key_len, key,
                               _cfg_key_del_bcmp_cb)) {
        bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                       "Failed to issue bcmp cfg del\n");
        break;
      }
    }
    rval = true;
  } while (0);
  return rval;
}

void ncp_cfg_init(void) {}

/* NOTE: Caller must free allocated buffer */
static uint8_t *alloc_ncp_key_buffer(uint8_t num_keys, const ConfigKey *keys, size_t &len) {
  len = 0;
  for (int i = 0; i < num_keys; i++) {
    len += sizeof(BmConfigStatusKeyData);
    len += keys[i].key_len;
  }
  uint8_t *buf = reinterpret_cast<uint8_t *>(pvPortMalloc(len));
  configASSERT(buf);
  BmConfigStatusKeyData *key_data =
      reinterpret_cast<BmConfigStatusKeyData *>(buf);
  for (int i = 0; i < num_keys; i++) {
    key_data->key_length = keys[i].key_len;
    memcpy(key_data->key, keys[i].key_buf, keys[i].key_len);
    key_data += sizeof(BmConfigStatusKeyData);
    key_data += keys[i].key_len;
  }
  return buf;
}

BmErr _cfg_get_bcmp_cb(uint8_t *payload) {
  BmErr rval = BmOK;
  if (payload != NULL) {
    BmConfigValue *msg = reinterpret_cast<BmConfigValue *>(payload);
    if (bm_serial_cfg_value(msg->header.source_node_id, msg->partition, msg->data_length,
                            msg->data) != BM_SERIAL_OK) {
      printf("Failed to send get cfg response\n");
      rval = BmEBADMSG;
    }
  } else {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER, "Failed to get cfg\n");
  }
  return rval;
}

BmErr _cfg_set_bcmp_cb(uint8_t *payload) {
  BmErr rval = BmOK;
  if (payload != NULL) {
    BmConfigValue *msg = reinterpret_cast<BmConfigValue *>(payload);
    if (bm_serial_cfg_value(msg->header.source_node_id, msg->partition, msg->data_length,
                            msg->data) != BM_SERIAL_OK) {
      printf("Failed to send set cfg response\n");
      rval = BmEBADMSG;
    }
  } else {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER, "Failed to set cfg\n");
  }
  return rval;
}

BmErr _cfg_status_request_bcmp_cb(uint8_t *payload) {
  BmErr rval = BmOK;
  if (payload != NULL) {
    BmConfigStatusResponse *msg =
        reinterpret_cast<BmConfigStatusResponse *>(payload);
    if (bm_serial_cfg_status_response(msg->header.source_node_id, msg->partition,
                                      msg->committed, msg->num_keys,
                                      msg->keyData) != BM_SERIAL_OK) {
      printf("Failed to send status response\n");
      rval = BmEBADMSG;
    }
  } else {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER,
                   "Failed to get cfg status\n");
  }
  return rval;
}

BmErr _cfg_key_del_bcmp_cb(uint8_t *payload) {
  BmErr rval = BmOK;
  if (payload != NULL) {
    BmConfigDeleteKeyResponse *msg =
        reinterpret_cast<BmConfigDeleteKeyResponse *>(payload);
    if (bm_serial_cfg_delete_response(msg->header.source_node_id, msg->partition,
                                      msg->key_length, msg->key,
                                      msg->success) != BM_SERIAL_OK) {
      printf("Failed to send key del response\n");
      rval = BmEBADMSG;
    }
  } else {
    bridgeLogPrint(BRIDGE_CFG, BM_COMMON_LOG_LEVEL_ERROR, USE_HEADER, "Failed to delete cfg\n");
  }
  return rval;
}
