#include "bm_caps.h"
#include "bsp.h"
#include "debug.h"
#include "bm_pubsub.h"
#include "zcbor_common.h"
#include "zcbor_decode.h"
#include "zcbor_encode.h"

/*!
  Get capabilities in CBOR format.

  \param[in] *buff - buffer to store CBOR data in
  \param[in/out] *buffSize - buffer size at input, len of CBOR data as output
  \return true if successful, false otherwise
*/
bool bm_caps_get_cbor(uint8_t *buff, uint32_t *buffSize) {
  configASSERT(buff);
  configASSERT(buffSize);

  /* Create zcbor state variable for encoding. */
  ZCBOR_STATE_E(encodingState, 0, buff, *buffSize, 0);

  bool success = false;

  do {
    // Start main info map
    if(!zcbor_map_start_encode(encodingState, 16)) {
      break;
    }

    if(!zcbor_tstr_put_lit(encodingState, "publications")) {
      break;
    }
    if(!zcbor_tstr_put_term(encodingState, bm_pubsub_get_pubs())) {
      break;
    }

    if(!zcbor_tstr_put_lit(encodingState, "subscriptions")) {
      break;
    }
    char * subscriptions = bm_pubsub_get_subs();
    if(subscriptions){
      if(!zcbor_tstr_put_term(encodingState, subscriptions)) {
        vPortFree(subscriptions);
        break;
      }
      vPortFree(subscriptions);
    } else {
      break;
    }

    // Close main info map
    if(!zcbor_map_end_encode(encodingState, 16)) {
      break;
    }

    success = true;
  } while(0);

  if (!success) {
    printf("Encoding failed: %d\r\n", zcbor_peek_error(encodingState));
    *buffSize = 0;
  } else {
    *buffSize = MIN(*buffSize, (size_t)encodingState[0].payload - (size_t)buff);
  }

  return success;
}

/*!
  Print pubsub capabilities from CBOR data

  \param[in] *buff - buffer with CBOR data
  \param[in] *buffSize - len of CBOR data
  \return true if successful, false otherwise
*/
bool bm_caps_print_from_cbor(const uint8_t *buff, uint32_t buffSize) {
  ZCBOR_STATE_D	(decodingState, 2, buff, buffSize, 1);
  bool success = false;
  do {

    if(!zcbor_map_start_decode(decodingState)) {
      break;
    }

    struct zcbor_string zcStr;
    if(!zcbor_tstr_expect_lit(decodingState, "publications")) {
      break;
    }
    if(!zcbor_tstr_decode(decodingState, &zcStr)) {
      break;
    }
    printf("Publications: %.*s\n", (int)zcStr.len, zcStr.value);

    if(!zcbor_tstr_expect_lit(decodingState, "subscriptions")) {
      break;
    }
    if(!zcbor_tstr_decode(decodingState, &zcStr)) {
      break;
    }
    printf("Subscriptions: %.*s\n", (int)zcStr.len, zcStr.value);

    if(!zcbor_map_end_decode(decodingState)) {
      break;
    }

    success = true;
  } while (0);

  return success;
}
