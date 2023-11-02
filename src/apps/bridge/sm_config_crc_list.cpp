#include "sm_config_crc_list.h"

SMConfigCRCList::SMConfigCRCList(cfg::Configuration *cfg) : _cfg(cfg) {}

bool SMConfigCRCList::contains(uint32_t crc) {
  bool found = false;
  bool should_clear = false;

  do {
    uint8_t cbor_buffer[MAX_BUFFER_SIZE];
    size_t cbor_buffer_size = MAX_BUFFER_SIZE;
    if (!_cfg->getConfigCbor(KEY, KEY_LEN, cbor_buffer, cbor_buffer_size)) {
      should_clear = true;
      break;
    }

    CborError err;
    CborParser parser;
    CborValue array_value;
    err = cbor_parser_init(cbor_buffer, cbor_buffer_size, 0, &parser, &array_value);
    if (err != CborNoError) {
      should_clear = true;
      break;
    }

    if (!cbor_value_is_array(&array_value)) {
      should_clear = true;
      break;
    }

    size_t list_len = 0;
    err = cbor_value_get_array_length(&array_value, &list_len);
    if (err != CborNoError) {
      should_clear = true;
      break;
    }
    if (list_len == 0) {
      break;
    }

    CborValue value;
    err = cbor_value_enter_container(&array_value, &value);
    if (err != CborNoError) {
      should_clear = true;
      break;
    }

    while (!cbor_value_at_end(&value)) {
      if (!cbor_value_is_unsigned_integer(&value)) {
        // TODO should remove this value
      }

      uint64_t retrieved_crc;
      err = cbor_value_get_uint64(&value, &retrieved_crc);
      if (err != CborNoError) {
        // TODO should remove this value
      }

      if (retrieved_crc == crc) {
        found = true;
        break;
      }

      err = cbor_value_advance(&value);
      if (err != CborNoError) {
        // TODO ???
      }
    }
  } while (0);

  if (should_clear) {
    // TODO reset the stored cbor value to any empty array
  }

  return found;
}
