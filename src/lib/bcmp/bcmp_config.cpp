#include <string.h>
#include "bcmp_config.h"
#include "bm_configs_generic.h"
#include "FreeRTOS.h"
#include "bcmp.h"
#include "bm_util.h"
#include "cbor.h"
extern "C" {
#include "bm_os.h"
#include "packet.h"
}

bool bcmp_config_get(uint64_t target_node_id, BmConfigPartition partition,
                     size_t key_len, const char *key, BmErr &err,
                     BmErr (*reply_cb)(uint8_t *)) {
  bool rval = false;
  if (key) {
    err = BmEINVAL;
    size_t msg_size = sizeof(BmConfigGet) + key_len;
    BmConfigGet *get_msg = (BmConfigGet *)bm_malloc(msg_size);
    if (get_msg) {
      do {
        get_msg->header.target_node_id = target_node_id;
        get_msg->header.source_node_id = node_id();
        get_msg->partition = partition;
        if (key_len > BM_MAX_KEY_LEN_BYTES) {
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
      bm_free(get_msg);
    }
  }
  return rval;
}

bool bcmp_config_set(uint64_t target_node_id, BmConfigPartition partition,
                     size_t key_len, const char *key, size_t value_size, void *val, BmErr &err,
                     BmErr (*reply_cb)(uint8_t *)) {
  bool rval = false;
  if (key) {
    err = BmEINVAL;
    size_t msg_len = sizeof(BmConfigSet) + key_len + value_size;
    BmConfigSet *set_msg = (BmConfigSet *)bm_malloc(msg_len);
    if (set_msg) {
      do {
        set_msg->header.target_node_id = target_node_id;
        set_msg->header.source_node_id = node_id();
        set_msg->partition = partition;
        if (key_len > BM_MAX_KEY_LEN_BYTES) {
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
      bm_free(set_msg);
    }
  }
  return rval;
}

bool bcmp_config_commit(uint64_t target_node_id, BmConfigPartition partition,
                        BmErr &err) {
  bool rval = false;
  err = BmEINVAL;
  BmConfigCommit *commit_msg =
      (BmConfigCommit *)bm_malloc(sizeof(BmConfigCommit));
  if (commit_msg) {
    commit_msg->header.target_node_id = target_node_id;
    commit_msg->header.source_node_id = node_id();
    commit_msg->partition = partition;
    err = bcmp_tx(&multicast_ll_addr, BcmpConfigCommitMessage, (uint8_t *)commit_msg,
                  sizeof(BmConfigCommit));
    if (err == BmOK) {
      rval = true;
    }
    bm_free(commit_msg);
  }
  return rval;
}

bool bcmp_config_status_request(uint64_t target_node_id, BmConfigPartition partition,
                                BmErr &err, BmErr (*reply_cb)(uint8_t *)) {
  bool rval = false;
  err = BmEINVAL;
  BmConfigStatusRequest *status_req_msg =
      (BmConfigStatusRequest *)bm_malloc(
          sizeof(BmConfigStatusRequest));
  if (status_req_msg) {
    status_req_msg->header.target_node_id = target_node_id;
    status_req_msg->header.source_node_id = node_id();
    status_req_msg->partition = partition;
    err = bcmp_tx(&multicast_ll_addr, BcmpConfigStatusRequestMessage, (uint8_t *)status_req_msg,
                  sizeof(BmConfigStatusRequest), 0, reply_cb);
    if (err == BmOK) {
      rval = true;
    }
    bm_free(status_req_msg);
  }
  return rval;
}

bool bcmp_config_status_response(uint64_t target_node_id,
                                 BmConfigPartition partition, bool commited,
                                 uint8_t num_keys, const GenericConfigKey *keys, BmErr &err,
                                 uint16_t seq_num) {
  bool rval = false;
  err = BmEINVAL;
  size_t msg_size = sizeof(BmConfigStatusResponse);
  for (int i = 0; i < num_keys; i++) {
    msg_size += sizeof(BmConfigStatusKeyData);
    msg_size += keys[i].keyLen;
  }
  if (msg_size > BCMP_MAX_PAYLOAD_SIZE_BYTES) {
    return false;
  }
  BmConfigStatusResponse *status_resp_msg =
      (BmConfigStatusResponse *)bm_malloc(msg_size);
  if (status_resp_msg) {
    do {
      status_resp_msg->header.target_node_id = target_node_id;
      status_resp_msg->header.source_node_id = node_id();
      status_resp_msg->committed = commited;
      status_resp_msg->partition = partition;
      status_resp_msg->num_keys = num_keys;
      BmConfigStatusKeyData *key_data =
          (BmConfigStatusKeyData *)status_resp_msg->keyData;
      for (int i = 0; i < num_keys; i++) {
        key_data->key_length = keys[i].keyLen;
        memcpy(key_data->key, keys[i].keyBuffer, keys[i].keyLen);
        key_data += sizeof(BmConfigStatusKeyData);
        key_data += keys[i].keyLen;
      }
      err = bcmp_tx(&multicast_ll_addr, BcmpConfigStatusResponseMessage,
                    (uint8_t *)status_resp_msg, msg_size, seq_num);
      if (err == BmOK) {
        rval = true;
      }
    } while (0);
    bm_free(status_resp_msg);
  }
  return rval;
}

static void bcmp_config_process_commit_msg(BmConfigCommit *msg) {
  if (msg) {
    bcmp_commit_config((BmConfigPartition)msg->partition);
  }
}

static void bcmp_config_process_status_request_msg(BmConfigStatusRequest *msg,
                                                   uint16_t seq_num) {
  if (msg) {
    BmErr err;
    do {
      uint8_t num_keys;
      const GenericConfigKey *keys =bcmp_config_get_stored_keys(num_keys, msg->partition);
      bcmp_config_status_response(msg->header.source_node_id, msg->partition, bcmp_config_needs_commit(msg->partition),
                                  num_keys, keys, err, seq_num);
      if (err != BmOK) {
        printf("Error processing config status request.\n");
      }
    } while (0);
  }
}

static bool bcmp_config_send_value(uint64_t target_node_id,
                                   BmConfigPartition partition, uint32_t data_length,
                                   void *data, BmErr &err, uint16_t seq_num) {
  bool rval = false;
  err = BmEINVAL;
  size_t msg_len = data_length + sizeof(BmConfigValue);
  BmConfigValue *value_msg = (BmConfigValue *)bm_malloc(msg_len);
  if (value_msg) {
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
    bm_free(value_msg);
  }
  return rval;
}

static void bcmp_config_process_config_get_msg(BmConfigGet *msg, uint16_t seq_num) {
  if (msg) {
    do {
      size_t buffer_len = BM_MAX_CONFIG_BUFFER_SIZE_BYTES;
      uint8_t *buffer = (uint8_t *)bm_malloc(buffer_len);
      if (buffer) {
        if (bcmp_get_config(msg->key, msg->key_length, buffer, buffer_len, msg->partition)) {
          BmErr err;
          bcmp_config_send_value(msg->header.source_node_id, msg->partition, buffer_len, buffer,
                                err, seq_num);
        }
        bm_free(buffer);
      }
    } while (0);
  }
}

static void bcmp_config_process_config_set_msg(BmConfigSet *msg, uint16_t seq_num) {
  if (msg) {
    do {
      if (msg->data_length > BM_MAX_CONFIG_BUFFER_SIZE_BYTES || msg->data_length == 0) {
        break;
      }
      if (bcmp_set_config((const char *)msg->keyAndData, msg->key_length,
                          &msg->keyAndData[msg->key_length], msg->data_length, msg->partition)) {
        BmErr err;
        bcmp_config_send_value(msg->header.source_node_id, msg->partition, msg->data_length,
                              &msg->keyAndData[msg->key_length], err, seq_num);
      }
    } while (0);
  }
}

static void bcmp_process_value_message(BmConfigValue *msg) {
  CborValue it;
  CborParser parser;
  do {
    if (cbor_parser_init(msg->data, msg->data_length, 0, &parser, &it) != CborNoError) {
      break;
    }
    if (!cbor_value_is_valid(&it)) {
      break;
    }
    GenericConfigDataTypes type;
    if (!Configuration::cborTypeToConfigType(&it, type)) {
      break;
    }
    switch (type) {
    case UINT32: {
      uint64_t temp;
      if (cbor_value_get_uint64(&it, &temp) != CborNoError) {
        break;
      }
      printf("Node Id: %016" PRIx64 " Value:%" PRIu32 "\n", msg->header.source_node_id, temp);
      break;
    }
    case INT32: {
      int64_t temp;
      if (cbor_value_get_int64(&it, &temp) != CborNoError) {
        break;
      }
      printf("Node Id: %016" PRIx64 " Value:%" PRId64 "\n", msg->header.source_node_id, temp);
      break;
    }
    case FLOAT: {
      float temp;
      if (cbor_value_get_float(&it, &temp) != CborNoError) {
        break;
      }
      printf("Node Id: %016" PRIx64 " Value:%f\n", msg->header.source_node_id, temp);
      break;
    }
    case STR: {
      size_t buffer_len = BM_MAX_CONFIG_BUFFER_SIZE_BYTES;
      char *buffer = (char *)bm_malloc(buffer_len);
      if (buffer) {
        do {
          if (cbor_value_copy_text_string(&it, buffer, &buffer_len, NULL) != CborNoError) {
            break;
          }
          if (buffer_len >= BM_MAX_CONFIG_BUFFER_SIZE_BYTES) {
            break;
          }
          buffer[buffer_len] = '\0';
          printf("Node Id: %016" PRIx64 " Value:%s\n", msg->header.source_node_id, buffer);
        } while (0);
        bm_free(buffer);
      }
      break;
    }
    case BYTES: {
      size_t buffer_len = BM_MAX_CONFIG_BUFFER_SIZE_BYTES;
      uint8_t *buffer = (uint8_t *)bm_malloc(buffer_len);
      if (buffer) {
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
        bm_free(buffer);
      }
      break;
    }
    case ARRAY: {
      printf("Node Id: %016" PRIx64 " Value: Array\n", msg->header.source_node_id);
      size_t buffer_len = BM_MAX_CONFIG_BUFFER_SIZE_BYTES;
      uint8_t *buffer = static_cast<uint8_t *>(bm_malloc(buffer_len));
      if (buffer) {
        for (size_t i = 0; i < buffer_len; i++) {
          printf(" %02x", buffer[i]);
          if (i % 16 == 15) {
            printf("\n");
          }
        }
        printf("\n");
        bm_free(buffer);
      }
      break;
    }
    }
  } while (0);
}

bool bcmp_config_del_key(uint64_t target_node_id, BmConfigPartition partition,
                         size_t key_len, const char *key, BmErr (*reply_cb)(uint8_t *)) {
  bool rval = false;
  if (key) {
    size_t msg_size = sizeof(BmConfigDeleteKeyRequest) + key_len;
    BmConfigDeleteKeyRequest *del_msg =
        (BmConfigDeleteKeyRequest *)bm_malloc(msg_size);
    if (del_msg) {
      del_msg->header.target_node_id = target_node_id;
      del_msg->header.source_node_id = node_id();
      del_msg->partition = partition;
      del_msg->key_length = key_len;
      memcpy(del_msg->key, key, key_len);
      if (bcmp_tx(&multicast_ll_addr, BcmpConfigDeleteRequestMessage, (uint8_t *)(del_msg),
                  msg_size, 0, reply_cb) == BmOK) {
        rval = true;
      }
      bm_free(del_msg);
    }
  }
  return rval;
}

static bool bcmp_config_send_del_key_response(uint64_t target_node_id,
                                              BmConfigPartition partition,
                                              size_t key_len, const char *key, bool success,
                                              uint16_t seq_num) {
  bool rval = false;
  if (key) {
    size_t msg_size = sizeof(BmConfigDeleteKeyResponse) + key_len;
    BmConfigDeleteKeyResponse *del_resp =
        (BmConfigDeleteKeyResponse *)bm_malloc(msg_size);
    if(del_resp) {
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
      bm_free(del_resp);
    }
  }
  return rval;
}

static void bcmp_process_del_request_message(BmConfigDeleteKeyRequest *msg,
                                             uint16_t seq_num) {
  if (msg) {
    do {
      bool success = bcmp_remove_key(msg->key, msg->key_length, msg->partition);
      if (!bcmp_config_send_del_key_response(msg->header.source_node_id, msg->partition,
                                            msg->key_length, msg->key, success, seq_num)) {
        printf("Failed to send del key resp\n");
      }
    } while (0);
  }
}

static void bcmp_process_del_response_message(BmConfigDeleteKeyResponse *msg) {
  if (msg) {
    char *key_print_buf = (char *)bm_malloc(msg->key_length + 1);
    if (key_print_buf) {
      memcpy(key_print_buf, msg->key, msg->key_length);
      key_print_buf[msg->key_length] = '\0';
      printf("Node Id: %016" PRIx64 " Key Delete Response - Key: %s, Partition: %d, Success %d\n",
            msg->header.source_node_id, key_print_buf, msg->partition, msg->success);
      bm_free(key_print_buf);
    }
  }
}

/*!
    \return true if the caller should forward the message, false if the message was handled
*/
static BmErr bcmp_process_config_message(BcmpProcessData data) {
  BmErr err = BmEINVAL;
  bool should_forward = false;
  BmConfigHeader *msg_header = (BmConfigHeader *)data.payload;

  if (msg_header->target_node_id != node_id()) {
    should_forward = true;
  } else {
    err = BmOK;
    switch (data.header->type) {
    case BcmpConfigGetMessage: {
      bcmp_config_process_config_get_msg((BmConfigGet *)(data.payload),
                                         data.header->seq_num);
      break;
    }
    case BcmpConfigSetMessage: {
      bcmp_config_process_config_set_msg((BmConfigSet *)data.payload,
                                         data.header->seq_num);
      break;
    }
    case BcmpConfigCommitMessage: {
      bcmp_config_process_commit_msg((BmConfigCommit *)data.payload);

      break;
    }
    case BcmpConfigStatusRequestMessage: {
      bcmp_config_process_status_request_msg((BmConfigStatusRequest *)data.payload,
                                             data.header->seq_num);
      break;
    }
    case BcmpConfigStatusResponseMessage: {
      BmConfigStatusResponse *msg =
          (BmConfigStatusResponse *)data.payload;
      printf("Response msg -- Node Id: %016" PRIx64 ", Partition: %d, Commit Status: %d\n",
             msg->header.source_node_id, msg->partition, msg->committed);
      printf("Num Keys: %d\n", msg->num_keys);
      BmConfigStatusKeyData *key =
          (BmConfigStatusKeyData *)msg->keyData;
      for (int i = 0; i < msg->num_keys; i++) {
        char *key_buf = (char *)bm_malloc(key->key_length + 1);
        if (key_buf) {
          memcpy(key_buf, key->key, key->key_length);
          key_buf[key->key_length] = '\0';
          printf("%s\n", key_buf);
          key += key->key_length + sizeof(BmConfigStatusKeyData);
          bm_free(key_buf);
        }
      }
      break;
    }
    case BcmpConfigValueMessage: {
      BmConfigValue *msg = (BmConfigValue *)data.payload;
      bcmp_process_value_message(msg);
      break;
    }
    case BcmpConfigDeleteRequestMessage: {
      BmConfigDeleteKeyRequest *msg =
          (BmConfigDeleteKeyRequest *)data.payload;
      bcmp_process_del_request_message(msg, data.header->seq_num);
      break;
    }
    case BcmpConfigDeleteResponseMessage: {
      BmConfigDeleteKeyResponse *msg =
          (BmConfigDeleteKeyResponse *)data.payload;
      bcmp_process_del_response_message(msg);
      break;
    }
    default:
      printf("Invalid config msg\n");
      break;
    }
  }

  if (should_forward) {
    err = bcmp_ll_forward(data.header, data.payload, data.size, data.ingress_port);
  }

  return err;
}

BmErr bcmp_config_init(void) {
  BmErr err = BmOK;
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
