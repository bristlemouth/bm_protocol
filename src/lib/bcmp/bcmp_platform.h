#pragma once

// In case they're not yet defined
#include <float.h>
#include <stdbool.h>
#include <stdint.h>

// C-callable functions
#ifdef __cplusplus
extern "C" {
#endif


// Example of a C-callable functions from the note-c library
// Card callback functions
typedef void * (*mallocFn) (size_t size);
typedef void (*freeFn) (void *);
typedef void (*delayMsFn) (uint32_t ms);
typedef uint32_t (*getMsFn) (void);

// Set the functions that will be used for memory allocation, free, delay, and millis
void BcmpSetFnDefault(mallocFn mallocfn, freeFn freefn, delayMsFn delayfn, getMsFn millisfn);
void BcmpSetFn(mallocFn mallocfn, freeFn freefn, delayMsFn delayfn, getMsFn millisfn);

// Why do we need to define these functions??? seems a bit redundant
void *BcmpMalloc(size_t size);
void BcmpFree(void *);
long unsigned int BcmpGetMs(void);
void BcmpDelayMs(uint32_t ms);

// End of C-callable functions
#ifdef __cplusplus
}
#endif