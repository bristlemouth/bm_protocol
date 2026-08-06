#include "aanderaa_adcp_sensor.h"
#include "FreeRTOS.h"
#include "bm_config.h"
#include "bm_os.h"
#include "configuration.h"
#include "payload_uart.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include <string>

extern "C" {
#include "bm_rtc.h"
}
#if __has_include("debug.h")
#include "debug.h"
#else
#define debug_printf printf
#endif

#define raw_log(fmt, ...) spotter_log(0, RAW_LOG, USE_TIMESTAMP, fmt, ##__VA_ARGS__)
#define sensor_log(fmt, ...) spotter_log(0, LOG, USE_TIMESTAMP, fmt, ##__VA_ARGS__)

AanderaaAdcpSensor::AanderaaAdcpSensor()
    : _parser("\t", 256, PARSER_VALUE_TYPE, FIELDS_PER_MEASUREMENT) {};

void AanderaaAdcpSensor::init() {
  _parser.init();
  get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_BM_LOG_ENABLE, strlen(SENSOR_BM_LOG_ENABLE),
                  &_sensorBmLogEnable);
  debug_printf("sensorBmLogEnable: %" PRIu32 "\n", _sensorBmLogEnable);

  // reading period in seconds
  get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_INTERVAL_S, strlen(SENSOR_INTERVAL_S),
                  &_readingPeriodS);
  debug_printf("readingPeriodS: %" PRIu32 "\n", _readingPeriodS);

  get_config_float(BM_CFG_PARTITION_SYSTEM, SENSOR_DEPTH_M, strlen(SENSOR_DEPTH_M),
                   &_sensorDepthM);
  debug_printf("sensorDepthM: %f\n", _sensorDepthM);

  PLUART::init(USER_TASK_PRIORITY);
  PLUART::setBaud(BAUD_RATE);
  // Enable passing raw bytes to user app.
  PLUART::setUseByteStreamBuffer(true);
  PLUART::setUseLineBuffer(true);
  PLUART::setTerminationCharacter(LINE_TERM);
  // Turn on the UART.
  PLUART::enable();
}

/*!
 @brief Configures the Aanderaa ADCP sensor

 @details The ADCP is facing downwards, which means the sensor must be set up
          to accomodate this configuration. Will print off configurations after
          they have been validated and/or written.
 */
void AanderaaAdcpSensor::configureSensor(void) {
  uint16_t read_len = 0;

  wakeSensor();

  // send stop command to stop streaming
  sendCommand(CMD_STOP);

  PLUART::write((uint8_t *)"Get SW Version\r\n", strlen("Get SW Version\r\n"));
  uint32_t read_duration_ms = 2000;
  uint32_t start_time = pdTICKS_TO_MS(xTaskGetTickCount());
  while ((pdTICKS_TO_MS(xTaskGetTickCount()) - start_time) < read_duration_ms) {
    if (PLUART::lineAvailable()) {
      read_len = PLUART::readLine(_payload_buffer, sizeof(_payload_buffer));
      if (read_len > 0) {
        debug_printf("%.*s\n", read_len, _payload_buffer);
      }
    }
  }
  clearPayloadBuffer();
  getSensorHelp();

  if (sendCommand(CMD_GET_SERIAL_NUMBER, &_serialNumber) == BmOK) {
    debug_printf("Serial number is %" PRIu32 "\n", _serialNumber);
    get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_SERIAL_NUMBER, strlen(SENSOR_SERIAL_NUMBER),
                    &_serialNumber);
  } else {
    sensor_log("Could not get serial number, sensor is not available.\n");
  }

  setDefaultConfigs();

  // Setup tilt parameters
#if AANDERAA_5400_FW_VERSION > 80129
  sendCommand(CMD_SET_PASSKEY_1000);
  readValidateWriteValue(CMD_TILT_PING_DISCARD, CMD_NO);
#endif
  sendCommand(CMD_SET_PASSKEY_1);
  readValidateWriteValue(CMD_ENABLE_TILT_COMPENSATION, CMD_YES);

  // Set interval and ping count
#if AANDERAA_5400_FW_VERSION > 80129
  readValidateWriteValue(CMD_INTERVAL, "1 min");
#else
  readValidateWriteValue(CMD_INTERVAL, "10 min");
#endif
  readValidateWriteValue(CMD_PING_COUNT, static_cast<AanderaaUint>(600));

  // Narrowband is recommended to use If the sensor is moving (as under a buoy
  // for example), reference 4.10.4 in TD 304
  readValidateWriteValue(CMD_BANDWIDTH, "Narrowband");

  // Perform in burst mode to optimize sleep time of the ADCP
  readValidateWriteValue(CMD_ENABLE_BURST_MODE, CMD_YES);
#if AANDERAA_5400_FW_VERSION > 80129
  readValidateWriteValue(CMD_BURST_PERIOD_PLACEMENT, "End of Interval");

  // Set simple output
  readValidateWriteValue(CMD_SELECT_PROFILE_PARAMETERS, "Simple Output");
#endif

  // Enable upside down
  readValidateWriteValue(CMD_ENABLE_UPSIDE_DOWN, CMD_YES);

  // Set up columns:
  //   - do not reference the ocean surface when setting up distances
  //   - set the distance from the sensor to the first cell
  //   - set the number of cells
  //   - set the cell size
  //   - set the cell distance apart
  // num_cells * (cell_size + center_cell_spacing) + distance_first_cell
  // must be below 80m
  readValidateWriteValue(COLUMN_1(CMD_ENABLE_SURFACE_REFERENCE), CMD_NO);
#if AANDERAA_5400_FW_VERSION > 80129
  readValidateWriteValue(COLUMN_1(CMD_DISTANCE_FIRST_CELL_CENTER), "1.5m");
  readValidateWriteValue(COLUMN_1(CMD_CELL_CENTER_SPACING), "1.0m");
#else
  readValidateWriteValue(COLUMN_1(CMD_DISTANCE_FIRST_CELL), "1.0m");
  readValidateWriteValue(COLUMN_1(CMD_CELL_OVERLAP), "0%");
#endif
  AanderaaUint cell_count = static_cast<AanderaaUint>(_sensorDepthM);
  readValidateWriteValue(COLUMN_1(CMD_NUMBER_OF_CELLS), cell_count);
  readValidateWriteValue(COLUMN_1(CMD_CELL_SIZE), "1.0m");
  readValidateWriteValue(COLUMN_2(CMD_ENABLE_COLUMN), CMD_NO);
  readValidateWriteValue(COLUMN_3(CMD_ENABLE_COLUMN), CMD_NO);

  // Disable unwanted outputs
  readValidateWriteValue("NE Speed Output", "Off");
  readValidateWriteValue("3-Beam Combination Output", "Off");
  readValidateWriteValue("AutoBeam Output", "Off");
  readValidateWriteValue("Vertical Speed Output", "Off");
  readValidateWriteValue("Strength Output", "Off");
  readValidateWriteValue("Beam Speed Output", "Off");
  readValidateWriteValue("Beam Strength Output", "Off");
  readValidateWriteValue("Heading Output", "Off");
  readValidateWriteValue("Pitch Roll Output", "Off");
  readValidateWriteValue("Abs Tilt Output", "Off");
  readValidateWriteValue("Max Tilt Output", "Off");
  readValidateWriteValue("Tilt Direction Output", "Off");
  readValidateWriteValue("Noise Level Output", "Off");
  readValidateWriteValue("Std Dev Speed Output", "Off");
  readValidateWriteValue("Std Dev Beam Speed Output", "Off");
  readValidateWriteValue("Cross Difference Output", "Off");
  readValidateWriteValue("Correlation Factor Output", "Off");
  readValidateWriteValue("Std Dev Heading Output", "Off");
  readValidateWriteValue("Std Dev Tilt Output", "Off");
  readValidateWriteValue("Charge Voltage Output", "Off");
  readValidateWriteValue("Memory Used Output", "Off");
  readValidateWriteValue("Voltage Output", "Off");
  readValidateWriteValue("Current Output", "Off");
  readValidateWriteValue("Air Detect Output", "Off");
  readValidateWriteValue("Speed Of Sound Output", "Off");
  readValidateWriteValue("Depth Output", "Off");
  readValidateWriteValue("Salinity Output", "Off");
  readValidateWriteValue("Density Output", "Off");

  // Must be run after setting the
  sendCommand(CMD_DO_REFRESH, 10000);

  // save
  if (saveConfiguration() == BmOK) {
    resetSensor(10000);
    wakeSensor();
    sendCommand(CMD_STOP);
  }

  // send get_all command
  getAllConfigurationParameters();
  PLUART::setUseLineBuffer(false);
}

/*!
 @brief Clears the payload buffer used to read from the ADCP
 */
void AanderaaAdcpSensor::clearPayloadBuffer(void) {
  memset(_payload_buffer, 0, sizeof(_payload_buffer));
}

/*!
 @brief Determine if the current output field is a cell index

 @param tok token to examine
 @param toklen token length

 @return Returns true if the index is an expected cell field
 */
static bool isCellIndex(const char *tok, size_t toklen) {
  char buf[8];
  if (toklen == 0 || toklen >= sizeof(buf)) {
    return false;
  }
  memcpy(buf, tok, toklen);
  buf[toklen] = '\0';
  char *end = nullptr;
  long idx = strtol(buf, &end, 10);
  if (end == buf || *end != '\0') {
    return false;
  }

  // Returns true if the first field of a block is a legal DCPS cell index:
  // 0 (surface), 1000-1074 (col 1), 2000-2049 (col 2), 3000-3024 (col 3).
  return idx == 0 || (idx >= 1000 && idx <= 1074) || (idx >= 2000 && idx <= 2049) ||
         (idx >= 3000 && idx <= 3024);
}

/*!
 @brief Handles a single measurement from output string

 @param begin pointer to beginning string of measurement
 @param end pointer to end of string of measurement
 */
void AanderaaAdcpSensor::handleMeasurement(const char *begin, const char *end) {
  uint16_t len = end - begin;
  _parser.parseLine(begin, len);
  uint8_t idx = 0;

  Value column = _parser.getValue(idx++);
  Value cell_state_1 = _parser.getValue(idx++);
#if AANDERAA_5400_FW_VERSION > 80129
  Value cell_state_2 = _parser.getValue(idx++);
#endif
  Value horizontal_speed = _parser.getValue(idx++);
  Value direction = _parser.getValue(idx++);

  bm_debug("tick: %" PRIu64 ", column: %" PRIu16 ", cell_state_1: %" PRIu32
#if AANDERAA_5400_FW_VERSION > 80129
           ", cell_state_2: %" PRIu32
#endif
           ", speed: %0.3f, direction: %0.3f\n",
           uptimeGetMs(), static_cast<uint16_t>(column.data.uint64_val),
           static_cast<uint32_t>(cell_state_1.data.uint64_val),
#if AANDERAA_5400_FW_VERSION > 80129
           static_cast<uint32_t>(cell_state_2.data.uint64_val),
#endif
           horizontal_speed.data.double_val, direction.data.double_val);
}

/*!
 @brief Validates and parses a measurement output string

 @details The output from the ADCP will have all of the cells readings in a 
          single message. The output format is as follows:

            MEASUREMENT 5400 {serial_number} {record_state} {ping_count}
              {{cell_number} {cell_state_1} {cell_state_2} {speed_cm/s} {direction_deg_m}}
              {{cell_number} {cell_state_1} {cell_state_2} {speed_cm/s} {direction_deg_m}}
              {{cell_number} {cell_state_1} {cell_state_2} {speed_cm/s} {direction_deg_m}}
              {{cell_number} {cell_state_1} {cell_state_2} {speed_cm/s} {direction_deg_m}}
              ...
          
          Each field in the output is delimited by a tab (\t). In order to
          parse the message, this function obtains the fields for each cell
          and runs them through the _parser.

 @param line line to parse cell output values

 @return BmOK on success
         BmEBADMSG if message could not be parsed
 */
BmErr AanderaaAdcpSensor::parseMeasurements(const char *line) {
  static const char *field_lut[] = {
#if AANDERAA_5400_FW_VERSION > 80129
      "MEASUREMENT",
#endif
      "5400",
  };

#if AANDERAA_5400_FW_VERSION > 80129
  static constexpr uint8_t throwaway_fields = 4;
#else
  static constexpr uint8_t throwaway_fields = 3;
#endif

  uint8_t ireading = 0;
  const char *reading_start = NULL;
  const char *p = line;
  for (uint32_t itok = 0; *p;) {
    const size_t toklen = strcspn(p, "\t\r\n");

    const char *token = p;
    p += toklen;
    if (*p) {
      p++;
    }

    if (toklen == 0) {
      continue;
    }

    if (itok < array_size(field_lut)) {
      if (strlen(field_lut[itok]) != toklen || strncmp(token, field_lut[itok], toklen) != 0) {
        return BmEBADMSG;
      }
    } else if (itok > throwaway_fields) {
      if (ireading == FIELDS_PER_MEASUREMENT) {
        handleMeasurement(reading_start, token);
        ireading = 0;
      }

      if (ireading == 0) {
        if (!isCellIndex(token, toklen)) {
          return BmEBADMSG;
        }
        reading_start = token;
      }
      ireading++;
    }
    itok++;
  }

  // Get the last cell measurement here
  if (ireading == FIELDS_PER_MEASUREMENT) {
    handleMeasurement(reading_start, p);
    ireading = 0;
  }

  return BmOK;
}

/*!
 @brief Obtain and parse data for Aanderaa ADCP sensor

 @return true if message was obtained and properly parsed
         false otherwise 
 */
bool AanderaaAdcpSensor::getData(void) {
  bool ret = false;
  while (PLUART::byteAvailable()) {
    if (_payload_idx >= array_size(_payload_buffer)) {
      bm_debug("Buffer overflow, clearing buffer\n");
      _payload_idx = 0;
    }

    char byte = PLUART::readByte();

    if (byte == 0x13 || byte == 0x11 || byte == '%') {
      continue;
    }

    bm_debug("Byte: %c index: %lu\n", byte, _payload_idx);

    _payload_buffer[_payload_idx] = byte;
    _payload_idx++;
    if (byte != LINE_TERM) {
      continue;
    }

    _payload_buffer[_payload_idx] = '\0';

    bm_debug("Data from sensor: %s\n", _payload_buffer);

    _payload_idx = 0;
    const char *line = &_payload_buffer[2];
    ret |= parseMeasurements(line) == BmOK;
    clearPayloadBuffer();
  }
  return ret;
}
