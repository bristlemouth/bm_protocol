#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "bcmp_platform.h"

// Example hooks from the note-c library
// Externalized Hooks
//**************************************************************************/
/*!
  @brief  Hook for the calling platform's memory allocation function.
*/
/**************************************************************************/
mallocFn hookMalloc = NULL;
//**************************************************************************/
/*!
  @brief  Hook for the calling platform's memory free function.
*/
/**************************************************************************/
freeFn hookFree = NULL;
//**************************************************************************/
/*!
  @brief  Hook for the calling platform's delay function.
*/
/**************************************************************************/
delayMsFn hookDelayMs = NULL;
//**************************************************************************/
/*!
  @brief  Hook for the calling platform's millis timing function.
*/
/**************************************************************************/
getMsFn hookGetMs = NULL;

//**************************************************************************/
/*!
  @brief  Set the default memory and timing hooks if they aren't already set
  @param   mallocfn  The default memory allocation `malloc`
  function to use.
  @param   freefn  The default memory free
  function to use.
  @param   delayfn  The default delay function to use.
  @param   millisfn  The default 'millis' function to use.
*/
/**************************************************************************/
void BcmpSetFnDefault(mallocFn mallocfn, freeFn freefn, delayMsFn delayfn, getMsFn millisfn)
{
    if (hookMalloc == NULL) {
        hookMalloc = mallocfn;
    }
    if (hookFree == NULL) {
        hookFree = freefn;
    }
    if (hookDelayMs == NULL) {
        hookDelayMs = delayfn;
    }
    if (hookGetMs == NULL) {
        hookGetMs = millisfn;
    }
}

//**************************************************************************/
/*!
  @brief  Set the platform-specific memory and timing hooks.
  @param   mallocfn  The platform-specific memory allocation `malloc`
  function to use.
  @param   freefn  The platform-specific memory free
  function to use.
  @param   delayfn  The platform-specific delay function to use.
  @param   millisfn  The platform-specific 'millis' function to use.
*/
/**************************************************************************/
void BcmpSetFn(mallocFn mallocfn, freeFn freefn, delayMsFn delayfn, getMsFn millisfn)
{
    hookMalloc = mallocfn;
    hookFree = freefn;
    hookDelayMs = delayfn;
    hookGetMs = millisfn;
}

//**************************************************************************/
/*!
  @brief  Allocate a memory chunk using the platform-specific hook.
  @param   size the number of bytes to allocate.
*/
/**************************************************************************/
void *BcmpMalloc(size_t size)
{
    if (hookMalloc == NULL) {
        return NULL;
    }
#if Bcmp_SHOW_MALLOC
    return malloc_show(size);
#else
    return hookMalloc(size);
#endif
}

//**************************************************************************/
/*!
  @brief  Free memory using the platform-specific hook.
  @param   p A pointer to the memory address to free.
*/
/**************************************************************************/
void BcmpFree(void *p)
{
    if (hookFree != NULL) {
#if Bcmp_SHOW_MALLOC
        char str[16];
        htoa32((uint32_t)p, str);
        hookDebugOutput("free");
        hookDebugOutput(str);
#endif
        hookFree(p);
    }
}

//**************************************************************************/
/*!
  @brief  Get the current milliseconds value from the platform-specific
  hook.
  @returns  The current milliseconds value.
*/
/**************************************************************************/
long unsigned int BcmpGetMs()
{
    if (hookGetMs == NULL) {
        return 0;
    }
    return hookGetMs();
}

//**************************************************************************/
/*!
  @brief  Delay milliseconds using the platform-specific hook.
  @param   ms the milliseconds delay value.
*/
/**************************************************************************/
void BcmpDelayMs(uint32_t ms)
{
    if (hookDelayMs != NULL) {
        hookDelayMs(ms);
    }
}