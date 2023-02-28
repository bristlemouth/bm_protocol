#include <stdint.h>
#include "serial.h"
#ifdef __cplusplus
extern "C" {
#endif

void pcapInit(SerialHandle_t *handle);
void pcapTxPacket(const uint8_t *buff, size_t len);
void pcapEnable();
void pcapDisable();

#ifdef __cplusplus
}
#endif
