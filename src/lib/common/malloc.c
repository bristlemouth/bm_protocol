#include "bm_os.h"
#include <stdio.h>

void *__wrap_malloc(size_t size) { return bm_malloc(size); }

void __wrap_free(void *p) {
  if (p) {
    bm_free(p);
  }
}
