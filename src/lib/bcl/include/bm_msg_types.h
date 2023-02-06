#ifndef BM_MSG_TYPES_H__
#define BM_MSG_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

enum {
    MSG_BM_DISCOVER_NEIGHBORS,
    MSG_BM_NEIGHBOR_INFO,
    MSG_BM_TIME,
    MSG_BM_TABLE_RESPONSE,
    MSG_BM_DURATION,
    MSG_BM_ACK,
    MSG_BM_HEADER,
    MSG_BM_HEARTBEAT,
    MSG_BM_REQUEST_TABLE,
    MSG_BM_TEMPERATURE,
};

#endif /* BM_MSG_TYPES_H__ */