#include "bcmp_config.h"
#include "FreeRTOS.h"
#include "device_info.h"

static Configuration* _usr_cfg;
static Configuration* _sys_cfg;

bool bcmp_config_get(uint64_t target_node_id, bcmp_config_partition_e partition, size_t key_len, const char* key, err_t &err) {
    configASSERT(key);
    bool rval = false;
    err = ERR_VAL;
    size_t msg_size = sizeof(bcmp_config_get_t) + key_len;
    bcmp_config_get_t * get_msg = (bcmp_config_get_t * )pvPortMalloc(msg_size);
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
        err = bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_GET, (uint8_t *)get_msg, msg_size);
        if(err == ERR_OK) {
            rval = true;
        }
    } while(0);
    vPortFree(get_msg);
    return rval;
}

bool bcmp_config_set(uint64_t target_node_id, bcmp_config_partition_e partition,
    size_t key_len, const char* key, size_t value_size, void * val, err_t &err) {
    configASSERT(key);
    bool rval = false;
    err = ERR_VAL;
    size_t msg_len = sizeof(bcmp_config_set_t) + key_len + value_size;
    bcmp_config_set_t * set_msg = (bcmp_config_set_t *)pvPortMalloc(msg_len);
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
        err = bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_SET, (uint8_t *)set_msg, msg_len);
        if(err == ERR_OK) {
            rval = true;
        }
    } while(0);
    vPortFree(set_msg);
    return rval;
}

bool bcmp_config_commit(uint64_t target_node_id, bcmp_config_partition_e partition, err_t &err) {
    bool rval = false;
    err = ERR_VAL;
    bcmp_config_commit_t *commit_msg = (bcmp_config_commit_t *)pvPortMalloc(sizeof(bcmp_config_commit_t));
    configASSERT(commit_msg);
    commit_msg->header.target_node_id = target_node_id;
    commit_msg->header.source_node_id = getNodeId();
    commit_msg->partition = partition;
    err = bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_COMMIT, (uint8_t *)commit_msg, sizeof(bcmp_config_commit_t));
    if(err == ERR_OK) {
        rval = true;
    }
    vPortFree(commit_msg);
    return rval;
}

bool bcmp_config_status_request(uint64_t target_node_id, bcmp_config_partition_e partition, err_t &err) {
    bool rval = false;
    err = ERR_VAL;
    bcmp_config_status_request_t *status_req_msg = (bcmp_config_status_request_t *)pvPortMalloc(sizeof(bcmp_config_status_request_t));
    configASSERT(status_req_msg);
    status_req_msg->header.target_node_id = target_node_id;
    status_req_msg->header.source_node_id = getNodeId();
    status_req_msg->partition = partition;
    err = bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_STATUS_REQUEST, (uint8_t *)status_req_msg, sizeof(bcmp_config_status_request_t));
    if(err == ERR_OK) {
        rval = true;
    }
    vPortFree(status_req_msg);
    return rval;
}

bool bcmp_config_status_response(uint64_t target_node_id, bcmp_config_partition_e partition, bool commited, err_t &err) {
    bool rval = false;
    err = ERR_VAL;
    bcmp_config_status_response_t *status_resp_msg = (bcmp_config_status_response_t *)pvPortMalloc(sizeof(bcmp_config_status_response_t));
    configASSERT(status_resp_msg);
    do {
        status_resp_msg->header.target_node_id = target_node_id;
        status_resp_msg->header.source_node_id = getNodeId();
        status_resp_msg->committed = commited;
        status_resp_msg->partition = partition;
        err = bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_STATUS_RESPONSE, (uint8_t *)status_resp_msg, sizeof(bcmp_config_status_response_t));
        if(err == ERR_OK) {
            rval = true;
        }
    } while(0);
    vPortFree(status_resp_msg);
    return rval;
}

static void bcmp_config_process_commit_msg(bcmp_config_commit_t * msg) {
    configASSERT(msg);
    switch(msg->partition) {
        case BCMP_CFG_PARTITION_USER: {
            _usr_cfg->saveConfig(); // Reboot!
            break;
        } 
        case BCMP_CFG_PARTITION_SYSTEM: {
            _sys_cfg->saveConfig(); // Reboot!
            break;
        }
        default:
            printf("Invalid partition\n");
            break;
    }
}

static void bcmp_config_process_status_request_msg(bcmp_config_status_request_t * msg) {
    configASSERT(msg);
    err_t err;
    switch(msg->partition) {
        case BCMP_CFG_PARTITION_USER: {
            bcmp_config_status_response(msg->header.source_node_id,BCMP_CFG_PARTITION_USER, _usr_cfg->needsCommit(), err);
            if(err != ERR_OK){
                printf("Error processing config status request.\n");
            }
            break;
        } 
        case BCMP_CFG_PARTITION_SYSTEM: {
            bcmp_config_status_response(msg->header.source_node_id,BCMP_CFG_PARTITION_SYSTEM, _sys_cfg->needsCommit(), err);
            if(err != ERR_OK){
                printf("Error processing config status request.\n");
            }
            break;
        }
        default:
            printf("Invalid partition\n");
            break;
    }
}

static bool bcmp_config_send_value(uint64_t target_node_id, bcmp_config_partition_e partition,uint32_t data_length, void* data, err_t &err) {
    bool rval = false;
    err = ERR_VAL;
    size_t msg_len = data_length + sizeof(bcmp_config_value_t);
    bcmp_config_value_t * value_msg = (bcmp_config_value_t * )pvPortMalloc(msg_len);
    configASSERT(value_msg);
    do {
        value_msg->header.target_node_id = target_node_id;
        value_msg->header.source_node_id = getNodeId();
        value_msg->partition = partition;
        value_msg->data_length = data_length;
        memcpy(value_msg->data, data, data_length);
        err = bcmp_tx(&multicast_ll_addr, BCMP_CONFIG_VALUE, (uint8_t *)value_msg, msg_len);
        if(err == ERR_OK) {
            rval = true;
        }
    } while(0);
    vPortFree(value_msg);
    return rval;
}

static void bcmp_config_process_config_get_msg(bcmp_config_get_t *msg){
    configASSERT(msg);
    do {
        Configuration *cfg;
        if(msg->partition == BCMP_CFG_PARTITION_USER){
            cfg = _usr_cfg;
        } else if (msg->partition == BCMP_CFG_PARTITION_SYSTEM){
            cfg = _sys_cfg;
        } else {
            break;
        }
        size_t buffer_len = cfg::MAX_STR_LEN_BYTES;
        uint8_t * buffer = (uint8_t *) pvPortMalloc(buffer_len);
        configASSERT(buffer);
        if(cfg->getConfigCbor(msg->key,msg->key_length, buffer, buffer_len)){
            err_t err;
            bcmp_config_send_value(msg->header.source_node_id,msg->partition, buffer_len, buffer, err);
        }
        vPortFree(buffer);
    } while(0);
}

static void bcmp_config_process_config_set_msg(bcmp_config_set_t *msg){
    configASSERT(msg);
    do {
        Configuration *cfg;
        if(msg->partition == BCMP_CFG_PARTITION_USER){
            cfg = _usr_cfg;
        } else if (msg->partition == BCMP_CFG_PARTITION_SYSTEM){
            cfg = _sys_cfg;
        } else {
            break;
        }
        if (msg->data_length > cfg::MAX_STR_LEN_BYTES || msg->data_length == 0) {
            break;
        }
        if(cfg->setConfigCbor(reinterpret_cast<const char *>(msg->keyAndData), msg->key_length, &msg->keyAndData[msg->key_length], msg->data_length)){
            err_t err;
            bcmp_config_send_value(msg->header.source_node_id,msg->partition, msg->data_length, &msg->keyAndData[msg->key_length], err);
        }
    } while(0);
}

static void bcmp_process_value_message(bcmp_config_value_t * msg) {
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
                size_t buffer_len = cfg::MAX_STR_LEN_BYTES;
                char * buffer = (char *) pvPortMalloc(buffer_len);
                configASSERT(buffer);
                do {
                    if(cbor_value_copy_text_string(&it,buffer, &buffer_len, NULL) != CborNoError){
                        break;
                    }
                    if(buffer_len >= cfg::MAX_STR_LEN_BYTES){
                        break;
                    }
                    buffer[buffer_len] = '\0';
                    printf("Node Id: %" PRIx64 " Value:%s\n", msg->header.source_node_id, buffer);
                } while(0);
                vPortFree(buffer);
                break;
            }
            case cfg::ConfigDataTypes_e::BYTES: {
                size_t buffer_len = cfg::MAX_STR_LEN_BYTES;
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
        }
    } while(0);
}

void bcmp_process_config_message(bcmp_message_type_t bcmp_msg_type, uint8_t* payload) {
    do {
        bcmp_config_header_t * msg_header = reinterpret_cast<bcmp_config_header_t *>(payload);
        if(msg_header->target_node_id != getNodeId()){
            break;
        }
        switch(bcmp_msg_type){
            case BCMP_CONFIG_GET: {
                bcmp_config_process_config_get_msg(reinterpret_cast<bcmp_config_get_t *>(payload));
                break;
            }
            case BCMP_CONFIG_SET: {
                bcmp_config_process_config_set_msg(reinterpret_cast<bcmp_config_set_t *>(payload));
                break;
            }
            case BCMP_CONFIG_COMMIT: {
                bcmp_config_process_commit_msg(reinterpret_cast<bcmp_config_commit_t *>(payload));
                break;
            }
            case BCMP_CONFIG_STATUS_REQUEST: {
                bcmp_config_process_status_request_msg(reinterpret_cast<bcmp_config_status_request_t *>(payload));
                break;
            }
            case BCMP_CONFIG_STATUS_RESPONSE: {
                bcmp_config_status_response_t *msg = reinterpret_cast<bcmp_config_status_response_t *>(payload);
                printf("Response msg -- Node Id:%" PRIu64 ",Partition:%d, Commit Status:%d\n",
                    msg->header.source_node_id, msg->partition, msg->committed);
                break;
            }
            case BCMP_CONFIG_VALUE: {
                bcmp_config_value_t *msg = reinterpret_cast<bcmp_config_value_t *>(payload);
                bcmp_process_value_message(msg);
                break;
            }
            default:
                printf("Invalid config msg\n");
                break;
        }
    } while(0);
}

void bcmp_config_init(Configuration* user_cfg, Configuration* sys_cfg) {
    configASSERT(user_cfg);
    configASSERT(sys_cfg);
    _usr_cfg = user_cfg;
    _sys_cfg = sys_cfg;
}
