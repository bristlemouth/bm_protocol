//
// Created by Uma Arthika Katikapalli on 10/7/24.
//
#pragma once
#include <stdint.h>
#include "OrderedSeparatorLineParser.h"
#include "Exo3LineParser.h"

#define sizeBuffer 256
#define breakTimeMs 15		// spec is 12 ms
#define markTimeMs 9		// spec is 8.33 ms

/* Not used now but maybe in the future improvements
 * #define maxSensorResponseTime 15	// time after the command is sent
 * #define maxTotalResponseTime 900	// time for the longest response which could be expectd
 * #define characterDelay 2	// maximum time to wait for the next character before a fault flagged
 * #define delayAfterTransmit 2	// a short delay put in after the last character is sent to allow for switching
 */

// The EXO3s these sensors
// 1. pH sensor
// 2. Wiped Condensation/Temperature
// 3. Turbidity
// 4. Optical Dissolved Oxygen
// 5. Central Wiper
struct __attribute__((packed)) EXO3sample {
  float temp_sensor;    // Celcius
  float sp_cond;        // uS/cm
  float pH;
  float pH_mV;          // mV
  float dis_oxy;        // % Sat
  float dis_oxy_mg;     // mg/L
  float turbidity;      // NTU
  float wiper_pos;      // volt
  float depth;          // meters
  float power;          // volt
};

class SondeEXO3sSensor {
  public:
    SondeEXO3sSensor();
    void init();
    void flush(void);
    void sdi_wake(void);
    void inquire_cmd(void);
    void identify_cmd(void);
    void measure_cmd(void);
    EXO3sample getLatestSample() const { return _latest_sample; }

    // Variables
    char slaveID = 0;
    char rxBuffer[sizeBuffer];
    char sensorCount;	// filled in to give the number of sensors that will be read
    char sensorsRead;	// number of sensors actually returned with each Dn command
    float sensorReadings[12];


  private:
    void sdi_break_mark();
    void sdi_transmit(const char*);
    bool sdi_receive();
    void sdi_cmd(int);
    int sdi_version = 0;
    uint32_t receive_timeout = 900;
    static constexpr uint32_t BAUD_RATE = 1200;
    static constexpr char LINE_TERM = '\n';

    Exo3DataLineParser d0_parser;
    Exo3DataLineParser d1_parser;
    Exo3DataLineParser d2_parser;
    EXO3sample _latest_sample;

    // Value types for a EXO3 sonde sensor
    // Example: 0+24.288+7.08+6.74+0.69\r\n
    // Set string values we don't care about to TYPE_INVALID to save memory.
    // Response of 0D0! is collected and saved to these variables
    static constexpr ValueType D0_PARSER_VALUE_TYPES[] = {
        TYPE_INVALID,   // echo of the command and slave address
        TYPE_DOUBLE,    // serial number
        TYPE_DOUBLE,    // sequence number
        TYPE_DOUBLE,    // datetime string
        TYPE_DOUBLE     // code space
    };

    // Response of 0D1! is collected and saved to these variables
    static constexpr ValueType D1_PARSER_VALUE_TYPES[] = {
        TYPE_INVALID,   // echo of the command and slave address
        TYPE_DOUBLE,    // serial number
        TYPE_DOUBLE,    // sequence number
        TYPE_DOUBLE,   // datetime string
        TYPE_DOUBLE    // code space
    };

    // Response of 0D2! is collected and saved to these variables
    static constexpr ValueType D2_PARSER_VALUE_TYPES[] = {
        TYPE_INVALID,   // echo of the command and slave address
        TYPE_DOUBLE,    // serial number
        TYPE_DOUBLE,    // sequence number
    };

};
