#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "serial.h"
#include "stream_buffer.h"
#include "task.h"
#include <stdio.h>

static SerialHandle_t *pcapSerialHandle;
static volatile bool firstMessage = false;

// See link for header/struct definitions: https://wiki.wireshark.org/Development/LibpcapFileFormat
typedef struct {
  uint32_t magic_number;
  uint16_t version_major;
  uint16_t version_minor;
  int32_t thiszone;
  uint32_t sigfigs;
  uint32_t snaplen;
  uint32_t network;
} PcapHeader_t;

typedef struct {
  uint32_t ts_sec;
  uint32_t ts_usec;
  uint32_t incl_len;
  uint32_t orig_len;
} PcapRecordHeader_t;

static const PcapHeader_t pcapHeader = {
    .magic_number=0xA1B2C3D4,
    .version_major=2,
    .version_minor=4,
    .thiszone=0,
    .sigfigs=0,
    .snaplen=65535,
    .network=1,
};

/*!
  Initialize pcap stream variables

  \param[in] *handle - Serial handle to use for pcap streams
  \return none
*/
void pcapInit(SerialHandle_t *handle) {
  configASSERT(handle);

  pcapSerialHandle = handle;
  // pcapSerialHandle->processByte = NULL;

  // Single byte trigger level for fast response time
  pcapSerialHandle->txStreamBuffer = xStreamBufferCreate(pcapSerialHandle->txBufferSize, 1);
  configASSERT(pcapSerialHandle->txStreamBuffer != NULL);

  pcapSerialHandle->rxStreamBuffer = xStreamBufferCreate(pcapSerialHandle->rxBufferSize, 1);
  configASSERT(pcapSerialHandle->rxStreamBuffer != NULL);

}

/*!
  Send packet over pcap usb port

  \param[in] buff pointer to packet
  \param[in] len Number of bytes to transmit
  \return none
*/
void pcapTxPacket(const uint8_t *buff, size_t len) {
  if(pcapSerialHandle && pcapSerialHandle->enabled) {
    PcapRecordHeader_t header;

    header.ts_sec = xTaskGetTickCount()/1000;
    header.ts_usec = (xTaskGetTickCount() - (header.ts_sec * 1000)) * 1000;
    header.incl_len = len;
    header.orig_len = len;

    if(!firstMessage) {
      // Tx pcap header
      firstMessage = true;
      serialWrite(pcapSerialHandle, (uint8_t *)&pcapHeader, sizeof(pcapHeader));
    }

    serialWrite(pcapSerialHandle, (uint8_t *)&header, sizeof(header));

    serialWrite(pcapSerialHandle, (uint8_t *)buff, len);
  }
}

/*!
  Enable pcap stream (automatically called when host connects over USB)
*/
void pcapEnable() {
  if(pcapSerialHandle) {
    pcapSerialHandle->enabled = true;
    xStreamBufferReset(pcapSerialHandle->rxStreamBuffer);
    xStreamBufferReset(pcapSerialHandle->txStreamBuffer);
    printf("PCAP Stream enabled!\n");
    firstMessage = false;
  }
}

/*!
  Disable pcapSerialHandle serial device. (automatically called when host disconnects over USB)
*/
void pcapDisable() {
  if(pcapSerialHandle) {
    pcapSerialHandle->enabled = false;
    printf("PCAP Stream disabled!\n");
  }
}
