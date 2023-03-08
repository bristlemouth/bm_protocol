#include "bm_info.h"
#include "bsp.h"
#include "debug.h"
#include "device_info.h"
#include "reset_reason.h"
#include "zcbor_common.h"
#include "zcbor_decode.h"
#include "zcbor_encode.h"

/*!
  Get device information in CBOR format. (Similar to `info` command locally)

  \param[in] *buff - buffer to store CBOR data in
  \param[in/out] *buffSize - buffer size at input, len of CBOR data as output
  \return true if successful, false otherwise
*/
bool bm_info_get_cbor(uint8_t *buff, uint32_t *buffSize) {
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

    if(!zcbor_tstr_put_lit(encodingState, "version")) {
      break;
    }
    if(!zcbor_tstr_put_term(encodingState, getFWVersionStr())) {
      break;
    }

    if(!zcbor_tstr_put_lit(encodingState, "APP_NAME")) {
      break;
    }
    if(!zcbor_tstr_put_lit(encodingState, APP_NAME)) {
      break;
    }

    if(!zcbor_tstr_put_lit(encodingState, "BSP")) {
      break;
    }
    if(!zcbor_tstr_put_lit(encodingState, BSP_NAME)) {
      break;
    }

    if(!zcbor_tstr_put_lit(encodingState, "SHA")) {
      break;
    }
    if(!zcbor_uint32_put(encodingState, getGitSHA())) {
      break;
    }

    if(!zcbor_tstr_put_lit(encodingState, "reset-reason")) {
      break;
    }
    if(!zcbor_tstr_put_term(encodingState, getResetReasonString())) {
      break;
    }

    const uint8_t *buildId;
    size_t buildIdLen = getBuildId(&buildId);
    if(!zcbor_tstr_put_lit(encodingState, "build-id")) {
      break;
    }
    if(!zcbor_bstr_encode_ptr(encodingState, (const char *)buildId, buildIdLen)) {
      break;
    }

    if(!zcbor_tstr_put_lit(encodingState, "bootloader")) {
      break;
    }
#ifdef USE_BOOTLOADER
    const versionInfo_t *bootloaderInfo = findVersionInfo(FLASH_START, BOOTLOADER_SIZE);

    if(bootloaderInfo) {
      // Start bootloader info map
      if(!zcbor_map_start_encode(encodingState, 8)) {
        break;
      }

      if(!zcbor_tstr_put_lit(encodingState, "version")) {
        break;
      }
      if(!zcbor_tstr_put_term(encodingState, bootloaderInfo->versionStr)) {
        break;
      }

      if(!zcbor_tstr_put_lit(encodingState, "eng")) {
        break;
      }
      if(!zcbor_bool_put(encodingState, fwIsEng(bootloaderInfo))) {
        break;
      }

      if(!zcbor_tstr_put_lit(encodingState, "dirty")) {
        break;
      }
      if(!zcbor_bool_put(encodingState, fwIsDirty(bootloaderInfo))) {
        break;
      }

      if(!zcbor_tstr_put_lit(encodingState, "SHA")) {
        break;
      }
      if(!zcbor_uint32_put(encodingState, bootloaderInfo->gitSHA)) {
        break;
      }

      if(!zcbor_tstr_put_lit(encodingState, "signature")) {
        break;
      }
      if(!zcbor_bool_put(encodingState, (bootloaderInfo->flags >> VER_SIGNATURE_SUPPORT_OFFSET) & 0x1)) {
        break;
      }

      if(!zcbor_tstr_put_lit(encodingState, "encryption")) {
        break;
      }
      if(!zcbor_bool_put(encodingState, (bootloaderInfo->flags >> VER_ENCRYPTION_SUPPORT_OFFSET) & 0x1)) {
        break;
      }

      // Close bootloader info map
      if(!zcbor_map_end_encode(encodingState, 8)) {
        break;
      }
    }
#else
    if(!zcbor_nil_put(encodingState, NULL)) {
      break;
    }
#endif

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
  Print device information from CBOR data

  \param[in] *buff - buffer with CBOR data
  \param[in] *buffSize - len of CBOR data
  \return true if successful, false otherwise
*/
bool bm_info_print_from_cbor(const uint8_t *buff, uint32_t buffSize) {
  ZCBOR_STATE_D	(decodingState, 2, buff, buffSize, 1);
  bool success = false;
  do {

    if(!zcbor_map_start_decode(decodingState)) {
      break;
    }

    struct zcbor_string zcStr;
    if(!zcbor_tstr_expect_lit(decodingState, "version")) {
      break;
    }
    if(!zcbor_tstr_decode(decodingState, &zcStr)) {
      break;
    }
    printf("version: %.*s\n", (int)zcStr.len, zcStr.value);

    if(!zcbor_tstr_expect_lit(decodingState, "APP_NAME")) {
      break;
    }
    if(!zcbor_tstr_decode(decodingState, &zcStr)) {
      break;
    }
    printf("APP_NAME: %.*s\n", (int)zcStr.len, zcStr.value);

    if(!zcbor_tstr_expect_lit(decodingState, "BSP")) {
      break;
    }
    if(!zcbor_tstr_decode(decodingState, &zcStr)) {
      break;
    }
    printf("BSP: %.*s\n", (int)zcStr.len, zcStr.value);

    uint32_t uval;
    if(!zcbor_tstr_expect_lit(decodingState, "SHA")) {
      break;
    }
    if(!zcbor_uint32_decode(decodingState, &uval)) {
      break;
    }
    printf("SHA: %08lX\n", uval);

    if(!zcbor_tstr_expect_lit(decodingState, "reset-reason")) {
      break;
    }
    if(!zcbor_tstr_decode(decodingState, &zcStr)) {
      break;
    }
    printf("Reset Reason: %.*s\n", (int)zcStr.len, zcStr.value);

    if(!zcbor_tstr_expect_lit(decodingState, "build-id")) {
      break;
    }
    if(!zcbor_bstr_decode(decodingState, &zcStr)) {
      break;
    }
    printf("Build ID:");
    for(uint32_t idx=0; idx < zcStr.len; idx++) {
      printf("%02x", zcStr.value[idx]);
    }
    printf("\n");


    if(!zcbor_tstr_expect_lit(decodingState, "bootloader")) {
      break;
    }

    if(!zcbor_nil_expect(decodingState, NULL)) {
      // Bootloader info present, decode!
      printf("Bootloader:\n");

      if(!zcbor_map_start_decode(decodingState)) {
        break;
      }

      if(!zcbor_tstr_expect_lit(decodingState, "version")) {
        break;
      }
      if(!zcbor_tstr_decode(decodingState, &zcStr)) {
        break;
      }
      printf("  Version: ");

      bool bval;
      if(!zcbor_tstr_expect_lit(decodingState, "eng")) {
        break;
      }
      if(!zcbor_bool_decode(decodingState, &bval)) {
        break;
      }
      if(bval) {
        printf("ENG-");
      }

      printf("%.*s", (int)zcStr.len, zcStr.value);

      if(!zcbor_tstr_expect_lit(decodingState, "dirty")) {
        break;
      }
      if(!zcbor_bool_decode(decodingState, &bval)) {
        break;
      }
      if(bval) {
        printf("-dirty\n");
      } else {
        printf("\n");
      }

      if(!zcbor_tstr_expect_lit(decodingState, "SHA")) {
      break;
      }
      if(!zcbor_uint32_decode(decodingState, &uval)) {
        break;
      }
      printf("  SHA: %08lX\n", uval);


      if(!zcbor_tstr_expect_lit(decodingState, "signature")) {
        break;
      }
      if(!zcbor_bool_decode(decodingState, &bval)) {
        break;
      }
      printf("  Signature Support: %d\n", bval);

      if(!zcbor_tstr_expect_lit(decodingState, "encryption")) {
        break;
      }
      if(!zcbor_bool_decode(decodingState, &bval)) {
        break;
      }
      printf("  Encryption Support: %d\n", bval);

      if(!zcbor_map_end_decode(decodingState)) {
        break;
      }
    } else {
      printf("No Bootloader\n");
    }

    if(!zcbor_map_end_decode(decodingState)) {
      break;
    }

    success = true;
  } while (0);

  return success;
}
