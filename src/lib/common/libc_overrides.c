#include "FreeRTOS.h"

// see link for more info:
// https://stackoverflow.com/questions/920500/what-is-the-purpose-of-cxa-pure-virtual
void __cxa_pure_virtual(){
	configASSERT(0);
}
