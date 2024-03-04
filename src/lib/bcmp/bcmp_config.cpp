#include "bcmp_config.h"
#include "FreeRTOS.h"
#include "device_info.h"
#include "bcmp.h"

static Configuration* _usr_cfg;
static Configuration* _sys_cfg;

bool bcmp_config_get(uint64_t target_node_id, bm_common_config_partition_e partition, size_t key_len, const char* key, err_t &err, bcmp_reply_message_cb reply_cb) {
    configASSERT(key);
    bool rval = false;
    err = ERR_VAL;
    size_t msg_size = sizeof(bm_common_config_get_t) + key_len;
    bm_common_config_get_t * get_msg = (bm_common_config_get_t * )pvPortMalloc(msg_size);
    configASSERT(get_msg);
    do {
        get_msg->header.target_node_id = target_node_id;
        get_msg->header.source_node_id = getNodeId();
        get_msg->partition = partition;
        if(key_len > cfg::MAX_KEY_LEN_BYTES) {
            break;
        }
        get_msg->key_length = key_len;
        memcpy(get_msg->key, key, key_len);
        err = bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_GET, (uint8_t *)get_msg, msg_size, 0, reply_cb);
        if(err == ERR_OK) {
            rval = true;
        }
    } while(0);
    vPortFree(get_msg);
    return rval;
}

bool bcmp_config_set(uint64_t target_node_id, bm_common_config_partition_e partition,
    size_t key_len, const char* key, size_t value_size, void * val, err_t &err, bcmp_reply_message_cb reply_cb) {
    configASSERT(key);
    bool rval = false;
    err = ERR_VAL;
    size_t msg_len = sizeof(bm_common_config_set_t) + key_len + value_size;
    bm_common_config_set_t * set_msg = (bm_common_config_set_t *)pvPortMalloc(msg_len);
    configASSERT(set_msg);
    do {
        set_msg->header.target_node_id = target_node_id;
        set_msg->header.source_node_id = getNodeId();
        set_msg->partition = partition;
        if(key_len > cfg::MAX_KEY_LEN_BYTES) {
            break;
        }
        set_msg->key_length = key_len;
        memcpy(set_msg->keyAndData, key, key_len);
        set_msg->data_length = value_size;
        memcpy(&set_msg->keyAndData[key_len], val, value_size);
        err = bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_SET, (uint8_t *)set_msg, msg_len, 0, reply_cb);
        if(err == ERR_OK) {
            rval = true;
        }
    } while(0);
    vPortFree(set_msg);
    return rval;
}

bool bcmp_config_commit(uint64_t target_node_id, bm_common_config_partition_e partition, err_t &err) {
    bool rval = false;
    err = ERR_VAL;
    bm_common_config_commit_t *commit_msg = (bm_common_config_commit_t *)pvPortMalloc(sizeof(bm_common_config_commit_t));
    configASSERT(commit_msg);
    commit_msg->header.target_node_id = target_node_id;
    commit_msg->header.source_node_id = getNodeId();
    commit_msg->partition = partition;
    err = bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_COMMIT, (uint8_t *)commit_msg, sizeof(bm_common_config_commit_t));
    if(err == ERR_OK) {
        rval = true;
    }
    vPortFree(commit_msg);
    return rval;
}

bool bcmp_config_status_request(uint64_t target_node_id, bm_common_config_partition_e partition, err_t &err, bcmp_reply_message_cb reply_cb) {
    bool rval = false;
    err = ERR_VAL;
    bm_common_config_status_request_t *status_req_msg = (bm_common_config_status_request_t *)pvPortMalloc(sizeof(bm_common_config_status_request_t));
    configASSERT(status_req_msg);
    status_req_msg->header.target_node_id = target_node_id;
    status_req_msg->header.source_node_id = getNodeId();
    status_req_msg->partition = partition;
    err = bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_STATUS_REQUEST, (uint8_t *)status_req_msg, sizeof(bm_common_config_status_request_t), 0, reply_cb);
    if(err == ERR_OK) {
        rval = true;
    }
    vPortFree(status_req_msg);
    return rval;
}

bool bcmp_config_status_response(uint64_t target_node_id, bm_common_config_partition_e partition, bool commited, uint8_t num_keys, const ConfigKey_t* keys, err_t &err, uint16_t seq_num) {
    bool rval = false;
    err = ERR_VAL;
    size_t msg_size = sizeof(bm_common_config_status_response_t);
    for(int i = 0; i < num_keys; i++){
        msg_size += sizeof(bm_common_config_status_key_data_t);
        msg_size += keys[i].keyLen;
    }
    if(msg_size > BCMP_MAX_PAYLOAD_SIZE_BYTES) {
        return false;
    }
    bm_common_config_status_response_t *status_resp_msg = (bm_common_config_status_response_t *)pvPortMalloc(msg_size);
    configASSERT(status_resp_msg);
    do {
        status_resp_msg->header.target_node_id = target_node_id;
        status_resp_msg->header.source_node_id = getNodeId();
        status_resp_msg->committed = commited;
        status_resp_msg->partition = partition;
        status_resp_msg->num_keys = num_keys;
        bm_common_config_status_key_data_t* key_data = reinterpret_cast<bm_common_config_status_key_data_t* >(status_resp_msg->keyData);
        for(int i = 0; i < num_keys; i++){
            key_data->key_length = keys[i].keyLen;
            memcpy(key_data->key, keys[i].keyBuffer, keys[i].keyLen);
            key_data += sizeof(bm_common_config_status_key_data_t);
            key_data += keys[i].keyLen;
        }
        err = bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_STATUS_RESPONSE, reinterpret_cast<uint8_t *>(status_resp_msg), msg_size, seq_num);
        if(err == ERR_OK) {
            rval = true;
        }
    } while(0);
    vPortFree(status_resp_msg);
    return rval;
}

static void bcmp_config_process_commit_msg(bm_common_config_commit_t * msg) {
    configASSERT(msg);
    switch(msg->partition) {
        case BM_COMMON_CFG_PARTITION_USER: {
            _usr_cfg->saveConfig(); // Reboot!
            break;
        }
        case BM_COMMON_CFG_PARTITION_SYSTEM: {
            _sys_cfg->saveConfig(); // Reboot!
            break;
        }
        default:
            printf("Invalid partition\n");
            break;
    }
}

static void bcmp_config_process_status_request_msg(bm_common_config_status_request_t * msg, uint16_t seq_num) {
    configASSERT(msg);
    err_t err;
    do {
        Configuration *cfg;
        if(msg->partition == BM_COMMON_CFG_PARTITION_USER){
            cfg = _usr_cfg;
        } else if (msg->partition == BM_COMMON_CFG_PARTITION_SYSTEM){
            cfg = _sys_cfg;
        } else {
            printf("Invalid configuration\n");
            break;
        }
        uint8_t num_keys;
        const ConfigKey_t * keys = cfg->getStoredKeys(num_keys);
        bcmp_config_status_response(msg->header.source_node_id, msg->partition, cfg->needsCommit(),num_keys,keys,err,seq_num);
        if(err != ERR_OK){
            printf("Error processing config status request.\n");
        }
    } while(0);
}

static bool bcmp_config_send_value(uint64_t target_node_id, bm_common_config_partition_e partition,uint32_t data_length, void* data, err_t &err, uint16_t seq_num) {
    bool rval = false;
    err = ERR_VAL;
    size_t msg_len = data_length + sizeof(bm_common_config_value_t);
    bm_common_config_value_t * value_msg = (bm_common_config_value_t * )pvPortMalloc(msg_len);
    configASSERT(value_msg);
    do {
        value_msg->header.target_node_id = target_node_id;
        value_msg->header.source_node_id = getNodeId();
        value_msg->partition = partition;
        value_msg->data_length = data_length;
        memcpy(value_msg->data, data, data_length);
        err = bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_VALUE, (uint8_t *)value_msg, msg_len, seq_num);
        if(err == ERR_OK) {
            rval = true;
        }
    } while(0);
    vPortFree(value_msg);
    return rval;
}

static void bcmp_config_process_config_get_msg(bm_common_config_get_t *msg, uint16_t seq_num){
    configASSERT(msg);
    do {
        Configuration *cfg;
        if(msg->partition == BM_COMMON_CFG_PARTITION_USER){
            cfg = _usr_cfg;
        } else if (msg->partition == BM_COMMON_CFG_PARTITION_SYSTEM){
            cfg = _sys_cfg;
        } else {
            break;
        }
        size_t buffer_len = cfg::MAX_CONFIG_BUFFER_SIZE_BYTES;
        uint8_t * buffer = (uint8_t *) pvPortMalloc(buffer_len);
        configASSERT(buffer);
        if(cfg->getConfigCbor(msg->key,msg->key_length, buffer, buffer_len)){
            err_t err;
            bcmp_config_send_value(msg->header.source_node_id,msg->partition, buffer_len, buffer, err, seq_num);
        }
        vPortFree(buffer);
    } while(0);
}

static void bcmp_config_process_config_set_msg(bm_common_config_set_t *msg, uint16_t seq_num){
    configASSERT(msg);
    do {
        Configuration *cfg;
        if(msg->partition == BM_COMMON_CFG_PARTITION_USER){
            cfg = _usr_cfg;
        } else if (msg->partition == BM_COMMON_CFG_PARTITION_SYSTEM){
            cfg = _sys_cfg;
        } else {
            break;
        }
        if (msg->data_length > cfg::MAX_CONFIG_BUFFER_SIZE_BYTES || msg->data_length == 0) {
            break;
        }
        if(cfg->setConfigCbor(reinterpret_cast<const char *>(msg->keyAndData), msg->key_length, &msg->keyAndData[msg->key_length], msg->data_length)){
            err_t err;
            bcmp_config_send_value(msg->header.source_node_id,msg->partition, msg->data_length, &msg->keyAndData[msg->key_length], err, seq_num);
        }
    } while(0);
}

static void bcmp_process_value_message(bm_common_config_value_t * msg) {
    CborValue it;
    CborParser parser;
    do {
        if(cbor_parser_init(msg->data, msg->data_length, 0, &parser, &it) != CborNoError){
            break;
        }
        if(!cbor_value_is_valid(&it)){
            break;
        }
        ConfigDataTypes_e type;
        if(!Configuration::cborTypeToConfigType(&it,type)){
            break;
        }
        switch(type) {
            case cfg::ConfigDataTypes_e::UINT32:{
                uint64_t temp;
                if(cbor_value_get_uint64(&it,&temp) != CborNoError){
                    break;
                }
                printf("Node Id: %" PRIx64 " Value:%" PRIu32 "\n", msg->header.source_node_id, temp);
                break;
            }
            case cfg::ConfigDataTypes_e::INT32 : {
                int64_t temp;
                if(cbor_value_get_int64(&it,&temp) != CborNoError){
                    break;
                }
                printf("Node Id: %" PRIx64 " Value:%" PRId64 "\n", msg->header.source_node_id, temp);
                break;
            }
            case cfg::ConfigDataTypes_e::FLOAT : {
                float temp;
                if(cbor_value_get_float(&it,&temp) != CborNoError){
                    break;
                }
                printf("Node Id: %" PRIx64 " Value:%f\n", msg->header.source_node_id, temp);
                break;
            }
            case cfg::ConfigDataTypes_e::STR : {
                size_t buffer_len = cfg::MAX_CONFIG_BUFFER_SIZE_BYTES;
                char * buffer = (char *) pvPortMalloc(buffer_len);
                configASSERT(buffer);
                do {
                    if(cbor_value_copy_text_string(&it,buffer, &buffer_len, NULL) != CborNoError){
                        break;
                    }
                    if(buffer_len >= cfg::MAX_CONFIG_BUFFER_SIZE_BYTES){
                        break;
                    }
                    buffer[buffer_len] = '\0';
                    printf("Node Id: %" PRIx64 " Value:%s\n", msg->header.source_node_id, buffer);
                } while(0);
                vPortFree(buffer);
                break;
            }
            case cfg::ConfigDataTypes_e::BYTES: {
                size_t buffer_len = cfg::MAX_CONFIG_BUFFER_SIZE_BYTES;
                uint8_t * buffer = (uint8_t *) pvPortMalloc(buffer_len);
                configASSERT(buffer);
                do {
                    if(cbor_value_copy_byte_string(&it,buffer, &buffer_len, NULL) != CborNoError){
                        break;
                    }
                    printf("Node Id: %" PRIx64 " Value: ", msg->header.source_node_id);
                    for(size_t i  = 0; i < buffer_len; i++) {
                        printf("0x%02x:",buffer[i]);
                        if(i % 8 == 0){
                            printf("\n");
                        }
                    }
                    printf("\n");
                } while(0);
                vPortFree(buffer);
                break;
            }
            case cfg::ConfigDataTypes_e::ARRAY: {
                printf("Node Id: %" PRIx64 " Value: Array\n", msg->header.source_node_id);
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
    } while(0);
}

bool bcmp_config_del_key(uint64_t target_node_id, bm_common_config_partition_e partition, size_t key_len, const char * key, bcmp_reply_message_cb reply_cb) {
    configASSERT(key);
    bool rval = false;
    size_t msg_size = sizeof(bm_common_config_delete_key_request_t) + key_len;
    bm_common_config_delete_key_request_t * del_msg = (bm_common_config_delete_key_request_t * )pvPortMalloc(msg_size);
    configASSERT(del_msg);
    del_msg->header.target_node_id = target_node_id;
    del_msg->header.source_node_id = getNodeId();
    del_msg->partition = partition;
    del_msg->key_length = key_len;
    memcpy(del_msg->key, key, key_len);
    if(bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_DELETE_REQUEST, reinterpret_cast<uint8_t*>(del_msg), msg_size, 0, reply_cb) == ERR_OK) {
        rval = true;
    }
    vPortFree(del_msg);
    return rval;
}

static bool bcmp_config_send_del_key_response(uint64_t target_node_id, bm_common_config_partition_e partition, size_t key_len, const char * key, bool success, uint16_t seq_num) {
    configASSERT(key);
    bool rval = false;
    size_t msg_size = sizeof(bm_common_config_delete_key_response_t) + key_len;
    bm_common_config_delete_key_response_t * del_resp = (bm_common_config_delete_key_response_t * )pvPortMalloc(msg_size);
    configASSERT(del_resp);
    del_resp->header.target_node_id = target_node_id;
    del_resp->header.source_node_id = getNodeId();
    del_resp->partition = partition;
    del_resp->key_length = key_len;
    memcpy(del_resp->key, key, key_len);
    del_resp->success = success;
    if(bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_DELETE_RESPONSE, reinterpret_cast<uint8_t*>(del_resp), msg_size, seq_num) == ERR_OK) {
        rval = true;
    }
    vPortFree(del_resp);
    return rval;
}

static void bcmp_process_del_request_message(bm_common_config_delete_key_request_t * msg, uint16_t seq_num) {
    configASSERT(msg);
    do {
        Configuration *cfg;
        if(msg->partition == BM_COMMON_CFG_PARTITION_USER){
            cfg = _usr_cfg;
        } else if (msg->partition == BM_COMMON_CFG_PARTITION_SYSTEM){
            cfg = _sys_cfg;
        } else {
            break;
        }
        bool success = cfg->removeKey(msg->key,msg->key_length);
        if(!bcmp_config_send_del_key_response(msg->header.source_node_id, msg->partition, msg->key_length, msg->key, success, seq_num)){
            printf("Failed to send del key resp\n");
        }
    } while(0);
}

static void bcmp_process_del_response_message(bm_common_config_delete_key_response_t * msg) {
    configASSERT(msg);
    char * keyprintbuf = (char * )pvPortMalloc(msg->key_length + 1);
    memcpy(keyprintbuf, msg->key, msg->key_length);
    keyprintbuf[msg->key_length] = '\0';
    printf("Node Id:%" PRIx64 " Key Delete Response - Key: %s, Partition: %d, Success %d\n", msg->header.source_node_id,
            keyprintbuf, msg->partition, msg->success);
    vPortFree(keyprintbuf);
}

/*!
    \return true if the caller should forward the message, false if the message was handled
*/
bool bcmp_process_config_message(bcmp_message_type_t bcmp_msg_type, uint8_t *payload, uint16_t seq_num) {
    bool should_forward = false;
    do {
        bm_common_config_header_t * msg_header = reinterpret_cast<bm_common_config_header_t *>(payload);
        if (msg_header->target_node_id != getNodeId()) {
            should_forward = true;
            break;
        }
        switch(bcmp_msg_type){
            case BCMP_CONFIG_GET: {
                bcmp_config_process_config_get_msg(reinterpret_cast<bm_common_config_get_t *>(payload), seq_num);
                break;
            }
            case BCMP_CONFIG_SET: {
                bcmp_config_process_config_set_msg(reinterpret_cast<bm_common_config_set_t *>(payload), seq_num);
                break;
            }
            case BCMP_CONFIG_COMMIT: {
                (void) seq_num;
                bcmp_config_process_commit_msg(reinterpret_cast<bm_common_config_commit_t *>(payload));
                break;
            }
            case BCMP_CONFIG_STATUS_REQUEST: {
                bcmp_config_process_status_request_msg(reinterpret_cast<bm_common_config_status_request_t *>(payload), seq_num);
                break;
            }
            case BCMP_CONFIG_STATUS_RESPONSE: {
                bm_common_config_status_response_t *msg = reinterpret_cast<bm_common_config_status_response_t *>(payload);
                printf("Response msg -- Node Id:%" PRIx64 ",Partition:%d, Commit Status:%d\n",
                    msg->header.source_node_id, msg->partition, msg->committed);
                printf("Num Keys: %d\n",msg->num_keys);
                bm_common_config_status_key_data_t * key = reinterpret_cast<bm_common_config_status_key_data_t*>(msg->keyData);
                for(int i = 0; i < msg->num_keys; i++){
                    char * keybuf = (char *) pvPortMalloc(key->key_length + 1);
                    configASSERT(keybuf);
                    memcpy(keybuf, key->key, key->key_length);
                    keybuf[key->key_length] = '\0';
                    printf("%s\n", keybuf);
                    key += key->key_length + sizeof(bm_common_config_status_key_data_t);
                    vPortFree(keybuf);
                }
                break;
            }
            case BCMP_CONFIG_VALUE: {
                (void) seq_num;
                bm_common_config_value_t *msg = reinterpret_cast<bm_common_config_value_t *>(payload);
                bcmp_process_value_message(msg);
                break;
            }
            case BCMP_CONFIG_DELETE_REQUEST: {
                bm_common_config_delete_key_request_t *msg = reinterpret_cast<bm_common_config_delete_key_request_t *>(payload);
                bcmp_process_del_request_message(msg, seq_num);
                break;
            }
            case BCMP_CONFIG_DELETE_RESPONSE: {
                (void) seq_num;
                bm_common_config_delete_key_response_t *msg = reinterpret_cast<bm_common_config_delete_key_response_t *>(payload);
                bcmp_process_del_response_message(msg);
                break;
            }
            default:
                (void) seq_num;
                printf("Invalid config msg\n");
                break;
        }
    } while(0);

    return should_forward;
}

void bcmp_config_init(Configuration* user_cfg, Configuration* sys_cfg) {
    configASSERT(user_cfg);
    configASSERT(sys_cfg);
    _usr_cfg = user_cfg;
    _sys_cfg = sys_cfg;
}