#include "aanderaa_adcp_sensor.h"
#include "FreeRTOS.h"
#include "app_util.h"
#include "bm_config.h"
#include "bm_os.h"
#include "configuration.h"
#include "payload_uart.h"
#include "spotter.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include <math.h>
#include <string>
#include <type_traits>

using namespace std;

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

template <typename T>
static void round_to_lowest_param(T &value, const char *acceptable_values[], size_t n) {
  static_assert(is_same<T, float>::value || is_same<T, uint32_t>::value,
                "Type is not supported");

  T compared_value = 0;
  for (size_t i = 0; i < n; i++) {
    T converted_value;
    if (is_same<T, float>::value) {
      converted_value = strtof(acceptable_values[i], NULL);
    } else if (is_same<T, uint32_t>::value) {
      converted_value = strtoul(acceptable_values[i], NULL, 10);
    }

    if (!i) {
      compared_value = converted_value;
    }

    if (converted_value > value) {
      break;
    }
    compared_value = converted_value;
  }
  value = compared_value;
}

static void get_acceptable_cell_sizes(float &size) {
  const char *cell_size[] = {
      "0.5m", "1.0m", "1.5m", "2.0m", "2.5m", "3.0m", "3.5m", "4.0m", "4.5m", "5.0m",
  };
  round_to_lowest_param(size, cell_size, array_size(cell_size));
}

static void get_acceptable_cell_count(uint32_t &depth) {
  const char *depths[] = {
      "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",  "10", "12", "15",
      "20", "25", "30", "35", "40", "45", "50", "55", "60", "65", "70", "75",
  };
  round_to_lowest_param(depth, depths, array_size(depths));
}

static void get_acceptable_first_cell_distance(float &distance) {
  const char *distances[]{
      "1.5m",  "1.6m",  "1.7m",  "1.8m",  "1.9m",  "2.0m",  "2.1m",  "2.2m",  "2.3m",
      "2.4m",  "2.5m",  "2.6m",  "2.7m",  "2.8m",  "2.9m",  "3.0m",  "3.5m",  "4.0m",
      "4.5m",  "5.0m",  "5.5m",  "6.0m",  "6.5m",  "7.0m",  "7.5m",  "8.0m",  "8.5m",
      "9.0m",  "9.5m",  "10.0m", "11.0m", "12.0m", "13.0m", "14.0m", "15.0m", "16.0m",
      "17.0m", "18.0m", "19.0m", "20.0m", "22.0m",
  };
  round_to_lowest_param(distance, distances, array_size(distances));
}

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
  if (_sensorDepthM > MAX_MEASURING_DEPTH) {
    _sensorDepthM = MAX_MEASURING_DEPTH;
    set_config_float(BM_CFG_PARTITION_SYSTEM, SENSOR_DEPTH_M, strlen(SENSOR_DEPTH_M),
                     _sensorDepthM);
    save_config(BM_CFG_PARTITION_SYSTEM, false);
  }

  get_config_float(BM_CFG_PARTITION_SYSTEM, CELL_SIZE_M, strlen(CELL_SIZE_M), &_cellSizeM);
  debug_printf("cellSizeM: %f\n", _cellSizeM);

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
  wakeSensor();

  // send stop command to stop streaming
  sendCommand(CMD_STOP, 10000);

  AanderaaString fw_vers_str = {};
  if (sendCommand(CMD_GET_FW_VERSION, &fw_vers_str) == BmOK) {
    debug_printf("ADCP fw version is %s\n", fw_vers_str);
  }
  clearPayloadBuffer();
  getSensorHelp();

  if (sendCommand(CMD_GET_SERIAL_NUMBER, &_serialNumber) == BmOK) {
    sensor_log("ADCP serial number is %" PRIu32 "\n", _serialNumber);
    get_config_uint(BM_CFG_PARTITION_SYSTEM, SENSOR_SERIAL_NUMBER, strlen(SENSOR_SERIAL_NUMBER),
                    &_serialNumber);
  } else {
    sensor_log("Could not get serial number, sensor is not available.\n");
  }

  setDefaultConfigs();

  // Setup tilt parameters
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
  readValidateWriteValue(CMD_BURST_PERIOD_PLACEMENT, "End Of Interval");

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
  // must be below MAX_MEASURING_DEPTH
  float cell_size = _cellSizeM;
  get_acceptable_cell_sizes(cell_size);
  printf("Cell size: %.02f\n", cell_size);
  uint32_t num_cells = ceil(_sensorDepthM / cell_size);
  get_acceptable_cell_count(num_cells);
  printf("Cell count: %" PRIu32 "\n", num_cells);

  // Reference 4.10.6 in TD 304: If instrument referenced the minimum distance
  // to first cell should be 1 meter blanking zone for 5400 and 2 meter for
  // 5402 and 5403, plus half the cell size
  float blanking_zone = 1 + (0.5 * cell_size);
  // Round  up to nearest tenths place hack
  blanking_zone = floor(10 * blanking_zone + 0.5f) / 10;

  float first_cell_distance = _sensorDepthM - static_cast<float>(num_cells) * cell_size;
  if (first_cell_distance < blanking_zone) {
    first_cell_distance += blanking_zone;
  }
  get_acceptable_first_cell_distance(first_cell_distance);
  printf("First cell distance: %.02f\n", first_cell_distance);

  if (cell_size != _cellSizeM) {
    _cellSizeM = cell_size;
    set_config_float(BM_CFG_PARTITION_SYSTEM, CELL_SIZE_M, strlen(CELL_SIZE_M), _cellSizeM);
    save_config(BM_CFG_PARTITION_SYSTEM, false);
  }

  readValidateWriteValue(COLUMN_1(CMD_ENABLE_SURFACE_REFERENCE), CMD_NO);
#if AANDERAA_5400_FW_VERSION > 80129
  readValidateWriteValue(COLUMN_1(CMD_DISTANCE_FIRST_CELL_CENTER), first_cell_distance);
  readValidateWriteValue(COLUMN_1(CMD_CELL_CENTER_SPACING), "1.0m");
#else
  readValidateWriteValue(COLUMN_1(CMD_DISTANCE_FIRST_CELL), "1.0m");
  readValidateWriteValue(COLUMN_1(CMD_CELL_OVERLAP), "0%");
#endif
  readValidateWriteValue(COLUMN_1(CMD_NUMBER_OF_CELLS), num_cells);
  readValidateWriteValue(COLUMN_1(CMD_CELL_SIZE), cell_size);
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
    sendCommand(CMD_STOP, 10000);
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

  raw_log("tick: %" PRIu64 ", column: %" PRIu16 ", cell_state_1: %" PRIu32
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

            5400 {serial_number} {record_state} {ping_count}
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
      "5400",
  };

  static constexpr uint8_t throwaway_fields = 3;

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

    if (byte == 0x13 || byte == 0x11 || byte == '%' || byte == '!') {
      continue;
    }

    _payload_buffer[_payload_idx] = byte;
    _payload_idx++;
    if (byte != LINE_TERM) {
      continue;
    }

    _payload_buffer[_payload_idx] = '\0';

    bm_debug("Data from sensor: %s\n", _payload_buffer);

    _payload_idx = 0;
    const char *line = _payload_buffer;
    ret |= parseMeasurements(line) == BmOK;
    clearPayloadBuffer();
  }
  return ret;
}
