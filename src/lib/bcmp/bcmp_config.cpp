#include "bcmp_config.h"
#include "bcmp.h"
#include "bm_util.h"
extern "C" {
#include "packet.h"
}

static Configuration *USR_CFG;
static Configuration *SYS_CFG;

bool bcmp_config_get(uint64_t target_node_id, bm_common_config_partition_e partition,
                     size_t key_len, const char *key, BmErr &err,
                     BmErr (*reply_cb)(uint8_t *)) {
  configASSERT(key);
  bool rval = false;
  err = BmEINVAL;
  size_t msg_size = sizeof(bm_common_config_get_t) + key_len;
  bm_common_config_get_t *get_msg = (bm_common_config_get_t *)pvPortMalloc(msg_size);
  configASSERT(get_msg);
  do {
    get_msg->header.target_node_id = target_node_id;
    get_msg->header.source_node_id = node_id();
    get_msg->partition = partition;
    if (key_len > cfg::MAX_KEY_LEN_BYTES) {
      break;
    }
    get_msg->key_length = key_len;
    memcpy(get_msg->key, key, key_len);
    err = bcmp_tx(&multicast_ll_addr, BcmpConfigGetMessage, (uint8_t *)get_msg, msg_size, 0,
                  reply_cb);
    if (err == BmOK) {
      rval = true;
    }
  } while (0);
  vPortFree(get_msg);
  return rval;
}

bool bcmp_config_set(uint64_t target_node_id, bm_common_config_partition_e partition,
                     size_t key_len, const char *key, size_t value_size, void *val, BmErr &err,
                     BmErr (*reply_cb)(uint8_t *)) {
  configASSERT(key);
  configASSERT(key);
  bool rval = false;
  err = BmEINVAL;
  size_t msg_len = sizeof(bm_common_config_set_t) + key_len + value_size;
  bm_common_config_set_t *set_msg = (bm_common_config_set_t *)pvPortMalloc(msg_len);
  configASSERT(set_msg);
  do {
    set_msg->header.target_node_id = target_node_id;
    set_msg->header.source_node_id = node_id();
    set_msg->partition = partition;
    if (key_len > cfg::MAX_KEY_LEN_BYTES) {
      break;
    }
    set_msg->key_length = key_len;
    memcpy(set_msg->keyAndData, key, key_len);
    set_msg->data_length = value_size;
    memcpy(&set_msg->keyAndData[key_len], val, value_size);
    err = bcmp_tx(&multicast_ll_addr, BcmpConfigSetMessage, (uint8_t *)set_msg, msg_len, 0,
                  reply_cb);
    if (err == BmOK) {
      rval = true;
    }
  } while (0);
  vPortFree(set_msg);
  return rval;
}

bool bcmp_config_commit(uint64_t target_node_id, bm_common_config_partition_e partition,
                        BmErr &err) {
  bool rval = false;
  err = BmEINVAL;
  bm_common_config_commit_t *commit_msg =
      (bm_common_config_commit_t *)pvPortMalloc(sizeof(bm_common_config_commit_t));
  configASSERT(commit_msg);
  commit_msg->header.target_node_id = target_node_id;
  commit_msg->header.source_node_id = node_id();
  commit_msg->partition = partition;
  err = bcmp_tx(&multicast_ll_addr, BcmpConfigCommitMessage, (uint8_t *)commit_msg,
                sizeof(bm_common_config_commit_t));
  if (err == BmOK) {
    rval = true;
  }
  vPortFree(commit_msg);
  return rval;
}

bool bcmp_config_status_request(uint64_t target_node_id, bm_common_config_partition_e partition,
                                BmErr &err, BmErr (*reply_cb)(uint8_t *)) {
  bool rval = false;
  err = BmEINVAL;
  bm_common_config_status_request_t *status_req_msg =
      (bm_common_config_status_request_t *)pvPortMalloc(
          sizeof(bm_common_config_status_request_t));
  configASSERT(status_req_msg);
  status_req_msg->header.target_node_id = target_node_id;
  status_req_msg->header.source_node_id = node_id();
  status_req_msg->partition = partition;
  err = bcmp_tx(&multicast_ll_addr, BcmpConfigStatusRequestMessage, (uint8_t *)status_req_msg,
                sizeof(bm_common_config_status_request_t), 0, reply_cb);
  if (err == BmOK) {
    rval = true;
  }
  vPortFree(status_req_msg);
  return rval;
}

bool bcmp_config_status_response(uint64_t target_node_id,
                                 bm_common_config_partition_e partition, bool commited,
                                 uint8_t num_keys, const ConfigKey_t *keys, BmErr &err,
                                 uint16_t seq_num) {
  bool rval = false;
  err = BmEINVAL;
  size_t msg_size = sizeof(bm_common_config_status_response_t);
  for (int i = 0; i < num_keys; i++) {
    msg_size += sizeof(bm_common_config_status_key_data_t);
    msg_size += keys[i].keyLen;
  }
  if (msg_size > BCMP_MAX_PAYLOAD_SIZE_BYTES) {
    return false;
  }
  bm_common_config_status_response_t *status_resp_msg =
      (bm_common_config_status_response_t *)pvPortMalloc(msg_size);
  configASSERT(status_resp_msg);
  do {
    status_resp_msg->header.target_node_id = target_node_id;
    status_resp_msg->header.source_node_id = node_id();
    status_resp_msg->committed = commited;
    status_resp_msg->partition = partition;
    status_resp_msg->num_keys = num_keys;
    bm_common_config_status_key_data_t *key_data =
        (bm_common_config_status_key_data_t *)status_resp_msg->keyData;
    for (int i = 0; i < num_keys; i++) {
      key_data->key_length = keys[i].keyLen;
      memcpy(key_data->key, keys[i].keyBuffer, keys[i].keyLen);
      key_data += sizeof(bm_common_config_status_key_data_t);
      key_data += keys[i].keyLen;
    }
    err = bcmp_tx(&multicast_ll_addr, BcmpConfigStatusResponseMessage,
                  (uint8_t *)status_resp_msg, msg_size, seq_num);
    if (err == BmOK) {
      rval = true;
    }
  } while (0);
  vPortFree(status_resp_msg);
  return rval;
}

static void bcmp_config_process_commit_msg(bm_common_config_commit_t *msg) {
  configASSERT(msg);
  switch (msg->partition) {
  case BM_COMMON_CFG_PARTITION_USER: {
    USR_CFG->saveConfig(); // Reboot!
    break;
  }
  case BM_COMMON_CFG_PARTITION_SYSTEM: {
    SYS_CFG->saveConfig(); // Reboot!
    break;
  }
  default:
    printf("Invalid partition\n");
    break;
  }
}

static void bcmp_config_process_status_request_msg(bm_common_config_status_request_t *msg,
                                                   uint16_t seq_num) {
  configASSERT(msg);
  BmErr err;
  do {
    Configuration *cfg;
    if (msg->partition == BM_COMMON_CFG_PARTITION_USER) {
      cfg = USR_CFG;
    } else if (msg->partition == BM_COMMON_CFG_PARTITION_SYSTEM) {
      cfg = SYS_CFG;
    } else {
      printf("Invalid configuration\n");
      break;
    }
    uint8_t num_keys;
    const ConfigKey_t *keys = cfg->getStoredKeys(num_keys);
    bcmp_config_status_response(msg->header.source_node_id, msg->partition, cfg->needsCommit(),
                                num_keys, keys, err, seq_num);
    if (err != BmOK) {
      printf("Error processing config status request.\n");
    }
  } while (0);
}

static bool bcmp_config_send_value(uint64_t target_node_id,
                                   bm_common_config_partition_e partition, uint32_t data_length,
                                   void *data, BmErr &err, uint16_t seq_num) {
  bool rval = false;
  err = BmEINVAL;
  size_t msg_len = data_length + sizeof(bm_common_config_value_t);
  bm_common_config_value_t *value_msg = (bm_common_config_value_t *)pvPortMalloc(msg_len);
  configASSERT(value_msg);
  do {
    value_msg->header.target_node_id = target_node_id;
    value_msg->header.source_node_id = node_id();
    value_msg->partition = partition;
    value_msg->data_length = data_length;
    memcpy(value_msg->data, data, data_length);
    err = bcmp_tx(&multicast_ll_addr, BcmpConfigValueMessage, (uint8_t *)value_msg, msg_len,
                  seq_num);
    if (err == BmOK) {
      rval = true;
    }
  } while (0);
  vPortFree(value_msg);
  return rval;
}

static void bcmp_config_process_config_get_msg(bm_common_config_get_t *msg, uint16_t seq_num) {
  configASSERT(msg);
  do {
    Configuration *cfg;
    if (msg->partition == BM_COMMON_CFG_PARTITION_USER) {
      cfg = USR_CFG;
    } else if (msg->partition == BM_COMMON_CFG_PARTITION_SYSTEM) {
      cfg = SYS_CFG;
    } else {
      break;
    }
    size_t buffer_len = cfg::MAX_CONFIG_BUFFER_SIZE_BYTES;
    uint8_t *buffer = (uint8_t *)pvPortMalloc(buffer_len);
    configASSERT(buffer);
    if (cfg->getConfigCbor(msg->key, msg->key_length, buffer, buffer_len)) {
      BmErr err;
      bcmp_config_send_value(msg->header.source_node_id, msg->partition, buffer_len, buffer,
                             err, seq_num);
    }
    vPortFree(buffer);
  } while (0);
}

static void bcmp_config_process_config_set_msg(bm_common_config_set_t *msg, uint16_t seq_num) {
  configASSERT(msg);
  do {
    Configuration *cfg;
    if (msg->partition == BM_COMMON_CFG_PARTITION_USER) {
      cfg = USR_CFG;
    } else if (msg->partition == BM_COMMON_CFG_PARTITION_SYSTEM) {
      cfg = SYS_CFG;
    } else {
      break;
    }
    if (msg->data_length > cfg::MAX_CONFIG_BUFFER_SIZE_BYTES || msg->data_length == 0) {
      break;
    }
    if (cfg->setConfigCbor((const char *)msg->keyAndData, msg->key_length,
                           &msg->keyAndData[msg->key_length], msg->data_length)) {
      BmErr err;
      bcmp_config_send_value(msg->header.source_node_id, msg->partition, msg->data_length,
                             &msg->keyAndData[msg->key_length], err, seq_num);
    }
  } while (0);
}

static void bcmp_process_value_message(bm_common_config_value_t *msg) {
  CborValue it;
  CborParser parser;
  do {
    if (cbor_parser_init(msg->data, msg->data_length, 0, &parser, &it) != CborNoError) {
      break;
    }
    if (!cbor_value_is_valid(&it)) {
      break;
    }
    ConfigDataTypes_e type;
    if (!Configuration::cborTypeToConfigType(&it, type)) {
      break;
    }
    switch (type) {
    case cfg::ConfigDataTypes_e::UINT32: {
      uint64_t temp;
      if (cbor_value_get_uint64(&it, &temp) != CborNoError) {
        break;
      }
      printf("Node Id: %016" PRIx64 " Value:%" PRIu32 "\n", msg->header.source_node_id, temp);
      break;
    }
    case cfg::ConfigDataTypes_e::INT32: {
      int64_t temp;
      if (cbor_value_get_int64(&it, &temp) != CborNoError) {
        break;
      }
      printf("Node Id: %016" PRIx64 " Value:%" PRId64 "\n", msg->header.source_node_id, temp);
      break;
    }
    case cfg::ConfigDataTypes_e::FLOAT: {
      float temp;
      if (cbor_value_get_float(&it, &temp) != CborNoError) {
        break;
      }
      printf("Node Id: %016" PRIx64 " Value:%f\n", msg->header.source_node_id, temp);
      break;
    }
    case cfg::ConfigDataTypes_e::STR: {
      size_t buffer_len = cfg::MAX_CONFIG_BUFFER_SIZE_BYTES;
      char *buffer = (char *)pvPortMalloc(buffer_len);
      configASSERT(buffer);
      do {
        if (cbor_value_copy_text_string(&it, buffer, &buffer_len, NULL) != CborNoError) {
          break;
        }
        if (buffer_len >= cfg::MAX_CONFIG_BUFFER_SIZE_BYTES) {
          break;
        }
        buffer[buffer_len] = '\0';
        printf("Node Id: %016" PRIx64 " Value:%s\n", msg->header.source_node_id, buffer);
      } while (0);
      vPortFree(buffer);
      break;
    }
    case cfg::ConfigDataTypes_e::BYTES: {
      size_t buffer_len = cfg::MAX_CONFIG_BUFFER_SIZE_BYTES;
      uint8_t *buffer = (uint8_t *)pvPortMalloc(buffer_len);
      configASSERT(buffer);
      do {
        if (cbor_value_copy_byte_string(&it, buffer, &buffer_len, NULL) != CborNoError) {
          break;
        }
        printf("Node Id: %016" PRIx64 " Value: ", msg->header.source_node_id);
        for (size_t i = 0; i < buffer_len; i++) {
          printf("0x%02x:", buffer[i]);
          if (i % 8 == 0) {
            printf("\n");
          }
        }
        printf("\n");
      } while (0);
      vPortFree(buffer);
      break;
    }
    case cfg::ConfigDataTypes_e::ARRAY: {
      printf("Node Id: %016" PRIx64 " Value: Array\n", msg->header.source_node_id);
      size_t buffer_len = cfg::MAX_CONFIG_BUFFER_SIZE_BYTES;
      uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(buffer_len));
      configASSERT(buffer);
      for (size_t i = 0; i < buffer_len; i++) {
        printf(" %02x", buffer[i]);
        if (i % 16 == 15) {
          printf("\n");
        }
      }
      printf("\n");
      vPortFree(buffer);
      break;
    }
    }
  } while (0);
}

bool bcmp_config_del_key(uint64_t target_node_id, bm_common_config_partition_e partition,
                         size_t key_len, const char *key, BmErr (*reply_cb)(uint8_t *)) {
  configASSERT(key);
  bool rval = false;
  size_t msg_size = sizeof(bm_common_config_delete_key_request_t) + key_len;
  bm_common_config_delete_key_request_t *del_msg =
      (bm_common_config_delete_key_request_t *)pvPortMalloc(msg_size);
  configASSERT(del_msg);
  del_msg->header.target_node_id = target_node_id;
  del_msg->header.source_node_id = node_id();
  del_msg->partition = partition;
  del_msg->key_length = key_len;
  memcpy(del_msg->key, key, key_len);
  if (bcmp_tx(&multicast_ll_addr, BcmpConfigDeleteRequestMessage, (uint8_t *)(del_msg),
              msg_size, 0, reply_cb) == BmOK) {
    rval = true;
  }
  vPortFree(del_msg);
  return rval;
}

static bool bcmp_config_send_del_key_response(uint64_t target_node_id,
                                              bm_common_config_partition_e partition,
                                              size_t key_len, const char *key, bool success,
                                              uint16_t seq_num) {
  configASSERT(key);
  bool rval = false;
  size_t msg_size = sizeof(bm_common_config_delete_key_response_t) + key_len;
  bm_common_config_delete_key_response_t *del_resp =
      (bm_common_config_delete_key_response_t *)pvPortMalloc(msg_size);
  configASSERT(del_resp);
  del_resp->header.target_node_id = target_node_id;
  del_resp->header.source_node_id = node_id();
  del_resp->partition = partition;
  del_resp->key_length = key_len;
  memcpy(del_resp->key, key, key_len);
  del_resp->success = success;
  if (bcmp_tx(&multicast_ll_addr, BcmpConfigDeleteResponseMessage, (uint8_t *)del_resp,
              msg_size, seq_num) == BmOK) {
    rval = true;
  }
  vPortFree(del_resp);
  return rval;
}

static void bcmp_process_del_request_message(bm_common_config_delete_key_request_t *msg,
                                             uint16_t seq_num) {
  configASSERT(msg);
  do {
    Configuration *cfg;
    if (msg->partition == BM_COMMON_CFG_PARTITION_USER) {
      cfg = USR_CFG;
    } else if (msg->partition == BM_COMMON_CFG_PARTITION_SYSTEM) {
      cfg = SYS_CFG;
    } else {
      break;
    }
    bool success = cfg->removeKey(msg->key, msg->key_length);
    if (!bcmp_config_send_del_key_response(msg->header.source_node_id, msg->partition,
                                           msg->key_length, msg->key, success, seq_num)) {
      printf("Failed to send del key resp\n");
    }
  } while (0);
}

static void bcmp_process_del_response_message(bm_common_config_delete_key_response_t *msg) {
  configASSERT(msg);
  char *keyprintbuf = (char *)pvPortMalloc(msg->key_length + 1);
  memcpy(keyprintbuf, msg->key, msg->key_length);
  keyprintbuf[msg->key_length] = '\0';
  printf("Node Id: %016" PRIx64 " Key Delete Response - Key: %s, Partition: %d, Success %d\n",
         msg->header.source_node_id, keyprintbuf, msg->partition, msg->success);
  vPortFree(keyprintbuf);
}

/*!
    \return true if the caller should forward the message, false if the message was handled
*/
static BmErr bcmp_process_config_message(BcmpProcessData data) {
  BmErr err = BmEINVAL;
  bool should_forward = false;
  bm_common_config_header_t *msg_header = (bm_common_config_header_t *)data.payload;

  if (msg_header->target_node_id != node_id()) {
    should_forward = true;
  } else {
    switch (data.header->type) {
    case BcmpConfigGetMessage: {
      bcmp_config_process_config_get_msg((bm_common_config_get_t *)(data.payload),
                                         data.header->seq_num);
      break;
    }
    case BcmpConfigSetMessage: {
      bcmp_config_process_config_set_msg((bm_common_config_set_t *)data.payload,
                                         data.header->seq_num);
      break;
    }
    case BcmpConfigCommitMessage: {
      bcmp_config_process_commit_msg((bm_common_config_commit_t *)data.payload);

      break;
    }
    case BcmpConfigStatusRequestMessage: {
      bcmp_config_process_status_request_msg((bm_common_config_status_request_t *)data.payload,
                                             data.header->seq_num);
      break;
    }
    case BcmpConfigStatusResponseMessage: {
      bm_common_config_status_response_t *msg =
          (bm_common_config_status_response_t *)data.payload;
      printf("Response msg -- Node Id: %016" PRIx64 ", Partition: %d, Commit Status: %d\n",
             msg->header.source_node_id, msg->partition, msg->committed);
      printf("Num Keys: %d\n", msg->num_keys);
      bm_common_config_status_key_data_t *key =
          (bm_common_config_status_key_data_t *)msg->keyData;
      for (int i = 0; i < msg->num_keys; i++) {
        char *keybuf = (char *)pvPortMalloc(key->key_length + 1);
        configASSERT(keybuf);
        memcpy(keybuf, key->key, key->key_length);
        keybuf[key->key_length] = '\0';
        printf("%s\n", keybuf);
        key += key->key_length + sizeof(bm_common_config_status_key_data_t);
        vPortFree(keybuf);
      }
      break;
    }
    case BcmpConfigValueMessage: {
      bm_common_config_value_t *msg = (bm_common_config_value_t *)data.payload;
      bcmp_process_value_message(msg);
      break;
    }
    case BcmpConfigDeleteRequestMessage: {
      bm_common_config_delete_key_request_t *msg =
          (bm_common_config_delete_key_request_t *)data.payload;
      bcmp_process_del_request_message(msg, data.header->seq_num);
      break;
    }
    case BcmpConfigDeleteResponseMessage: {
      bm_common_config_delete_key_response_t *msg =
          (bm_common_config_delete_key_response_t *)data.payload;
      bcmp_process_del_response_message(msg);
      break;
    }
    default:
      printf("Invalid config msg\n");
      break;
    }
  }

  if (should_forward) {
    bcmp_ll_forward(data.header, data.payload, data.size, data.ingress_port);
  }

  //TODO properly assign this
  err = BmOK;
  return err;
}

BmErr bcmp_config_init(Configuration *user_cfg, Configuration *sys_cfg) {
  BmErr err = BmEINVAL;
  BcmpPacketCfg config_get = {
      false,
      true,
      bcmp_process_config_message,
  };
  BcmpPacketCfg config_set = {
      false,
      true,
      bcmp_process_config_message,
  };
  BcmpPacketCfg config_commit = {
      false,
      false,
      bcmp_process_config_message,
  };
  BcmpPacketCfg config_status_request = {
      false,
      true,
      bcmp_process_config_message,
  };
  BcmpPacketCfg config_status_response = {
      true,
      false,
      bcmp_process_config_message,
  };
  BcmpPacketCfg config_delete_request = {
      false,
      true,
      bcmp_process_config_message,
  };
  BcmpPacketCfg config_delete_response = {
      true,
      false,
      bcmp_process_config_message,
  };
  BcmpPacketCfg config_value = {
      true,
      false,
      bcmp_process_config_message,
  };

  if (user_cfg && sys_cfg) {
    err = BmOK;
    USR_CFG = user_cfg;
    SYS_CFG = sys_cfg;
  }

  bm_err_check(err, packet_add(&config_get, BcmpConfigGetMessage));
  bm_err_check(err, packet_add(&config_set, BcmpConfigSetMessage));
  bm_err_check(err, packet_add(&config_commit, BcmpConfigCommitMessage));
  bm_err_check(err, packet_add(&config_status_request, BcmpConfigStatusRequestMessage));
  bm_err_check(err, packet_add(&config_status_response, BcmpConfigStatusResponseMessage));
  bm_err_check(err, packet_add(&config_delete_request, BcmpConfigDeleteRequestMessage));
  bm_err_check(err, packet_add(&config_delete_response, BcmpConfigDeleteResponseMessage));
  bm_err_check(err, packet_add(&config_value, BcmpConfigValueMessage));
  return err;
}
