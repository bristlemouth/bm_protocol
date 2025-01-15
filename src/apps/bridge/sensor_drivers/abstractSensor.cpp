#include "abstractSensor.h"
#include "bm_config.h"
#include "bm_os.h"
#include "bridgeLog.h"
#include "stm32_rtc.h"
#include "topology_sampler.h"
#include <inttypes.h>
#include <string.h>

#define GET_S_FROM_MS(ms) (ms / 1000U)
#define GET_MS_LEFT(ms) (ms % 1000U)

/*!
 @brief Constructor For Abstract Sensor
 */
AbstractSensor::AbstractSensor() {
  m_position.last_timestamp_ms = 0;
  m_position.first_time = true;
  m_position.node_position = 0;
}

/*!
 @brief Updates Node Position Of A Sensor

 @details Will update the nodes position on the first time that this function
          is ran, or if the latest reading from the node is longer than maximum
          expected reading time between successive reads.

 @param max_reading_period_ms max reading time between successive readings

 @return The position of the node in the node list or -1 if not found
 */
int8_t AbstractSensor::update_node_position(uint32_t max_reading_period_ms) {
  uint32_t current_timestamp_ms = bm_ticks_to_ms(bm_get_tick_count());
  if ((current_timestamp_ms - m_position.last_timestamp_ms > max_reading_period_ms) ||
      m_position.first_time) {
    printf("Updating %016" PRIx64 " node m_position, current_time = %" PRIu32
           ", last_time = %" PRIu32 "\n",
           node_id, current_timestamp_ms, m_position.last_timestamp_ms);
    m_position.node_position = topology_sampler_get_node_position(node_id, 5000);
    m_position.first_time = false;
  }
  m_position.last_timestamp_ms = current_timestamp_ms;
  return m_position.node_position;
}

/*!
 @brief Format And Send An Individual Sensor LogPrintf Message

 @details Provides a generic way to format and send individual sensor readings.

 @param app_name name of the bristlemouth node's application
 @param header sensor header
 @param max_reading_period_ms max reading time between successive readings
 @param fmt format string of the data to be logged
 @param ... formatted string arguments

 @return BmOK on success
 @return BmErr on failure
 */
BmErr AbstractSensor::send_spotter_log_individual(const char *app_name,
                                                  SensorHeaderMsg::Data header,
                                                  uint32_t max_reading_period_ms,
                                                  const char *fmt, ...) {
  BmErr err = BmENOMEM;
  char *log_buf = static_cast<char *>(bm_malloc(SENSOR_LOG_BUF_SIZE));
  va_list args;

  va_start(args, fmt);

  if (log_buf) {
    do {
      int fixed_length = snprintf(
          log_buf, SENSOR_LOG_BUF_SIZE,
          "%016" PRIx64 "," // Node Id
          "%" PRIi8 ","     // node_position
          "%s,"             // node_app_name
          "%" PRIu64 ","    // reading_uptime_millis
          "%" PRIu64 "."    // reading_time_utc_ms seconds part
          "%03" PRIu32 ","  // reading_time_utc_ms millis part
          "%" PRIu64 "."    // sensor_reading_time_ms seconds part
          "%03" PRIu32 ",", // sensor_reading_time_ms millis part
          node_id, update_node_position(max_reading_period_ms), app_name,
          header.reading_uptime_millis, GET_S_FROM_MS(header.reading_time_utc_ms),
          GET_MS_LEFT(header.reading_time_utc_ms), GET_S_FROM_MS(header.sensor_reading_time_ms),
          GET_MS_LEFT(header.sensor_reading_time_ms));

      if (fixed_length <= 0) {
        bm_debug("Failed to print %s individual log\n", app_name);
        err = BmENODATA;
        break;
      }
      int var_length =
          vsnprintf(log_buf + fixed_length, SENSOR_LOG_BUF_SIZE - fixed_length, fmt, args);

      if (var_length <= 0) {
        bm_debug("Failed to print %s individual log\n", app_name);
        err = BmENODATA;
        break;
      }

      size_t total_length = (size_t)(var_length + fixed_length);
      BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_IND, log_buf, total_length);
      err = BmOK;
    } while (0);
    bm_free(log_buf);
  }

  va_end(args);

  return err;
}

/*!
 @brief Format And Send An Aggregate Sensor LogPrintf Message

 @details Provides a generic way to format and send aggregate sensor readings.

 @param app_name name of the bristlemouth node's application
 @param reading_count number of readings aggregated
 @param fmt format string of the data to be logged
 @param ... formatted string arguments

 @return BmOK on success
 @return BmErr on failure
 */
BmErr AbstractSensor::send_spotter_log_aggregate(const char *app_name, uint32_t reading_count,
                                                 const char *fmt, ...) {
  BmErr err = BmENOMEM;
  char *log_buf = static_cast<char *>(bm_malloc(SENSOR_LOG_BUF_SIZE));
  va_list args;

  static constexpr uint8_t TIME_STR_BUFSIZE = 50;
  char time_str[TIME_STR_BUFSIZE];
  if (!logRtcGetTimeStr(time_str, TIME_STR_BUFSIZE, true)) {
    bm_debug("Failed to get RTC time string for %s aggregation\n", app_name);
    snprintf(time_str, TIME_STR_BUFSIZE, "0");
  }

  va_start(args, fmt);

  if (log_buf) {
    do {
      int fixed_length = snprintf(log_buf, SENSOR_LOG_BUF_SIZE,
                                  "%016" PRIx64 "," // Node Id
                                  "%" PRIi8 ","     // node_position
                                  "%s,"             // node_app_name
                                  "%s,"             // timestamp(ticks/UTC)
                                  "%" PRIu32 ",",   // reading_count
                                  node_id, topology_sampler_get_node_position(node_id, 5000),
                                  app_name, time_str, reading_count);

      if (fixed_length <= 0) {
        bm_debug("Failed to print %s aggregate log\n", app_name);
        err = BmENODATA;
        break;
      }

      int var_length =
          vsnprintf(log_buf + fixed_length, SENSOR_LOG_BUF_SIZE - fixed_length, fmt, args);

      if (var_length <= 0) {
        bm_debug("Failed to print %s aggregate log\n", app_name);
        err = BmENODATA;
        break;
      }

      size_t total_length = (size_t)(var_length + fixed_length);
      BRIDGE_SENSOR_LOG_PRINTN(BM_COMMON_AGG, log_buf, total_length);
      err = BmOK;
    } while (0);
    bm_free(log_buf);
  }

  // Need to reset this!
  m_position.first_time = true;

  va_end(args);
  return err;
}
