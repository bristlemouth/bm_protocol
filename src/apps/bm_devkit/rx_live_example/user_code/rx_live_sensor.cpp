//
// Created by Evan Shapiro on 9/22/24.
//

#include "rx_live_sensor.h"
#include "payload_uart.h"
#include "uptime.h"
#include <stdio.h>  // For printf
#include <string.h> // For strlen

// Constructor without requiring the serial number
RxLiveSensor::RxLiveSensor()
    : _serial_number(0), _ccCode(0), _state(RX_LIVE_IDLE), _pending_command(nullptr),
      _state_timeout_timer(-1), _state_action_timer(-1),
      _std_parser(",", 256, STD_PARSER_VALUE_TYPES, 10),
      _sts_parser(",", 256, STS_PARSER_VALUE_TYPES, 14),
      _latest_detection{} {
  // No need to compute CC code yet, as we don't have the serial number
}

void RxLiveSensor::init(const uint32_t serial_number) {
  // allocate memory for parsers
  _sts_parser.init();
  _std_parser.init();
  this->_serial_number = serial_number;
  computeCCCode();
}

// Compute the CC code from the serial number
void RxLiveSensor::computeCCCode() {
  uint32_t tempSerial = _serial_number;
  uint32_t sum = 0;

  // Calculate the sum of the digits of the serial number
  while (tempSerial > 0) {
    sum += tempSerial % 10; // Add the last digit
    tempSerial /= 10;       // Remove the last digit
  }

  _ccCode = sum; // Set the computed CC code to the sum of the digits
  printf("%llut | Computed CC Code: %u for serial: %u\n", uptimeGetMs(), _ccCode,
         _serial_number);
}

// Transmit the given RxLiveCommand
void RxLiveSensor::txCommand(const RxLiveCommand &command) const {
  // Transmit the command buffer using PLUART, with serial number and ccCode
  char formattedCommand[64] = {0};
  snprintf(formattedCommand, sizeof(formattedCommand), "*%06u.0#%02u,%s\r",
            _serial_number, _ccCode, command.command_buffer);

  const size_t commandLen = strlen(formattedCommand);
  PLUART::startTransaction();
  PLUART::write(reinterpret_cast<uint8_t *>(formattedCommand), commandLen);
  PLUART::endTransaction();

  printf("%llut | Tx'd command: %s\n", uptimeGetMs(), formattedCommand);
}

// Sends a wakes up the sensor then sends the specified command.
bool RxLiveSensor::sendCommand(RxLiveCommandType command_type) {
  if (_state != RX_LIVE_IDLE) {
    printf("%llut | Cannot send command from state: %d\n", uptimeGetMs(), _state);
    return false;
  }

  // Search for the command in the rxLiveCommandSet
  const RxLiveCommand *foundCommand = nullptr;
  for (const auto &i : rxLiveCommandSet) {
    if (i.type == command_type) {
      foundCommand = &i;
      break;
    }
  }

  // If command doesn't exist, return false
  if (foundCommand == nullptr) {
    printf("%llut | Error: Command type %d not found!\n", uptimeGetMs(), command_type);
    return false;
  }

  _pending_command = foundCommand; // Store the found command as pending
  printf("%llut | Waking up sensor, then will send command: %s\n", uptimeGetMs(),
         _pending_command->command_buffer);
  txCommand(rxLiveCommandSet[0]);  // Transmit the STATUS command
  transitionToState(RX_LIVE_WAKE); // Change state to RX_LIVE_WAKE
  return true;
}

bool RxLiveSensor::validateResponseHeader(const char *rx_data) const {
  char headerBuffer[13] = {0};
  snprintf(headerBuffer, sizeof(headerBuffer), "*%06u.0#%02u", _serial_number, _ccCode);
  return strstr(rx_data, headerBuffer) != nullptr;
}

bool RxLiveSensor::validateCommandResponse(const char *rx_data,
                                           RxLiveCommand const &cmd) const {
  bool ret_val = validateResponseHeader(rx_data);
  ret_val &= strstr(rx_data, cmd.valid_response_pattern) != nullptr;
  if (ret_val) {
    printf("%llut | Received valid response to command: %s\n", uptimeGetMs(),
           cmd.command_buffer);
  } else {
    printf("%llut | EROOR - Received invalid response to command: %s\n!", uptimeGetMs(),
           cmd.command_buffer);
  }
  return ret_val;
}

void RxLiveSensor::transitionToState(const RxLiveState to_state) {
  switch (to_state) {
  case RX_LIVE_IDLE:
    _state_timeout_timer = -1;
    _state_action_timer = -1;
    _pending_command = nullptr;
    _state = RX_LIVE_IDLE;
    break;
  case RX_LIVE_WAKE:
    _state_timeout_timer = -1;
    _state_action_timer = -1;
    _state = RX_LIVE_WAKE;
    break;
  case RX_LIVE_CMD_PENDING:
    _state_timeout_timer = -1;
    _state_action_timer = -1;
    _state = RX_LIVE_CMD_PENDING;
    break;
  default:
    printf("%llut | ERROR unhandled state transition to: %d\n", uptimeGetMs(), to_state);
    _state = to_state;
    break;
  }
}

RxLiveStatusCode RxLiveSensor::serviceStateTimers() {
  switch (_state) {
  case RX_LIVE_WAKE:
    if (_pending_command == nullptr) {
      printf("%llut | No pending command in serviceStateTimers.RX_LIVE_WAKE!\n", uptimeGetMs());
      transitionToState(RX_LIVE_IDLE);
      return RX_CODE_TMR_STATE_ERR_1;
    }
    if (_state_action_timer >= 0 &&
        (uint32_t)uptimeGetMs() - (uint32_t)_state_action_timer >= WAKE_POLL_S * 1000) {
      printf("%llut | Re-sending status to wakeup sensor, then will send command: %s\n",
             uptimeGetMs(), _pending_command->command_buffer);
      txCommand(rxLiveCommandSet[0]); // Transmit the STATUS command
      _state_action_timer = (uint32_t)uptimeGetMs();
    } else if (_state_action_timer < 0) {
      printf("%llut | Starting state action timer for serviceStateTimers.RX_LIVE_WAKE.\n",
             uptimeGetMs());
      _state_action_timer = (uint32_t)uptimeGetMs();
    }
    return RX_CODE_WAIT_WAKE;

  case RX_LIVE_CMD_PENDING:
    if (_pending_command == nullptr) {
      printf("%llut | No pending command in serviceStateTimers.RX_LIVE_CMD_PENDING!\n",
             uptimeGetMs());
      transitionToState(RX_LIVE_IDLE);
      return RX_CODE_TMR_STATE_ERR_2;
    }
    if (_state_timeout_timer >= 0 && (uint32_t)uptimeGetMs() - (uint32_t)_state_timeout_timer >=
                                         CMD_PENDING_TIMEOUT_S * 1000) {
      printf("%llut | CMD_PENDING_TIMEOUT, retrying command: %s\n", uptimeGetMs(),
             _pending_command->command_buffer);
      txCommand(*_pending_command); // Transmit the found command
      _state_timeout_timer = (uint32_t)uptimeGetMs();
      return RX_CODE_TMR_CMD_TIMEOUT;
    }
    if (_state_timeout_timer < 0) {
      printf(
          "%llut | Starting state timeout timer for serviceStateTimers.RX_LIVE_CMD_PENDING.\n",
          uptimeGetMs());
      _state_timeout_timer = (uint32_t)uptimeGetMs();
    }
    return RX_CODE_WAIT_CMD;
  default:
    break;
  }
  return RX_CODE_TMR_NOOP;
}

RxLiveStatusCode RxLiveSensor::run(const uint8_t *rx_data, const size_t rx_len) {
  const bool has_rx_data = (rx_data != nullptr && rx_len > 0);

  if (const RxLiveStatusCode timer_status = serviceStateTimers();
      timer_status > RX_CODE_ERROR) {
    return timer_status;
  }

  switch (_state) {

  case RX_LIVE_IDLE:
    if (has_rx_data) {
      if (_sts_parser.parseLine(reinterpret_cast<const char *>(rx_data), rx_len)) {
        // TODO - store any data from the STS lines?
        printf("%llut | Rx-Live data was an STS line.\n", uptimeGetMs());
        return RX_CODE_STS;
      }
      printf("%llut | WARN - Rx-Live data was not an STS line!\n", uptimeGetMs());
      if (_std_parser.parseLine(reinterpret_cast<const char *>(rx_data), rx_len)) {
        printf("%llut | Rx-Live data was an STD detection!\n", uptimeGetMs());
        const Value tag_serial_no = _std_parser.getValue(4);
        const Value tag_code_space = _std_parser.getValue(3);
        if (tag_serial_no.type != TYPE_UINT64) {
          printf("%llut | Parsed invalid tag_serial_no data type: %u\n", uptimeGetMs(),
                 tag_serial_no.type);
          return RX_CODE_DETECTION_FAIL;
        }
        if (tag_code_space.type != TYPE_STRING) {
          printf("%llut | Parsed invalid tag_code_space data type: %u\n", uptimeGetMs(),
                 tag_code_space.type);
          return RX_CODE_DETECTION_FAIL;
        }
        TagID parsed_tag_id;
        if (parseTagCodeSpace(tag_code_space.data.string_val_ptr, &parsed_tag_id)) {
          parsed_tag_id.tag_serial_no = tag_serial_no.data.uint64_val;
          parsed_tag_id.detection_count = 1;
          printf("\tParsed detection Tag ID:\n");
          printf("\t\tCode Char: %c\n", parsed_tag_id.code_char);
          printf("\t\tCode Freq: %u\n", parsed_tag_id.code_freq);
          printf("\t\tCode Channel: %u\n", parsed_tag_id.code_channel);
          printf("\t\tTag Serial no: %u\n", parsed_tag_id.tag_serial_no);
          _latest_detection = parsed_tag_id;
          return RX_CODE_DETECTION;
        } else {
          printf("\tERROR - Failed to parse tag code space!\n");
          return RX_CODE_TAG_PARSE_FAIL;
        }
      }
      printf("%llut | WARN - Rx-Live data was not an STD line!\n", uptimeGetMs());
      printf("%llut | TODO, process data: %.*s\n", uptimeGetMs(), rx_len,
             reinterpret_cast<const char *>(rx_data));
      return RX_CODE_NO_PARSE;
    }
    return RX_CODE_IDLE;

  case RX_LIVE_WAKE:
    if (!has_rx_data) {
      if (_pending_command != nullptr) {
        // Will spin here quite quickly, only un-comment for print-debugging.
        // printf("Waiting for wakeup. ");
        // printf(" Pending command: %s\n", _pending_command->command_buffer);
        return RX_CODE_WAIT_WAKE;
      }
      printf("%llut | Waiting for wakeup, but NO Pending command!\n", uptimeGetMs());
      transitionToState(RX_LIVE_IDLE);
      return RX_CODE_STATE_ERR;
    }
    if (validateCommandResponse(reinterpret_cast<const char *>(rx_data), rxLiveCommandSet[0])) {
      printf("%llut | Valid STATUS response received, Rx-Live is awake.\n", uptimeGetMs());
      // If there's a pending command, send it and go to CMD pending. Otherwise go back to idle
      if (_pending_command != nullptr) {
        txCommand(*_pending_command);
        transitionToState(RX_LIVE_CMD_PENDING);
        return RX_CODE_WOKE_UP;
      }
      printf(" NO Pending command!\n");
      transitionToState(RX_LIVE_IDLE);
      return RX_CODE_STATE_ERR;
    }
    printf("%llut | Invalid STATUS response received, keep waiting?\n", uptimeGetMs());
    return RX_CODE_BAD_STATUS;

  case RX_LIVE_CMD_PENDING:
    if (_pending_command == nullptr) {
      printf("%llut | Error - no pending_command in RX_LIVE_CMD_PENDING state!", uptimeGetMs());
      transitionToState(RX_LIVE_IDLE);
      return RX_CODE_STATE_ERR;
    }

    if (!has_rx_data) {
      printf("%llut | Waiting for response to command: %s\n", uptimeGetMs(),
             _pending_command->command_buffer);
      return RX_CODE_WAIT_CMD;
    }

    //TODO - Some commands may want their responses processed for useful data?
    // printf("Received response to command: %s\n", _pending_command->command_buffer);
    // printf("response_len: %d", rx_len);
    // printf("\tresponse: %.*s\n", rx_len, rx_data);
    if (validateCommandResponse(reinterpret_cast<const char *>(rx_data), *_pending_command)) {
      transitionToState(RX_LIVE_IDLE);
      return RX_CODE_VALID_CMD;
    }
    // If it's not the response we're looking for, it might have been other data. Keep waiting.
    return RX_CODE_INVALID_CMD;

  default:
    // We should never get here!
    transitionToState(RX_LIVE_IDLE);
    return RX_CODE_DEFAULT_STATE;
  }
}

// Static function to parse the tag_code_space string into a TagCodeSpace struct
bool RxLiveSensor::parseTagCodeSpace(const char *code_space_str, TagID *tag_id) {
  if (code_space_str == nullptr || tag_id == nullptr) {
    printf("Error: Invalid input to parseTagCodeSpace.\n");
    return false;
  }
  const char *p = code_space_str;
  // Parse code_char
  char code_char = *p;
  if (!isalpha(static_cast<unsigned char>(code_char))) {
    printf("Error: Invalid code_char in tag code space: %s\n", code_space_str);
    return false;
  }
  tag_id->code_char = code_char;
  p++; // Move to next character
  // Parse code_freq
  char freq_str[4] = {0}; // Max 3 digits + null terminator
  int freq_index = 0;
  while (isdigit(static_cast<unsigned char>(*p)) && freq_index < 3) {
    freq_str[freq_index++] = *p++;
  }
  freq_str[freq_index] = '\0';
  if (freq_index == 0) {
    printf("Error: Invalid code_freq in tag code space: %s\n", code_space_str);
    return false;
  }
  tag_id->code_freq = static_cast<uint16_t>(atoi(freq_str));
  // Expect a '-'
  if (*p != '-') {
    printf("Error: Expected '-' in tag code space after freq: %s\n", code_space_str);
    return false;
  }
  p++; // Move past '-'
  // Parse code_channel
  char channel_str[5] = {0}; // Max 4 digits + null terminator
  int channel_index = 0;
  while (isdigit(static_cast<unsigned char>(*p)) && channel_index < 4) {
    channel_str[channel_index++] = *p++;
  }
  channel_str[channel_index] = '\0';
  if (channel_index == 0) {
    printf("Error: Invalid code_channel in tag code space: %s\n", code_space_str);
    return false;
  }
  tag_id->code_channel = static_cast<uint16_t>(atoi(channel_str));
  // Check for any extra characters
  if (*p != '\0') {
    printf("Error: Unexpected characters in tag code space: %s\n", code_space_str);
    return false;
  }
  return true; // Parsing successful
}
