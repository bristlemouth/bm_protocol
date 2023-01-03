#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

void* aligned_malloc(size_t align, size_t size);
void aligned_free(void* ptr);

#ifdef __cplusplus
}
#endif
