#include "bm_os.h"
#include <stdio.h>
#include <string.h>

void *__wrap_malloc(size_t size) { return bm_malloc(size); }

void *__wrap_calloc(size_t num, size_t size) {
  void *ret = bm_malloc(num * size);

  if (ret) {
    memset(ret, 0, num * size);
  }

  return ret;
}

void __wrap_free(void *p) {
  if (p) {
    bm_free(p);
  }
}
