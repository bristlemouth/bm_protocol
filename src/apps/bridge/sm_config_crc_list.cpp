#include "sm_config_crc_list.h"
#include "FreeRTOS.h"
#include "cbor.h"

SMConfigCRCList::SMConfigCRCList(BmConfigPartition partition)
    : _cfg(partition), _crc_list{0}, _num_crcs(0) {}

/*!
  \brief Check whether the list contains the given CRC.

  This function reads the list from the configuration.
  It will not write to the configuration unless the retrieved data is invalid,
  in which case it will clear the list, writing an empty array to the configuration.

  \param crc[in] The CRC to search for.
  \return True if the list contains the given CRC, false otherwise.
*/
bool SMConfigCRCList::contains(uint32_t crc) {
  decode();
  for (size_t i = 0; i < _num_crcs; i++) {
    if (_crc_list[i] == crc) {
      return true;
    }
  }
  return false;
}

/*!
  \brief Add the given CRC to the list.

  This function reads the list from the configuration.
  If the CRC is already in the list, it will not be added again,
  and nothing will be written to the configuration.

  Otherwise, the CRC will be added to the front of the list,
  possibly pushing the oldest CRC off the end of the list
  if it already contains the maximum number of items.

  The list will then be written to the configuration.

  \param crc[in] The CRC to add.
*/
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

/*!
  \brief Clear the list.

  This function clears the list and writes an empty array to the configuration.
*/
void SMConfigCRCList::clear() {
  _num_crcs = 0;
  encode();
}

/*!
  \brief Alloc the list of CRCs.

  This function reads the list from the configuration.
  User needs to free the returned list.

  \param[out] num_crcs[out] The number of CRCs in the list.
  \return A pointer to the list of CRCs.
*/
uint32_t *SMConfigCRCList::alloc_list(uint32_t &num_crcs) {
  decode();
  if (_num_crcs) {
    num_crcs = _num_crcs;
    uint32_t *crc_list = static_cast<uint32_t *>(pvPortMalloc(_num_crcs * sizeof(uint32_t)));
    configASSERT(crc_list);
    memcpy(crc_list, _crc_list, _num_crcs * sizeof(uint32_t));
    return crc_list;
  } else {
    num_crcs = 0;
  }
  return NULL;
}

// ---------------------------- PRIVATE ----------------------------

/*!
  \brief Read the list from the configuration.

  This function reads the list from the configuration.
  If the retrieved data is invalid, it will clear the list,
  writing an empty array to the configuration.
*/
void SMConfigCRCList::decode() {
  _num_crcs = 0;
  bool should_clear = false;

  do {
    uint8_t cbor_buffer[MAX_BUFFER_SIZE];
    size_t cbor_buffer_size = MAX_BUFFER_SIZE;
    if (!get_config_cbor(_cfg, KEY, KEY_LEN, cbor_buffer, &cbor_buffer_size)) {
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
        break;
      }
    }
  } while (0);

  if (should_clear) {
    clear();
  }
}

/*!
  \brief Write the list to the configuration.

  This function writes the list to the configuration.
*/
void SMConfigCRCList::encode() {
  CborEncoder encoder;
  uint8_t buffer[MAX_BUFFER_SIZE];
  cbor_encoder_init(&encoder, buffer, MAX_BUFFER_SIZE, 0);

  CborError err;
  CborEncoder array_encoder;
  err = cbor_encoder_create_array(&encoder, &array_encoder, _num_crcs);
  do {
    if (err != CborNoError) {
      break;
    }

    for (size_t i = 0; i < _num_crcs; i++) {
      err = cbor_encode_uint(&array_encoder, _crc_list[i]);
      if (err != CborNoError) {
        break;
      }
    }

    err = cbor_encoder_close_container(&encoder, &array_encoder);
    if (err != CborNoError) {
      break;
    }

    size_t buffer_size = cbor_encoder_get_buffer_size(&encoder, buffer);
    set_config_cbor(_cfg, KEY, KEY_LEN, buffer, buffer_size);
  } while (0);
}
