#include "debug.h"
#include "app_util.h"
#include "user_code.h"
#include "bristlefin.h"
#include "bsp.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "spotter.h"
#include "pubsub.h"
#include "lwip/inet.h"
#include "usart.h"
#include "payload_uart.h"
#include "sensors.h"
#include "OrderedSeparatorLineParser.h"
#include "array_utils.h"


#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000


//Defines for nortek comms
#define MESSAGE_COUNT_THRESHOLD 10 //900  //we are at 1 second period from nortek, so this is 3 mins
#define DATA_BUFFER_SIZE 100


#define DEFAULT_SENSOR_AGG_PERIOD_MIN (3) //15 minutes makes the mote run out of memory with the buffer averaging and the number of sensors.
#define SENSOR_AGG_PERIOD_MS ((double)DEFAULT_SENSOR_AGG_PERIOD_MIN * 60.0 * 1000.0)
// How long should our sample buffer arrays be?
// Rely on a 2s polling period for now, and add 10 samples for padding.
#define MAX_SENSOR_SAMPLES (((uint64_t)SENSOR_AGG_PERIOD_MS / 1000) + 10)

char payload_buffer[2048];

// Structures to store readings for our on-board sensors.
// Note, I made a double and unit64 version of this so I didn't have to mess with the line parser and worry about casting.
typedef struct {
  uint16_t sample_count; // Keep track of how many samples we have in the buffer.
  double* values; // Pointer to the buffer. We'll allocate memory later.
} __attribute__((__packed__)) sensorStatDataDouble_t;

typedef struct {
  uint16_t sample_count; // Keep track of how many samples we have in the buffer.
  uint64_t* values; // Pointer to the buffer. We'll allocate memory later.
} __attribute__((__packed__)) sensorStatDataUint64_t;

typedef struct {
  uint16_t sample_count;
  float mean;
  float stdev;
} __attribute__((__packed__)) sensorStatAgg_t;



float v_b1_x_mean = 0;
float v_b2_y_mean = 0;
float v_b3_z_mean = 0;
float heading_mean = 0;
float pitch_mean = 0;
float roll_mean = 0;
float pressure_mean = 0;
float temperature_mean = 0;
float speed_mean = 0;
float direction_mean = 0;

float v_b1_x_stdev = 0;
float v_b2_y_stdev = 0;
float v_b3_z_stdev = 0;
float heading_stdev = 0;
float pitch_stdev = 0;
float roll_stdev = 0;
float pressure_stdev = 0;
float temperature_stdev = 0;
float speed_stdev = 0;
float direction_stdev = 0;

//Keeping this struct as it provides a nicer way to reference than
typedef struct {
    uint8_t month;
    uint8_t day;
    uint16_t year;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t error_code;
    uint16_t status_code;
    double v_b1_x;
    double v_b2_y;
    double v_b3_z;
    uint16_t amp_b1_x;
    uint16_t amp_b2_y;
    uint16_t amp_b3_z;
    double batt_v;
    double sound_speed;
    double heading;
    double pitch;
    double roll;
    double pressure;   //need
    double temperature;     //need
    uint16_t an_in_1;
    uint16_t an_in_2;
    double speed;
    double direction;
} __attribute__((__packed__))  NortekData_t;

NortekData_t nortekData = {};


// We'll allocate memory for the values buffers later.
sensorStatDataUint64_t month_stats = {};
sensorStatDataUint64_t day_stats = {};
sensorStatDataUint64_t year_stats = {};
sensorStatDataUint64_t hour_stats = {};
sensorStatDataUint64_t minute_stats = {};
sensorStatDataUint64_t second_stats = {};
sensorStatDataUint64_t error_code_stats = {};
sensorStatDataUint64_t status_code_stats = {};
sensorStatDataDouble_t v_b1_x_stats = {};
sensorStatDataDouble_t v_b2_y_stats = {};
sensorStatDataDouble_t v_b3_z_stats = {};
sensorStatDataUint64_t amp_b1_x_stats = {};
sensorStatDataUint64_t amp_b2_y_stats = {};
sensorStatDataUint64_t amp_b3_z_stats = {};
sensorStatDataDouble_t batt_v_stats = {};
sensorStatDataDouble_t sound_speed_stats = {};
sensorStatDataDouble_t heading_stats = {};
sensorStatDataDouble_t pitch_stats = {};
sensorStatDataDouble_t roll_stats = {};
sensorStatDataDouble_t pressure_stats = {};
sensorStatDataDouble_t temperature_stats = {};
sensorStatDataDouble_t an_in_1_stats = {};
sensorStatDataDouble_t an_in_2_stats = {};
sensorStatDataDouble_t speed_stats = {};
sensorStatDataDouble_t direction_stats = {};


// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;

char rtcTimeBuffer[32] = {}; // Let's get UTC time string for convenience
uint64_t this_uptime = 0;
uint64_t net_time = 0;


const ValueType valueTypes[] = {TYPE_UINT64, TYPE_UINT64, TYPE_UINT64, TYPE_UINT64, TYPE_UINT64, TYPE_UINT64, TYPE_UINT64, TYPE_UINT64, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_UINT64, TYPE_UINT64, TYPE_UINT64, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE, TYPE_UINT64, TYPE_UINT64, TYPE_DOUBLE, TYPE_DOUBLE};
// LineParser parser(" ", 512, valueTypes, 25);
OrderedSeparatorLineParser parser(" ", 512, valueTypes, 25);

////////LineParser::LineParser(const char* separator, size_t maxLineLen, ValueType* valueTypes, size_t numValues) :

void fillNortekStruct(){
    nortekData.month = parser.getValue(0).data.uint64_val;
    nortekData.day = parser.getValue(1).data.uint64_val;
    nortekData.year = parser.getValue(2).data.uint64_val;
    nortekData.hour = parser.getValue(3).data.uint64_val;
    nortekData.minute = parser.getValue(4).data.uint64_val;
    nortekData.second = parser.getValue(5).data.uint64_val;
    nortekData.error_code = parser.getValue(6).data.uint64_val;
    nortekData.status_code = parser.getValue(7).data.uint64_val;
    nortekData.v_b1_x = parser.getValue(8).data.double_val;
    nortekData.v_b2_y = parser.getValue(9).data.double_val;
    nortekData.v_b3_z = parser.getValue(10).data.double_val;
    nortekData.amp_b1_x = parser.getValue(11).data.uint64_val;
    nortekData.amp_b2_y = parser.getValue(12).data.uint64_val;
    nortekData.amp_b3_z = parser.getValue(13).data.uint64_val;
    nortekData.batt_v = parser.getValue(14).data.double_val;
    nortekData.sound_speed = parser.getValue(15).data.double_val;
    nortekData.heading = parser.getValue(16).data.double_val;
    nortekData.pitch = parser.getValue(17).data.double_val;
    nortekData.roll = parser.getValue(18).data.double_val;
    nortekData.pressure = parser.getValue(19).data.double_val;
    nortekData.temperature = parser.getValue(20).data.double_val;
    nortekData.an_in_1 = parser.getValue(21).data.uint64_val;
    nortekData.an_in_2 = parser.getValue(22).data.uint64_val;
    nortekData.speed = parser.getValue(23).data.double_val;
    nortekData.direction = parser.getValue(24).data.double_val;
}

void addDataToStatsBuffers(){
    month_stats.values[month_stats.sample_count++] = parser.getValue(0).data.uint64_val;
    day_stats.values[day_stats.sample_count++] = parser.getValue(1).data.uint64_val;
    year_stats.values[year_stats.sample_count++] = parser.getValue(2).data.uint64_val;
    hour_stats.values[hour_stats.sample_count++] = parser.getValue(3).data.uint64_val;
    minute_stats.values[minute_stats.sample_count++] = parser.getValue(4).data.uint64_val;
    second_stats.values[second_stats.sample_count++] = parser.getValue(5).data.uint64_val;
    error_code_stats.values[error_code_stats.sample_count++] = parser.getValue(6).data.uint64_val;
    status_code_stats.values[status_code_stats.sample_count++] = parser.getValue(7).data.uint64_val;
    v_b1_x_stats.values[v_b1_x_stats.sample_count++] = parser.getValue(8).data.double_val;
    v_b2_y_stats.values[v_b2_y_stats.sample_count++] = parser.getValue(9).data.double_val;
    v_b3_z_stats.values[v_b3_z_stats.sample_count++] = parser.getValue(10).data.double_val;
    amp_b1_x_stats.values[amp_b1_x_stats.sample_count++] = parser.getValue(11).data.uint64_val;
    amp_b2_y_stats.values[amp_b2_y_stats.sample_count++] = parser.getValue(12).data.uint64_val;
    amp_b3_z_stats.values[amp_b3_z_stats.sample_count++] = parser.getValue(13).data.uint64_val;
    batt_v_stats.values[batt_v_stats.sample_count++] = parser.getValue(14).data.double_val;
    sound_speed_stats.values[sound_speed_stats.sample_count++] = parser.getValue(15).data.double_val;
    heading_stats.values[heading_stats.sample_count++] = parser.getValue(16).data.double_val;
    pitch_stats.values[ pitch_stats.sample_count++] = parser.getValue(17).data.double_val;
    roll_stats.values[roll_stats.sample_count++] = parser.getValue(18).data.double_val;
    pressure_stats.values[pressure_stats.sample_count++] = parser.getValue(19).data.double_val;
    temperature_stats.values[temperature_stats.sample_count++] = parser.getValue(20).data.double_val;
    an_in_1_stats.values[an_in_1_stats.sample_count++] = parser.getValue(21).data.uint64_val;
    an_in_2_stats.values[an_in_2_stats.sample_count++] = parser.getValue(22).data.uint64_val;
    speed_stats.values[speed_stats.sample_count++] = parser.getValue(23).data.double_val;
    direction_stats.values[direction_stats.sample_count++] = parser.getValue(24).data.double_val;
}

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  parser.init();

  //appointing memory for all of the buffers.
  month_stats.values = static_cast<uint64_t *>(pvPortMalloc(sizeof(uint64_t) * MAX_SENSOR_SAMPLES));
  day_stats.values = static_cast<uint64_t *>(pvPortMalloc(sizeof(uint64_t) * MAX_SENSOR_SAMPLES));
  year_stats.values = static_cast<uint64_t *>(pvPortMalloc(sizeof(uint64_t) * MAX_SENSOR_SAMPLES));
  hour_stats.values = static_cast<uint64_t *>(pvPortMalloc(sizeof(uint64_t) * MAX_SENSOR_SAMPLES));
  minute_stats.values = static_cast<uint64_t *>(pvPortMalloc(sizeof(uint64_t) * MAX_SENSOR_SAMPLES));
  second_stats.values = static_cast<uint64_t *>(pvPortMalloc(sizeof(uint64_t) * MAX_SENSOR_SAMPLES));
  error_code_stats.values = static_cast<uint64_t *>(pvPortMalloc(sizeof(uint64_t) * MAX_SENSOR_SAMPLES));
  status_code_stats.values = static_cast<uint64_t *>(pvPortMalloc(sizeof(uint64_t) * MAX_SENSOR_SAMPLES));
  v_b1_x_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  v_b2_y_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  v_b3_z_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  amp_b1_x_stats.values = static_cast<uint64_t *>(pvPortMalloc(sizeof(uint64_t) * MAX_SENSOR_SAMPLES));
  amp_b2_y_stats.values = static_cast<uint64_t *>(pvPortMalloc(sizeof(uint64_t) * MAX_SENSOR_SAMPLES));
  amp_b3_z_stats.values = static_cast<uint64_t *>(pvPortMalloc(sizeof(uint64_t) * MAX_SENSOR_SAMPLES));
  batt_v_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  sound_speed_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  heading_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  pitch_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  roll_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  pressure_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  temperature_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  an_in_1_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  an_in_2_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  speed_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));
  direction_stats.values = static_cast<double *>(pvPortMalloc(sizeof(double) * MAX_SENSOR_SAMPLES));

  // Setup the UART – the on-board serial driver that talks to the RS232 transceiver.
  PLUART::init(USER_TASK_PRIORITY);
  // Baud set per expected baud rate of the sensor.
  PLUART::setBaud(9600);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter('\n');
  // Turn on the UART.
  PLUART::enable();
  // Enable the input to the Vout power supply.
  bristlefin.enableVbus();
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  bristlefin.enableVout();

  //GPIO Sets to keep BF from burning extra power
  IOWrite(&BF_HFIO, 0);
  IOWrite(&BF_SDI12_OE, 0);

  //Print Headers into log file. will happen every time the mote power cycles, but that's fine.
  //This doesn't seem to be working. For some reason not being written.
  spotter_log(0,"nortek_averages.log", USE_TIMESTAMP,
          "HEADERS,uptime,rtcTimeBuffer,v_b1_x_mean,v_b1_x_stdev,v_b2_y_mean,v_b2_y_stdev,v_b3_z_mean,v_b3_z_stdev,heading_mean,heading_stdev,pitch_mean,pitch_stdev,roll_mean,roll_stdev,pressure_mean,pressure_stdev,temperature_mean,temperature_stdev,speed_mean,speed_stdev,direction_mean,direction_stdev"
      );
  spotter_log(0,"nortek.log", USE_TIMESTAMP,
          "HEADERS,uptime,rtcTimeBuffer,month,day,year,hour,minute,second,error_code,status_code,v_b1_x,v_b2_y,v_b3_z,amp_b1_x,amp_b2_y,amp_b3_z,batt_v,sound_speed,heading,pitch,roll,pressure,temperature,an_in_1,an_in_2,speed,direction"
      );
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */

  //Note that if I use anything protected by a semaphores

  static bool led2State = false;
  /// This checks for a trigger set by ledLinePulse when data is received from the payload UART.
  ///   Each time this happens, we pulse LED2 Green.
  // If LED2 is off and the ledLinePulse flag is set (done in our payload UART process line function), turn it on Green.
  if (!led2State && ledLinePulse > -1) {
    bristlefin.setLed(2, Bristlefin::LED_GREEN);
    led2State = true;
  }
  // If LED2 has been on for LED_ON_TIME_MS, turn it off.
  else if (led2State && ((u_int32_t)uptimeGetMs() - ledLinePulse >= LED_ON_TIME_MS)) {
    bristlefin.setLed(2, Bristlefin::LED_OFF);
    ledLinePulse = -1;
    led2State = false;
  }

  /// This section demonstrates a simple non-blocking bare metal method for rollover-safe timed tasks,
  ///   like blinking an LED.
  /// More canonical (but more arcane) modern methods of implementing this kind functionality
  ///   would bee to use FreeRTOS tasks or hardware timer ISRs.
  static u_int32_t ledPulseTimer = uptimeGetMs();
  static u_int32_t ledOnTimer = 0;
  static bool led1State = false;
  // Turn LED1 on green every LED_PERIOD_MS milliseconds.
  if (!led1State && ((u_int32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_GREEN);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    led1State = true;
  }
    // If LED1 has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (led1State && ((u_int32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_OFF);
    led1State = false;
  }

  if (PLUART::lineAvailable()) {
    ledLinePulse = uptimeGetMs(); // trigger a pulse on LED2

    uint16_t read_len = PLUART::readLine(payload_buffer, sizeof(payload_buffer));

    rtcPrint(rtcTimeBuffer, NULL);
    spotter_log(0, "nortek_raw.log", USE_TIMESTAMP, "tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
    spotter_log_console(0, "[nortek] | tick: %" PRIu64 ", rtc: %s, line: %.*s", uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
    printf("[nortek] | tick: %" PRIu64 ", rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);

    if (parser.parseLine(payload_buffer, read_len)) {
      printf("parsed values: %f | %f\n", parser.getValue(23).data, parser.getValue(24).data);
      fillNortekStruct(); //No args. This uses global parser to update global nortkeData
      if (month_stats.sample_count >= MAX_SENSOR_SAMPLES) {
        printf("ERR - No more room in the Nortek stats buffer, already have %lu readings!\n", MAX_SENSOR_SAMPLES);
        return;
      }

      addDataToStatsBuffers(); // No args, updates all the buffers with latest data from parser.]

      // Let's get UTC time string for convenience
      rtcPrint(rtcTimeBuffer, NULL);
      this_uptime = uptimeGetMs();

      printf("batt v from userProcessLine | count: %u/%lu batt v: %f \n",
        batt_v_stats.sample_count,
        MAX_SENSOR_SAMPLES-10,
        nortekData.batt_v);

      //printing to ebox and writing to sd card
      printf(
        "DATA, %" PRIu64 ", %s, %u, %u, %u, %u, %u, %u, %u, %u, %.4f, %.4f, %.4f, %u, %u, %u, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %u, %u, %.4f, %.4f\n",
        this_uptime, rtcTimeBuffer,nortekData.month, nortekData.day, nortekData.year, nortekData.hour, nortekData.minute, nortekData.second, nortekData.error_code, nortekData.status_code, nortekData.v_b1_x, nortekData.v_b2_y, nortekData.v_b3_z, nortekData.amp_b1_x, nortekData.amp_b2_y, nortekData.amp_b3_z, nortekData.batt_v, nortekData.sound_speed, nortekData.heading, nortekData.pitch, nortekData.roll, nortekData.pressure, nortekData.temperature, nortekData.an_in_1, nortekData.an_in_2, nortekData.speed, nortekData.direction
      );
      spotter_log_console(0,
        "DATA, %" PRIu64 ", %s, %u, %u, %u, %u, %u, %u, %u, %u, %.4f, %.4f, %.4f, %u, %u, %u, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %u, %u, %.4f, %.4f\n",
        this_uptime, rtcTimeBuffer,nortekData.month, nortekData.day, nortekData.year, nortekData.hour, nortekData.minute, nortekData.second, nortekData.error_code, nortekData.status_code, nortekData.v_b1_x, nortekData.v_b2_y, nortekData.v_b3_z, nortekData.amp_b1_x, nortekData.amp_b2_y, nortekData.amp_b3_z, nortekData.batt_v, nortekData.sound_speed, nortekData.heading, nortekData.pitch, nortekData.roll, nortekData.pressure, nortekData.temperature, nortekData.an_in_1, nortekData.an_in_2, nortekData.speed, nortekData.direction
      );
      spotter_log(0,"nortek.log", USE_TIMESTAMP,
        "DATA, %" PRIu64 ", %s, %u, %u, %u, %u, %u, %u, %u, %u, %.4f, %.4f, %.4f, %u, %u, %u, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %u, %u, %.4f, %.4f\n",
        uptimeGetMs(), rtcTimeBuffer,nortekData.month, nortekData.day, nortekData.year, nortekData.hour, nortekData.minute, nortekData.second, nortekData.error_code, nortekData.status_code, nortekData.v_b1_x, nortekData.v_b2_y, nortekData.v_b3_z, nortekData.amp_b1_x, nortekData.amp_b2_y, nortekData.amp_b3_z, nortekData.batt_v, nortekData.sound_speed, nortekData.heading, nortekData.pitch, nortekData.roll, nortekData.pressure, nortekData.temperature, nortekData.an_in_1, nortekData.an_in_2, nortekData.speed, nortekData.direction
      );
    }
    else {
      printf("Error parsing line!\n");
      return;
    }

    static uint32_t sensorStatsTimer = uptimeGetMs();
    printf("\n\ntimer %" PRIu32 "\n\n",(uint32_t)uptimeGetMs() - sensorStatsTimer);
    net_time = uptimeGetMs();
    printf("global timer %" PRIu64 "\n\n",net_time);
    if ((uint32_t)uptimeGetMs() - sensorStatsTimer >= SENSOR_AGG_PERIOD_MS) { // Note - this is a "rollover safe" implementation.
      sensorStatsTimer = uptimeGetMs(); // when the timer triggers, reset the timer!

      v_b1_x_mean = (float) getMean(v_b1_x_stats.values, v_b1_x_stats.sample_count);
      v_b2_y_mean = (float) getMean(v_b2_y_stats.values, v_b2_y_stats.sample_count);
      v_b3_z_mean = (float) getMean(v_b3_z_stats.values, v_b3_z_stats.sample_count);
      heading_mean = (float) getMean(heading_stats.values, heading_stats.sample_count);
      pitch_mean = (float) getMean(pitch_stats.values, pitch_stats.sample_count);
      roll_mean = (float) getMean(roll_stats.values, roll_stats.sample_count);
      pressure_mean = (float) getMean(pressure_stats.values, pressure_stats.sample_count);
      temperature_mean = (float) getMean(temperature_stats.values, temperature_stats.sample_count);
      speed_mean = (float) getMean(speed_stats.values, speed_stats.sample_count);
      direction_mean = (float) getMean(direction_stats.values, direction_stats.sample_count);

      v_b1_x_stdev = (float) getStd(v_b1_x_stats.values, v_b1_x_stats.sample_count,v_b1_x_mean);
      v_b2_y_stdev = (float) getStd(v_b2_y_stats.values, v_b2_y_stats.sample_count,v_b2_y_mean);
      v_b3_z_stdev = (float) getStd(v_b3_z_stats.values, v_b3_z_stats.sample_count,v_b3_z_mean);
      heading_stdev = (float) getStd(heading_stats.values, heading_stats.sample_count,heading_mean);
      pitch_stdev = (float) getStd(pitch_stats.values, pitch_stats.sample_count,pitch_mean);
      roll_stdev = (float) getStd(roll_stats.values, roll_stats.sample_count,roll_mean);
      pressure_stdev = (float) getStd(pressure_stats.values, pressure_stats.sample_count,pressure_mean);
      temperature_stdev = (float) getStd(temperature_stats.values, temperature_stats.sample_count,temperature_mean);
      speed_stdev = (float) getStd(speed_stats.values, speed_stats.sample_count,speed_mean);
      direction_stdev = (float) getStd(direction_stats.values, direction_stats.sample_count,direction_mean);

      // Let's get UTC time string for convenience
      rtcPrint(rtcTimeBuffer, NULL);
      // printf(
      //     "[sensor-agg] | tick: %llu, rtc:%s, heading_n: %u, heading_mean: %.4f, heading_stdev: %.4f, pitch_n: %u, pitch_mean: %.4f, pitch_stdev: %.4f\n",
      //     uptimeGetMs(), rtcTimeBuffer, heading_stats.sample_count, heading_mean, heading_stdev, pitch_stats.sample_count, pitch_mean,
      //     pitch_stdev
      // );
      printf(
          "MEANS |  v_x %.4f, v_y %.4f, v_z %.4f, heading %.4f, pitch %.4f, roll %.4f, pressure %.4f, temperature %.4f, speed %.4f, direction %.4f\n\n",
          v_b1_x_mean,v_b2_y_mean,v_b3_z_mean,heading_mean,pitch_mean,roll_mean,pressure_mean,temperature_mean,speed_mean,direction_mean
      );
      printf(
          "STDEVS |  v_x %.4f, v_y %.4f, v_z %.4f, heading %.4f, pitch %.4f, roll %.4f, pressure %.4f, temperature %.4f, speed %.4f, direction %.4f\n\n",
          v_b1_x_stdev,v_b2_y_stdev,v_b3_z_stdev,heading_stdev,pitch_stdev,roll_stdev,pressure_stdev,temperature_stdev,speed_stdev,direction_stdev
      );

      spotter_log_console(0,
          "MEANS |  v_x %.4f, v_y %.4f, v_z %.4f, heading %.4f, pitch %.4f, roll %.4f, pressure %.4f, temperature %.4f, speed %.4f, direction %.4f\n\n",
          v_b1_x_mean,v_b2_y_mean,v_b3_z_mean,heading_mean,pitch_mean,roll_mean,pressure_mean,temperature_mean,speed_mean,direction_mean
      );
      spotter_log_console(0,
          "STDEVS |  v_x %.4f, v_y %.4f, v_z %.4f, heading %.4f, pitch %.4f, roll %.4f, pressure %.4f, temperature %.4f, speed %.4f, direction %.4f\n\n",
          v_b1_x_stdev,v_b2_y_stdev,v_b3_z_stdev,heading_stdev,pitch_stdev,roll_stdev,pressure_stdev,temperature_stdev,speed_stdev,direction_stdev
      );

      spotter_log(0,"nortek_averages.log", USE_TIMESTAMP,
          "DATA, %llu, %s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
          uptimeGetMs(), rtcTimeBuffer, v_b1_x_mean, v_b1_x_stdev, v_b2_y_mean, v_b2_y_stdev, v_b3_z_mean, v_b3_z_stdev, heading_mean, heading_stdev, pitch_mean, pitch_stdev, roll_mean, roll_stdev, pressure_mean, pressure_stdev, temperature_mean, temperature_stdev, speed_mean, speed_stdev, direction_mean, direction_stdev
      );

      static const uint8_t NUM_SENSORS = 10;
      // Create an array of stat agg structs.
      sensorStatAgg_t report_stats[NUM_SENSORS] = {};
      report_stats[0].sample_count = v_b1_x_stats.sample_count;
      report_stats[0].mean = v_b1_x_mean;
      report_stats[0].stdev = v_b1_x_stdev;
      report_stats[1].sample_count = v_b2_y_stats.sample_count;
      report_stats[1].mean = v_b2_y_mean;
      report_stats[1].stdev = v_b2_y_stdev;
      report_stats[2].sample_count = v_b3_z_stats.sample_count;
      report_stats[2].mean = v_b3_z_mean;
      report_stats[2].stdev = v_b3_z_stdev;
      report_stats[3].sample_count = heading_stats.sample_count;
      report_stats[3].mean = heading_mean;
      report_stats[3].stdev = heading_stdev;
      report_stats[4].sample_count = pitch_stats.sample_count;
      report_stats[4].mean = pitch_mean;
      report_stats[4].stdev = pitch_stdev;
      report_stats[5].sample_count = roll_stats.sample_count;
      report_stats[5].mean = roll_mean;
      report_stats[5].stdev = roll_stdev;
      report_stats[6].sample_count = pressure_stats.sample_count;
      report_stats[6].mean = pressure_mean;
      report_stats[6].stdev = pressure_stdev;
      report_stats[7].sample_count = temperature_stats.sample_count;
      report_stats[7].mean = temperature_mean;
      report_stats[7].stdev = temperature_stdev;
      report_stats[8].sample_count = speed_stats.sample_count;
      report_stats[8].mean = speed_mean;
      report_stats[8].stdev = speed_stdev;
      report_stats[9].sample_count = direction_stats.sample_count;
      report_stats[9].mean = direction_mean;
      report_stats[9].stdev = direction_stdev;
      // Simply clear the sample_count variables to reset the buffers.
      month_stats.sample_count = 0;
      day_stats.sample_count = 0;
      year_stats.sample_count = 0;
      hour_stats.sample_count = 0;
      minute_stats.sample_count = 0;
      second_stats.sample_count = 0;
      error_code_stats.sample_count = 0;
      status_code_stats.sample_count = 0;
      v_b1_x_stats.sample_count = 0;
      v_b2_y_stats.sample_count = 0;
      v_b3_z_stats.sample_count = 0;
      amp_b1_x_stats.sample_count = 0;
      amp_b2_y_stats.sample_count = 0;
      amp_b3_z_stats.sample_count = 0;
      batt_v_stats.sample_count = 0;
      sound_speed_stats.sample_count = 0;
      heading_stats.sample_count = 0;
      pitch_stats.sample_count = 0;
      roll_stats.sample_count = 0;
      pressure_stats.sample_count = 0;
      temperature_stats.sample_count = 0;
      an_in_1_stats.sample_count = 0;
      an_in_2_stats.sample_count = 0;
      speed_stats.sample_count = 0;
      direction_stats.sample_count = 0;

      // Set up raw bytes for Tx data buffer.
      uint8_t tx_data[sizeof(sensorStatAgg_t) * NUM_SENSORS] = {};

      for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        memcpy(tx_data + sizeof(sensorStatAgg_t) * i, reinterpret_cast<uint8_t *>(&report_stats[i]), sizeof(sensorStatAgg_t));
      }

      rtcPrint(rtcTimeBuffer, NULL);
      if(spotter_tx_data(tx_data, sizeof(sensorStatAgg_t) * NUM_SENSORS, BmNetworkTypeCellularIriFallback)){
        printf("%llut - %s | Sucessfully sent Spotter transmit data request\n", uptimeGetMs(), rtcTimeBuffer);
      }
      else {
        printf("%llut - %s | ERR Failed to send Spotter transmit data request!\n", uptimeGetMs(), rtcTimeBuffer);
      }
    }
    return;
  }
}






// v_b1_x_mean
// v_b1_x_stdev
// v_b2_y_mean
// v_b2_y_stdev
// v_b3_z_mean
// v_b3_z_stdev
// heading_mean
// heading_stdev
// pitch_mean
// pitch_stdev
// roll_mean
// roll_stdev
// pressure_mean
// pressure_stdev
// temperature_mean
// temperature_stdev
// speed_mean
// speed_stdev
// direction_mean
// direction_stdev

  // month;
  // day;
  // year;
  // hour;
  // minute;
  // second;
  // error_cod
  // status_co
  // v_b1_x;
  // v_b2_y;
  // v_b3_z;
  // amp_b1_x;
  // amp_b2_y;
  // amp_b3_z;
  // batt_v;
  // sound_speed
  // heading;
  // pitch;
  // roll;
  // pressure;
  // temperature
  // an_in_1;
  // an_in_2;
  // speed;
  // direction;


// nortekData.month, nortekData.day, nortekData.year, nortekData.hour, nortekData.minute, nortekData.second, nortekData.error_code, nortekData.status_code, nortekData.v_b1_x, nortekData.v_b2_y, nortekData.v_b3_z, nortekData.amp_b1_x, nortekData.amp_b2_y, nortekData.amp_b3_z, nortekData.batt_v, nortekData.sound_speed, nortekData.heading, nortekData.pitch, nortekData.roll, nortekData.pressure, nortekData.temperature, nortekData.an_in_1, nortekData.an_in_2, nortekData.speed, nortekData.direction

//// Values for running averages
// float heading_sum = 0;
// float pitch_sum = 0;
// float roll_sum = 0;
// float speed_sum = 0;
// float direction_sum = 0;
// float heading_sum_sq = 0;
// float pitch_sum_sq = 0;
// float roll_sum_sq = 0;
// float speed_sum_sq = 0;
// float direction_sum_sq = 0;
// float heading_mean = 0;
// float pitch_mean = 0;
// float roll_mean = 0;
// float speed_mean = 0;
// float direction_mean = 0;
// float heading_var = 0;
// float pitch_var = 0;
// float roll_var = 0;
// float speed_var = 0;
// float direction_var = 0;
// int16_t reading_count = 0;


// typedef struct {
//     uint8_t month;
//     uint8_t day;
//     uint16_t year;
//     uint8_t hour;
//     uint8_t minute;
//     uint8_t second;
//     uint16_t error_code;
//     uint16_t status_code;
//     double v_b1_x;
//     double v_b2_y;
//     double v_b3_z;
//     uint16_t amp_b1_x;
//     uint16_t amp_b2_y;
//     uint16_t amp_b3_z;
//     double batt_v;
//     double sound_speed;
//     double heading;
//     double pitch;
//     double roll;
//     double pressure;   //need
//     double temperature;     //need
//     double an_in_1;
//     double an_in_2;
//     double speed;
//     double direction;
// } __attribute__((__packed__))  NortekData_t;

// NortekData_t nortekData = {};

// void fillNortekStruct(){
//     nortekData.month = parser.getValue(0).data.uint64_val;
//     nortekData.day = parser.getValue(1).data.uint64_val;
//     nortekData.year = parser.getValue(2).data.uint64_val;
//     nortekData.hour = parser.getValue(3).data.uint64_val;
//     nortekData.minute = parser.getValue(4).data.uint64_val;
//     nortekData.second = parser.getValue(5).data.uint64_val;
//     nortekData.error_code = parser.getValue(6).data.uint64_val;
//     nortekData.status_code = parser.getValue(7).data.uint64_val;
//     nortekData.v_b1_x = parser.getValue(8).data.double_val;
//     nortekData.v_b2_y = parser.getValue(9).data.double_val;
//     nortekData.v_b3_z = parser.getValue(10).data.double_val;
//     nortekData.amp_b1_x = parser.getValue(11).data.uint64_val;
//     nortekData.amp_b2_y = parser.getValue(12).data.uint64_val;
//     nortekData.amp_b3_z = parser.getValue(13).data.uint64_val;
//     nortekData.batt_v = parser.getValue(14).data.double_val;
//     nortekData.sound_speed = parser.getValue(15).data.double_val;
//     nortekData.heading = parser.getValue(16).data.double_val;
//     nortekData.pitch = parser.getValue(17).data.double_val;
//     nortekData.roll = parser.getValue(18).data.double_val;
//     nortekData.pressure = parser.getValue(19).data.double_val;
//     nortekData.temperature = parser.getValue(20).data.double_val;
//     nortekData.an_in_1 = parser.getValue(21).data.double_val;
//     nortekData.an_in_2 = parser.getValue(22).data.double_val;
//     nortekData.speed = parser.getValue(23).data.double_val;
//     nortekData.direction = parser.getValue(24).data.double_val;
// }

// float recoverVariance(float sum,float sumsq){
//     return (sumsq-((sum*sum)/reading_count))/reading_count;
// }

// void updateRunningAverages(NortekData_t &data){
//     reading_count +=1;
//     heading_sum += data.heading;
//     pitch_sum += data.pitch;
//     roll_sum += data.roll;
//     speed_sum += data.speed;
//     direction_sum += data.direction;

//     heading_sum_sq += (data.heading*data.heading);
//     pitch_sum_sq += (data.pitch*data.pitch);
//     roll_sum_sq+= (data.roll*data.roll);
//     speed_sum_sq += (data.speed*data.speed);
//     direction_sum_sq += (data.direction*data.direction);
// }
