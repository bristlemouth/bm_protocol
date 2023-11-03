#include "sm_config_crc_list.h"
#include "FreeRTOS.h"
#include "cbor.h"

SMConfigCRCList::SMConfigCRCList(cfg::AbstractConfiguration *cfg)
    : _cfg(cfg), _crc_list{0}, _num_crcs(0) {
  configASSERT(_cfg != nullptr);
}

bool SMConfigCRCList::contains(uint32_t crc) {
  decode();
  for (size_t i = 0; i < _num_crcs; i++) {
    if (_crc_list[i] == crc) {
      return true;
    }
  }
  return false;
}

void SMConfigCRCList::add(uint32_t crc) {
  if (contains(crc)) {
    return;
  }

  for (size_t i = MAX_LIST_SIZE - 1; i > 0; i--) {
    _crc_list[i] = _crc_list[i - 1];
  }

  _crc_list[0] = crc;
  if (_num_crcs < MAX_LIST_SIZE) {
    _num_crcs++;
  }

  encode();
}

// ---------------------------- PRIVATE ----------------------------

void SMConfigCRCList::decode() {
  _num_crcs = 0;
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
      if (cbor_value_is_unsigned_integer(&value)) {
        uint64_t retrieved_crc;
        err = cbor_value_get_uint64(&value, &retrieved_crc);
        if (err == CborNoError) {
          _crc_list[_num_crcs++] = retrieved_crc;
        }
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
}

void SMConfigCRCList::encode() {
  CborEncoder encoder;
  uint8_t buffer[MAX_BUFFER_SIZE];
  cbor_encoder_init(&encoder, buffer, MAX_BUFFER_SIZE, 0);

  CborError err;
  CborEncoder array_encoder;
  err = cbor_encoder_create_array(&encoder, &array_encoder, _num_crcs);
  if (err != CborNoError) {
    // TODO
  }

  for (size_t i = 0; i < _num_crcs; i++) {
    err = cbor_encode_uint(&array_encoder, _crc_list[i]);
    if (err != CborNoError) {
      // TODO
    }
  }

  err = cbor_encoder_close_container(&encoder, &array_encoder);
  if (err != CborNoError) {
    // TODO
  }

  size_t buffer_size = cbor_encoder_get_buffer_size(&encoder, buffer);
  _cfg->setConfigCbor(KEY, KEY_LEN, buffer, buffer_size);
}
