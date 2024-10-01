//
// Created by Evan Shapiro on 9/22/24.
//

#pragma once
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include "OrderedSeparatorLineParser.h"

#define WAKE_POLL_S 3  // Time between wake-up polls in seconds
#define WAKE_TIMEOUT_S 45
#define CMD_PENDING_TIMEOUT_S 1

enum RxLiveState {
  RX_LIVE_IDLE,
  RX_LIVE_WAKE,
  RX_LIVE_CMD_PENDING
};

enum RxLiveCommandType {
  RX_LIVE_STATUS,
  RX_LIVE_RTMNOW
};

enum RxLiveStatusCode {
  RX_CODE_IDLE,
  RX_CODE_TMR_NOOP,
  RX_CODE_WAIT_WAKE,
  RX_CODE_WAIT_CMD,
  RX_CODE_INFO, // enum placeholder for convenience
  RX_CODE_WOKE_UP,
  RX_CODE_VALID_CMD,
  RX_CODE_STS,
  RX_CODE_DETECTION,
  RX_CODE_ERROR, // enum placeholder for convenience
  RX_CODE_NO_PARSE,
  RX_CODE_INVALID_CMD,
  RX_CODE_BAD_STATUS,
  RX_CODE_DETECTION_FAIL,
  RX_CODE_TAG_PARSE_FAIL,
  RX_CODE_CMD_FAIL,
  RX_CODE_PARSE_FAIL,
  RX_CODE_TMR_CMD_TIMEOUT,
  RX_CODE_STATE_ERR,
  RX_CODE_TMR_STATE_ERR_1,
  RX_CODE_TMR_STATE_ERR_2,
  RX_CODE_DEFAULT_STATE,
};

// Struct to encapsulate the command type and literal command buffer
struct RxLiveCommand {
  RxLiveCommandType type;   // The type of command (STATUS, RTMNOW)
  const char* command_buffer;   // The literal command string buffer
  const char* valid_response_pattern;   // substring that should exist in a valid response
};

// Struct to store code space data and serial number
// Assumes structure like [code_char][code_freq]-[code_channel]
//  eg: A69-9006, A180-1702.
//  It _appears_ code_char is always 'A', freq is always 2-3 digits, and channel is always 4
//  All the detail we have found is in vr100-manual.pdf 7.1.1
// (packed) for serialization when Tx-ing data.
// detection_count included here for convenience so user can keep track of counts
struct __attribute__((packed)) TagID {
  uint32_t tag_serial_no;
  uint16_t code_freq;
  uint16_t code_channel;
  uint16_t detection_count;
  char code_char;
};

// Hard-coded list of RxLive commands
constexpr RxLiveCommand rxLiveCommandSet[] = {
  { RX_LIVE_STATUS, "STATUS", "RECORDING,OK" },
  { RX_LIVE_RTMNOW, "RTMNOW","[0009],OK,#9A" }
};

class RxLiveSensor {
public:
  // Constructor to initialize empty instance
  RxLiveSensor();

  // Sends a wakes up the sensor then sends the specified command.
  bool sendCommand(RxLiveCommandType command_type);

  // init must be called after FreeRTOS sets up memory allocation
  //  Sets the serial number of the sensor to the instance
  //  This will instantiate data for the parser
  void init(uint32_t serial_number);

  // Manages command state machine. Optionally pass in received data.
  RxLiveStatusCode run(const uint8_t* rx_data, size_t rx_len);

  TagID getLatestDetection() const { return _latest_detection; }

private:
  // Computes the CC code from the serial number and stores in instance
  void computeCCCode();
  void txCommand(const RxLiveCommand& command) const;  // Transmit the given command
  bool validateResponseHeader(const char* rx_data) const;
  bool validateCommandResponse(const char* rx_data, RxLiveCommand const& cmd) const;
  RxLiveStatusCode serviceStateTimers();
  void transitionToState(RxLiveState to_state);
  static bool parseTagCodeSpace(const char* code_space_str, TagID* tag_code_space);

  uint32_t _serial_number;  // Stores the serial number of the Rx-Live sensor
  uint16_t _ccCode;        // Stores the computed CC code
  RxLiveState _state;      // Stores the current command state
  const RxLiveCommand* _pending_command;
  int64_t _state_timeout_timer;
  int64_t _state_action_timer;
  OrderedSeparatorLineParser _std_parser;
  OrderedSeparatorLineParser _sts_parser;
  TagID _latest_detection;

  // Value types for a Sensor Tag Detection line
  // Example: 667057,004,2019-09-05 10:21:24.106,A69-9006,1025,123,S=63.0,N=39.0,C=0,#4B\r\n
  // Set string values we don't care about to TYPE_INVALID to save memory.
  static constexpr ValueType STD_PARSER_VALUE_TYPES[] = {
    TYPE_UINT64,    // serial number
    TYPE_UINT64,    // sequence number
    TYPE_INVALID,   // datetime string
    TYPE_STRING,   // code space
    TYPE_UINT64,    // *** Tag ID ***
    TYPE_UINT64,    // ADC value
    TYPE_INVALID,   // Signal Level dB
    TYPE_INVALID,   // Noise Level dB
    TYPE_INVALID,   // Channel
    TYPE_INVALID    // Checksum
  };

  // Value types for a Scheduled Status Information (STS) line
  // 667057,000,2019-08-28 17:13:18.324,STS,DC=1,PC=8,LV=12.6,T=22.2,DU=0.0,RU=0.0,XYZ=0.00:0.88:-0.44,N=68.5,NP=40.5,#A1\r\n
  // TODO - parse these out if we ever care
  static constexpr ValueType STS_PARSER_VALUE_TYPES[14] = {TYPE_INVALID};
};
