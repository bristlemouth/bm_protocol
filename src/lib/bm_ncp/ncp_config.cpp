#include "ncp_config.h"
#include "bcmp_config.h"
#include "device_info.h"
#include "FreeRTOS.h"
#include "bm_serial.h"

using namespace cfg;

static Configuration *_usr_cfg;
static Configuration *_sys_cfg;
static Configuration *_hw_cfg;

// TODO: BCMP downstream passthrough to be done later. 

static Configuration* get_partition(bm_common_config_partition_e partition);
static uint8_t* alloc_ncp_key_buffer(uint8_t num_keys, const ConfigKey_t* keys, size_t& len);

bool ncp_cfg_get_cb(uint64_t node_id, bm_common_config_partition_e partition, size_t key_len, const char* key) {
    bool rval = false;
    uint8_t * valueBuf = reinterpret_cast<uint8_t*>(pvPortMalloc(MAX_CONFIG_BUFFER_SIZE_BYTES));
    configASSERT(valueBuf);
    do {
        Configuration* p = get_partition(partition);
        if(!p) {
            printf("Invalid partition\n.");
            break;
        }
        size_t value_len = MAX_CONFIG_BUFFER_SIZE_BYTES;
        if(!p->getConfigCbor(key, key_len, valueBuf, value_len)){
            printf("Failed to get config.\n");
            break;
        }
        if(bm_serial_cfg_value(node_id, partition, value_len, valueBuf) != BM_SERIAL_OK) {
            printf("Failed to send cfg\n");
            break;
        }
        rval = true;
    } while(0);
    vPortFree(valueBuf);
    return rval;
}

bool ncp_cfg_set_cb(uint64_t node_id, bm_common_config_partition_e partition,
    size_t key_len, const char* key, size_t value_size, void * val) {
    bool rval = false;
    do {
        Configuration* p = get_partition(partition);
        if(!p) {
            printf("Invalid partition\n.");
            break;
        }
        if(!p->setConfigCbor(key, key_len, reinterpret_cast<uint8_t*>(val), value_size)){
            printf("Failed to set Config\n");
            break;
        }
        if(bm_serial_cfg_value(node_id, partition, value_size, reinterpret_cast<uint8_t*>(val)) != BM_SERIAL_OK) {
            printf("Failed to send cfg\n");
            break;
        }
        rval = true;
    } while(0);
    return rval;
}

bool ncp_cfg_commit_cb(uint64_t node_id, bm_common_config_partition_e partition) {
    (void) node_id;
    bool rval = false;
    do {
        Configuration* p = get_partition(partition);
        if(!p) {
            printf("Invalid partition\n.");
            break;
        }
        if(!p->saveConfig()){
            printf("Failed to save config!\n");
            break;
        }
        rval = true;
    } while(0);
    return rval;
}

bool ncp_cfg_status_request_cb(uint64_t node_id, bm_common_config_partition_e partition) {
    bool rval = false;
    do {
        Configuration* p = get_partition(partition);
        if(!p) {
            printf("Invalid partition\n.");
            break;
        }
        uint8_t num_keys;
        const ConfigKey_t * keys = p->getStoredKeys(num_keys);
        size_t buffer_size;
        uint8_t * keyBuf = (num_keys) ? alloc_ncp_key_buffer(num_keys, keys, buffer_size): NULL;
        do {
            if(bm_serial_cfg_status_response(node_id, partition, p->needsCommit(), num_keys, keyBuf) != BM_SERIAL_OK){
                printf("Failed to send status resp\n");
                break;
            }
            rval = true;
       } while(0);
       vPortFree(keyBuf);
    } while(0);
    return rval;
}

bool ncp_cfg_key_del_request_cb(uint64_t node_id, bm_common_config_partition_e partition, size_t key_len, const char * key) {
    bool rval = true;
    do {
        Configuration* p = get_partition(partition);
        if(!p) {
            printf("Invalid partition\n.");
            break;
        }
        bool success = true;
        if(!p->removeKey(key, key_len)){
            success = false;
        }
        if(bm_serial_cfg_delete_response(node_id, partition, key_len, key, success) != BM_SERIAL_OK) {
            printf("failed to send key del response\n");
            break;
        }
        rval = true;
    } while(0);
    return rval;
}

void ncp_cfg_init(Configuration *usr_cfg, Configuration *sys_cfg, Configuration *hw_cfg) {
    configASSERT(usr_cfg);
    configASSERT(sys_cfg);
    configASSERT(hw_cfg);
    _usr_cfg = usr_cfg;
    _sys_cfg = sys_cfg;
    _hw_cfg = hw_cfg;
}

static Configuration* get_partition(bm_common_config_partition_e partition) {
    Configuration* part = NULL;
    if(partition == BM_COMMON_CFG_PARTITION_USER) {
        part = _usr_cfg;
    } else if (partition == BM_COMMON_CFG_PARTITION_SYSTEM){
        part = _sys_cfg;
    } else if (partition == BM_COMMON_CFG_PARTITION_HARDWARE) {
        part = _hw_cfg;
    }
    return part;
}

/* NOTE: Caller must free allocated buffer */
static uint8_t* alloc_ncp_key_buffer(uint8_t num_keys, const ConfigKey_t* keys, size_t& len) {
    len = 0;
    for(int i = 0; i < num_keys; i++){
        len += sizeof(bm_common_config_status_key_data_t);
        len += keys[i].keyLen;
    }
    uint8_t* buf = reinterpret_cast<uint8_t*>(pvPortMalloc(len));
    configASSERT(buf);
    bm_common_config_status_key_data_t* key_data = reinterpret_cast<bm_common_config_status_key_data_t* >(buf);
    for(int i = 0; i < num_keys; i++){
        key_data->key_length = keys[i].keyLen;
        memcpy(key_data->key, keys[i].keyBuffer, keys[i].keyLen);
        key_data += sizeof(bm_common_config_status_key_data_t);
        key_data += keys[i].keyLen;
    }
    return buf;
}

