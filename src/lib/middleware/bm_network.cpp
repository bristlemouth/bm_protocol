#include "bm_network.h"
#include "bm_pubsub.h"
#include <string.h>
#include "bm_serial.h"
#include <stdlib.h>
#include "FreeRTOS.h"

// FIXME: Add to shared message definition.
typedef struct {
    // Network type to send over.
    bm_serial_network_type_e type;
    // Data
    uint8_t data[0];
} __attribute__((packed)) bm_serial_network_data_header_t;

static const char networkTopic[] = "spotter/transmit-data";
static constexpr uint8_t networkTopicType = 1;

/*! \brief Transmit data via the Spotter's satellite or cellular connection.
 *
 * Currently, there are 2 notable limitations on this function.
 * First, only network type `BM_NETWORK_TYPE_CELLULAR_IRI_FALLBACK` shows up in the Sofar Ocean API.
 * Second, the `data_len` has a realistic max of 311 bytes because
 * Iridium messages are limited to 340 bytes, and Spotter adds a 29-byte header.
 *
 * \param data Pointer to the data to transmit.
 * \param data_len Length of the data to transmit. Max: 311.
 * \param type Network type to send over. MUST be BM_NETWORK_TYPE_CELLULAR_IRI_FALLBACK.
 * \return True if the data was successfully queued for transmission.
 */
bool spotter_tx_data(const void* data, uint16_t data_len, bm_serial_network_type_e type) {
    bool rval = false;
    size_t msg_len = sizeof(bm_serial_network_data_header_t) + data_len;
    uint8_t * data_buf = static_cast<uint8_t*>(pvPortMalloc(msg_len));
    configASSERT(data_buf);
    bm_serial_network_data_header_t * header = reinterpret_cast<bm_serial_network_data_header_t *>(data_buf);
    header->type = type;
    memcpy(header->data, data, data_len);
    rval = bm_pub(networkTopic, data_buf, msg_len, networkTopicType);
    vPortFree(data_buf);
    return rval;
}
