#ifndef __AANDERAA_ADCP_H__
#define __AANDERAA_ADCP_H__

#include "OrderedSeparatorLineParser.h"
#include "aanderaa_sensor.h"
#include <stdint.h>

#define CMD_ENABLE_TILT_COMPENSATION "Enable Tilt Compensation"

// Broadband (30-70m) or Narrowband (35-80m)
#define CMD_BANDWIDTH "Bandwidth"

#define CMD_ENABLE_UPSIDE_DOWN "Enable Upside Down"

#if AANDERAA_5400_FW_VERSION > 80129
#define CMD_SELECT_PROFILE_PARAMETERS "Select Profile Parameters"

// Tilt limit in degress for when to discard a ping, default is 60 degrees,
// reference table 1-9 in TD 304
#define CMD_TILT_PING_DISCARD "Enable Max Tilt Ping Discard "
#define CMD_MAX_TILT_LIMIT "Max Tilt Limit Ping Discard "

// Whether readings should happen at the beginning or end of an interval
#define CMD_BURST_PERIOD_PLACEMENT "Burst Period Placement"
#endif

#define CMD_ENABLE_BURST_MODE "Enable Burst Mode"

// The following commands are used to configure columns in the water, supports
// up to 3 columns that can be overlapping if desired
#define CMD_ENABLE_SURFACE_REFERENCE "Enable Surface Reference"
#define CMD_CELL_SIZE "Cell Size"
#if AANDERAA_5400_FW_VERSION > 80129
#define CMD_DISTANCE_FIRST_CELL_CENTER "Distance First Cell Center"
#define CMD_CELL_CENTER_SPACING "Cell Center Spacing"
#else
#define CMD_DISTANCE_FIRST_CELL "Distance First Cell"
#define CMD_CELL_OVERLAP "Cell Overlap"
#endif
#define CMD_ENABLE_COLUMN "Enable Column"
#define CMD_NUMBER_OF_CELLS "Number Of Cells"

#define COLUMN_1(cmd) "C1 " cmd
#define COLUMN_2(cmd) "C2 " cmd
#define COLUMN_3(cmd) "C3 " cmd

// Must set after setting cell commands
#define CMD_DO_REFRESH "Do Refresh\r\n"

#define CMD_PING_COUNT "Ping Number"

class AanderaaAdcpSensor : public AanderaaSensor {
public:
  AanderaaAdcpSensor();
  void init();
  void configureSensor(void);
  BmErr parseMeasurements(const char *line);
  bool getData(void);
  void clearPayloadBuffer(void);

  static constexpr char RAW_LOG[] = "aanderaa_adcp_raw.log";
  static constexpr char LOG[] = "aanderaa_adcp.log";

private:
  //cell_index  cell_state_1  cell_state_2  horizontal_speed direction
  static constexpr ValueType PARSER_VALUE_TYPE[] = {
      TYPE_UINT64,
#if AANDERAA_5400_FW_VERSION > 80129
      TYPE_UINT64,
#endif
      TYPE_UINT64, TYPE_DOUBLE, TYPE_DOUBLE,
  };
  static constexpr uint8_t FIELDS_PER_MEASUREMENT = array_size(PARSER_VALUE_TYPE);

  typedef struct {
    AanderaaUint serial_number;
  } ProductionConfigs;

  // Baud set to 115200 default, which is expected by the DCP sensor,
  // reference Table 1-9 in TD 304
  static constexpr uint32_t BAUD_RATE = 115200;

  static constexpr char SENSOR_BM_LOG_ENABLE[] = "sensorBmLogEnable";
  static constexpr char SENSOR_INTERVAL_S[] = "readingPeriodS";
  static constexpr char SENSOR_SERIAL_NUMBER[] = "sensorSerialNum";
  static constexpr char SENSOR_DEPTH_M[] = "sensorDepthM";

  uint32_t _sensorBmLogEnable = 0;
  uint32_t _readingPeriodS = 2;
  float _sensorDepthM = 0;
  OrderedSeparatorLineParser _parser;
  char _payload_buffer[8192] = {};
  uint32_t _payload_idx = 0;
  AanderaaUint _serialNumber;

  void handleMeasurement(const char *begin, const char *end);
};

#endif
