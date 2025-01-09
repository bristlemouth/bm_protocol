#include "pme_wipe_msg.h"
#include "bm_config.h"
#include <math.h>

namespace PmeWipeMsg {

CborError encode(Data &d, uint8_t *cbor_buffer, size_t size,
                 size_t *encoded_len) {
  CborError err;
  CborEncoder encoder, map_encoder;
  cbor_encoder_init(&encoder, cbor_buffer, size, 0);

  do {
    err = cbor_encoder_create_map(&encoder, &map_encoder, NUM_FIELDS);
    if (err != CborNoError) {
      bm_debug("cbor_encoder_create_map failed: %d\n", err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }

    // sensor_header_msg
    err = SensorHeaderMsg::encode(map_encoder, d.header);
    if (err != CborNoError) {
      bm_debug("SensorHeaderMsg::encode failed: %d\n", err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }

    // wipe_time_sec
    err = cbor_encode_text_stringz(&map_encoder, "wipe_time_sec");
    if (err != CborNoError) {
      bm_debug("cbor_encode_text_stringz failed for wipe_time_sec key: %d\n",
             err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }
    err = cbor_encode_double(&map_encoder, d.wipe_time_sec);
    if (err != CborNoError) {
      bm_debug("cbor_encode_double failed for wipe_time_sec value: %d\n",
             err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }

    // start1_mA
    err = cbor_encode_text_stringz(&map_encoder, "start1_mA");
    if (err != CborNoError) {
      bm_debug("cbor_encode_text_stringz failed for start1_mA key: %d\n",
             err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }
    err = cbor_encode_double(&map_encoder, d.start1_mA);
    if (err != CborNoError) {
      bm_debug("cbor_encode_double failed for start1_mA value: %d\n",
             err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }

    // avg1_mA
    err = cbor_encode_text_stringz(&map_encoder, "avg1_mA");
    if (err != CborNoError) {
      bm_debug("cbor_encode_text_stringz failed for avg1_mA key: %d\n",
             err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }
    err = cbor_encode_double(&map_encoder, d.avg1_mA);
    if (err != CborNoError) {
      bm_debug("cbor_encode_double failed for avg1_mA value: %d\n",
             err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }

    // start2_mA
    err = cbor_encode_text_stringz(&map_encoder, "start2_mA");
    if (err != CborNoError) {
      bm_debug("cbor_encode_text_stringz failed for start2_mA key: %d\n",
             err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }
    err = cbor_encode_double(&map_encoder, d.start2_mA);
    if (err != CborNoError) {
      bm_debug("cbor_encode_double failed for start2_mA value: %d\n",
             err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }

    // avg2_mA
    err = cbor_encode_text_stringz(&map_encoder, "avg2_mA");
    if (err != CborNoError) {
      bm_debug("cbor_encode_text_stringz failed for avg2_mA key: %d\n",
             err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }
    err = cbor_encode_double(&map_encoder, d.avg2_mA);
    if (err != CborNoError) {
      bm_debug("cbor_encode_double failed for avg2_mA value: %d\n",
             err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }

    // rsource
    err = cbor_encode_text_stringz(&map_encoder, "rsource");
    if (err != CborNoError) {
      bm_debug("cbor_encode_text_stringz failed for rsource key: %d\n",
             err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }
    err = cbor_encode_double(&map_encoder, d.rsource);
    if (err != CborNoError) {
      bm_debug("cbor_encode_double failed for rsource value: %d\n",
             err);
      if (err != CborErrorOutOfMemory) {
        break;
      }
    }

    err = cbor_encoder_close_container(&encoder, &map_encoder);
    if (err == CborNoError) {
      *encoded_len = cbor_encoder_get_buffer_size(&encoder, cbor_buffer);
    } else {
      bm_debug("cbor_encoder_close_container failed: %d\n", err);

      if (err != CborErrorOutOfMemory) {
        break;
      }
      size_t extra_bytes_needed = cbor_encoder_get_extra_bytes_needed(&encoder);
      bm_debug("extra_bytes_needed: %zu\n", extra_bytes_needed);
    }
  } while (0);

  return err;
}

CborError decode(Data &d, const uint8_t *cbor_buffer, size_t size) {
  CborParser parser;
  CborValue map;
  CborError err = cbor_parser_init(cbor_buffer, size, 0, &parser, &map);

  do {
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_validate_basic(&map);
    if (err != CborNoError) {
      break;
    }
    if (!cbor_value_is_map(&map)) {
      err = CborErrorIllegalType;
      break;
    }

    size_t num_fields;
    err = cbor_value_get_map_length(&map, &num_fields);
    if (err != CborNoError) {
      break;
    }
    if (num_fields != NUM_FIELDS) {
      err = CborErrorUnknownLength;
      bm_debug("expected %zu fields but got %zu\n", NUM_FIELDS, num_fields);
      break;
    }

    CborValue value;
    err = cbor_value_enter_container(&map, &value);
    if (err != CborNoError) {
      break;
    }

    // header
    err = SensorHeaderMsg::decode(value, d.header);
    if (err != CborNoError) {
      break;
    }

    // wipe_time_sec
    if (!cbor_value_is_text_string(&value)) {
      err = CborErrorIllegalType;
      bm_debug("expected string key but got something else\n");
      break;
    }
    err = cbor_value_advance(&value);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_get_double(&value, &d.wipe_time_sec);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_advance(&value);
    if (err != CborNoError) {
      break;
    }

    // start1_mA
    if (!cbor_value_is_text_string(&value)) {
      err = CborErrorIllegalType;
      bm_debug("expected string key but got something else\n");
      break;
    }
    err = cbor_value_advance(&value);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_get_double(&value, &d.start1_mA);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_advance(&value);
    if (err != CborNoError) {
      break;
    }

    // avg1_mA
    if (!cbor_value_is_text_string(&value)) {
      err = CborErrorIllegalType;
      bm_debug("expected string key but got something else\n");
      break;
    }
    err = cbor_value_advance(&value);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_get_double(&value, &d.avg1_mA);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_advance(&value);
    if (err != CborNoError) {
      break;
    }

    // start2_mA
    if (!cbor_value_is_text_string(&value)) {
      err = CborErrorIllegalType;
      bm_debug("expected string key but got something else\n");
      break;
    }
    err = cbor_value_advance(&value);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_get_double(&value, &d.start2_mA);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_advance(&value);
    if (err != CborNoError) {
      break;
    }

    // avg2_mA
    if (!cbor_value_is_text_string(&value)) {
      err = CborErrorIllegalType;
      bm_debug("expected string key but got something else\n");
      break;
    }
    err = cbor_value_advance(&value);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_get_double(&value, &d.avg2_mA);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_advance(&value);
    if (err != CborNoError) {
      break;
    }

    // rsource
    if (!cbor_value_is_text_string(&value)) {
      err = CborErrorIllegalType;
      bm_debug("expected string key but got something else\n");
      break;
    }
    err = cbor_value_advance(&value);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_get_double(&value, &d.rsource);
    if (err != CborNoError) {
      break;
    }
    err = cbor_value_advance(&value);
    if (err != CborNoError) {
      break;
    }

    if (err == CborNoError) {
      err = cbor_value_leave_container(&map, &value);
      if (err != CborNoError) {
        break;
      }
      if (!cbor_value_at_end(&map)) {
        err = CborErrorGarbageAtEnd;
        break;
      }
    }
  } while (0);

  return err;
}

} // namespace PmeWipeMsg