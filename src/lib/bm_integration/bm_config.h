#ifndef __BM_CONFIG_H__
#define __BM_CONFIG_H__

#include <stdio.h>

#define bm_app_name APP_NAME

#define bm_debug(format, ...) printf(format, ##__VA_ARGS__)

#define bm_noinit_ram_attribute section(".noinit")

#endif /* __BM_CONFIG_H__ */
