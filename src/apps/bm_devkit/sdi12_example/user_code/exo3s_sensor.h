//
// Created by Uma Arthika Katikapalli on 10/7/24.
//
#pragma once
#include <stdint.h>
#include "OrderedSeparatorLineParser.h"

#define sizeBuffer 256
#define sdiSuccess 0
#define sdiFail 1
#define sdiTimeout 2		// timeout condition
#define sdiParityError 3 	// parity error in reply
#define sdiReplyError 4		//
#define breakTimeMin 15		// spec is 12 but a bit extra does no harm
#define markTimeMin 9		// actually 8.33 but I am not OCD
#define maxSensorResponseTime 15	// time after the command is sent
#define maxTotalResponseTime 900	// time for the longest response which could be expectd
#define characterDelay 2	// maximum time to wait for the next character before a fault flagged
#define delayAfterTransmit 2	// a short delay put in after the last character is sent to allow for switching

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
  // TODO:  fill this out correctly
};

class SondeEXO3sSensor {
  public:
    SondeEXO3sSensor();
    void init();
    void flush(void);
    void sdi_wake(uint32_t);
    void sdi_sleep(void);
    void inquire_cmd(void);
    void identify_cmd(void);
    void measure_cmd(void);
    void data_cmd(int);
    static constexpr char SONDE_EXO3S_RAW_LOG[] = "sonde_exo3s_raw.log";
    EXO3sample getLatestSample() const { return _latest_sample; }


    // Variables
    char slaveID = 0;
    unsigned char txBufferIndex = 0;
    unsigned char txBufferLength = 0;
    unsigned char rxBufferIndex = 0;
    unsigned char rxBufferLength = 0;
    char txBuffer[sizeBuffer];
    char rxBuffer[sizeBuffer];
    int availTime;		// filled in to give time befor sensor will have a reading milliSecs
    char sensorCount;	// filled in to give the number of sensors that will be read
    char sensorsRead;	// number of sensors actually returned with each Dn command
    float sensorReadings[12];


  private:
    void sdi_break_mark();
    void sdi_transmit(const char*);
    bool sdi_receive();
//    void sdi_cmd(const char*);
    void sdi_cmd(int);
    void clear_buffer(int);
    int sdi_version = 0;
    int m =1;
    uint32_t receive_timeout = 900;
    static constexpr uint32_t BAUD_RATE = 1200;
    static constexpr char LINE_TERM = '\n';
    static constexpr char SENSOR_BM_LOG_ENABLE[] = "sensorBmLogEnable";
    uint32_t _sensorBmLogEnable = 0;

    OrderedSeparatorLineParser d0_parser;
    OrderedSeparatorLineParser d1_parser;
    OrderedSeparatorLineParser d2_parser;
    EXO3sample _latest_sample;

    // Value types for a EXO3 sonde sensor
    // Example: 0+24.288+7.08+6.74+0.69\r\n
    // Set string values we don't care about to TYPE_INVALID to save memory.
    static constexpr ValueType D0_PARSER_VALUE_TYPES[] = {
        TYPE_INVALID,   // echo of the command and slave address
        TYPE_DOUBLE,    // serial number
        TYPE_DOUBLE,    // sequence number
        TYPE_DOUBLE,   // datetime string
        TYPE_DOUBLE    // code space
    };

    static constexpr ValueType D1_PARSER_VALUE_TYPES[] = {
        TYPE_INVALID,   // echo of the command and slave address
        TYPE_DOUBLE,    // serial number
        TYPE_DOUBLE,    // sequence number
        TYPE_DOUBLE,   // datetime string
        TYPE_DOUBLE    // code space
    };

    static constexpr ValueType D2_PARSER_VALUE_TYPES[] = {
        TYPE_INVALID,   // echo of the command and slave address
        TYPE_DOUBLE,    // serial number
        TYPE_DOUBLE,    // sequence number
    };

};
