#ifndef __BM_CONFIG_H__
#define __BM_CONFIG_H__

#include <stdio.h>

#define ADIN_PORT_MASK_1 (1 << ADIN2111_PORT_1)
#define ADIN_PORT_MASK_2 (1 << ADIN2111_PORT_2)
#define ADIN_PORT_MASK_ALL (ADIN_PORT_MASK_1 | ADIN_PORT_MASK_2)

#define bm_app_name APP_NAME

#define bm_debug(format, ...) printf(format, ##__VA_ARGS__)

#endif /* __BM_CONFIG_H__ */
