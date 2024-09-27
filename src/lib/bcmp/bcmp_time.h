#pragma once
#include <stdint.h>

// #include "bcmp_messages.h"

bool bcmp_time_set_time(uint64_t target_node_id, uint64_t utc_us);
bool bcmp_time_get_time(uint64_t target_node_id);

// bool bcmp_time_process_time_message(bcmp_message_type_t bcmp_msg_type, uint8_t* payload);
