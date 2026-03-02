#include "user_code.h"
#include "bm_os.h"
#include "power_battery_msg.h"
#include "power_battery_averages_msg.h"
#include "power_solar_reading_msg.h"
#include "power_solar_averages_msg.h"
#include "pubsub.h"
#include "uptime.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
This application demonstrates how to subscribe to a data topic over bristlemouth via the bm_sub function.
The application subscribes to the topic "pubsub_example" and prints the received message to the console.
The callback function subscribe_callback is called when a message is received.
To test this application, you will need to run the pub_example application on another node.
*/


// TODO - Update these!
// The topic type is a identifier for the topic to encode different data types, we don't need to worry about this value for the tutorial.
static constexpr uint32_t EXAMPLE_SUBSCRIBE_TOPIC_TYPE = (1);
// The topic version is a version number for the topic, in case things change we don't need to worry about this value for the tutorial.
static constexpr uint32_t EXAMPLE_SUBSCRIBE_TOPIC_VERSION = (1);
// This is the topic to subscribe to, the publisher will need to publish to this topic (see the application pub_example)
static const char *const POWER_BATTERY_AVGS_TOPIC = "sensor/*/power/battery/avgs";
static const char *const POWER_SOLAR_TOPIC = "sensor/*/power/solar";
static const char *const POWER_SOLAR_AVGS_TOPIC = "sensor/*/power/solar/avgs";

static void power_battery_avgs_callback(uint64_t node_id, const char *topic, uint16_t topic_len,
                           const uint8_t *data, uint16_t data_len, uint8_t type,
                           uint8_t version) {
  (void)topic_len;
  (void)data_len;
  (void)topic;
  (void)node_id;

  if (type != 1 || version != 1) {
    printf("version or type incorrect\n");
    return;
  }

  PowerBatteryAveragesMsg::Data d = {};
  CborError err = PowerBatteryAveragesMsg::decode(d, data, data_len);
  if (err == CborNoError) {
    printf("Spotter battery averages data:\n");
    printf("\tpower_reading_type: %d\n", d.power_reading_type);
    printf("\tstatus: %d\n", d.status);
    printf("\tnum_samples: %lu\n", d.num_samples);
    printf("\taveraging_window_length_s: %f\n", d.averaging_window_length_s);
    printf("\tnum_cell_voltages: %d\n", d.num_cell_voltages);
    for (uint8_t i = 0; i < d.num_cell_voltages; i++) {
      printf("\tcell %d - voltage avg: %.3f, max: %.3f, min: %.3f, stdev: %.3f\n",
             i, d.cell_voltage_v_avg[i], d.cell_voltage_v_max[i],
             d.cell_voltage_v_min[i], d.cell_voltage_v_stdev[i]);
    }
    printf("\tnum_temp_sensors: %d\n", d.num_temp_sensors);
    for (uint8_t i = 0; i < d.num_temp_sensors; i++) {
      printf("\tcell %d - temp avg: %.3f, max: %.3f, min: %.3f, stdev: %.3f\n",
             i, d.cell_temperature_c_avg[i], d.cell_temperature_c_max[i],
             d.cell_temperature_c_min[i], d.cell_temperature_c_stdev[i]);
    }
  } else {
    printf("Failed to decode the battery averages message!\n");
  }
  bm_free(d.cell_voltage_v_avg);
  bm_free(d.cell_voltage_v_max);
  bm_free(d.cell_voltage_v_min);
  bm_free(d.cell_voltage_v_stdev);
  bm_free(d.cell_temperature_c_avg);
  bm_free(d.cell_temperature_c_max);
  bm_free(d.cell_temperature_c_min);
  bm_free(d.cell_temperature_c_stdev);
}

static void power_solar_callback(uint64_t node_id, const char *topic, uint16_t topic_len,
                           const uint8_t *data, uint16_t data_len, uint8_t type,
                           uint8_t version) {
  (void)topic_len;
  (void)data_len;
  (void)topic;
  (void)node_id;

  if (type != 1 || version != 1) {
    printf("version or type incorrect\n");
    return;
  }

  PowerSolarReadingMsg::Data d = {};
  CborError err = PowerSolarReadingMsg::decode(d, data, data_len);
  if (err == CborNoError) {
    printf("Spotter solar reading data:\n");
    printf("\tpower_reading_type: %d\n", d.power_reading_type);
    printf("\tstatus: %d\n", d.status);
    printf("\tvoltage_v: %.3f\n", d.voltage_v);
    printf("\tcurrent_a: %.3f\n", d.current_a);
    printf("\tmpp_position: %.3f\n", d.mpp_position);
    printf("\tnum_temp_sensors: %d\n", d.num_temp_sensors);
    for (uint8_t i = 0; i < d.num_temp_sensors; i++) {
      printf("\ttemp_sensor %d - temp: %.3f\n",
             i, d.panel_temperatures[i]);
    }
    printf("\tnum_lines: %d\n", d.num_lines);
    for (uint8_t i = 0; i < d.num_lines; i++) {
      printf("\tline %d - voltage: %.3f, current: %.3f\n",
             i, d.panel_voltages[i], d.panel_currents[i]);
    }
  } else {
    printf("Failed to decode the solar reading message!\n");
  }
  bm_free(d.panel_temperatures);
  bm_free(d.panel_voltages);
  bm_free(d.panel_currents);
}

// static void power_solar_avgs_callback(uint64_t node_id, const char *topic, uint16_t topic_len,
//                            const uint8_t *data, uint16_t data_len, uint8_t type,
//                            uint8_t version) {
//   (void)topic_len;
//   (void)data_len;
//   (void)topic;
//   (void)node_id;

//   if (type != 1 || version != 1) {
//     printf("version or type incorrect\n");
//     return;
//   }

//   PowerSolarAveragesMsg::Data d = {};
//   CborError err = PowerSolarAveragesMsg::decode(d, data, data_len);
//   if (err == CborNoError) {
//     printf("Spotter solar averages data:\n");
//     printf("\tnum_samples: %lu\n", d.num_samples);
//     printf("\taveraging_window_length_s: %.3f\n", d.averaging_window_length_s);
//     printf("\tnum_temp_sensors: %d\n", d.num_temp_sensors);
//     printf("\tnum_lines: %d\n", d.num_lines);

//     printf("\tPanel Temperatures:\n");
//     for (uint8_t i = 0; i < d.num_temp_sensors; i++) {
//       printf("\t  sensor %d - avg: %.3f, max: %.3f, min: %.3f, stdev: %.3f\n",
//              i, d.panel_temperatures_average[i], d.panel_temperatures_max[i],
//              d.panel_temperatures_min[i], d.panel_temperatures_stdev[i]);
//     }

//     printf("\tPanel Voltages:\n");
//     for (uint8_t i = 0; i < d.num_lines; i++) {
//       printf("\t  line %d - avg: %.3f, max: %.3f, min: %.3f, stdev: %.3f\n",
//              i, d.panel_voltages_average[i], d.panel_voltages_max[i],
//              d.panel_voltages_min[i], d.panel_voltages_stdev[i]);
//     }

//     printf("\tPanel Currents:\n");
//     for (uint8_t i = 0; i < d.num_lines; i++) {
//       printf("\t  line %d - avg: %.3f, max: %.3f, min: %.3f, stdev: %.3f\n",
//              i, d.panel_currents_average[i], d.panel_currents_max[i],
//              d.panel_currents_min[i], d.panel_currents_stdev[i]);
//     }
//   } else {
//     printf("Failed to decode the solar averages message!\n");
//   }
//   bm_free(d.panel_temperatures_average);
//   bm_free(d.panel_temperatures_max);
//   bm_free(d.panel_temperatures_min);
//   bm_free(d.panel_temperatures_stdev);
//   bm_free(d.panel_voltages_average);
//   bm_free(d.panel_voltages_max);
//   bm_free(d.panel_voltages_min);
//   bm_free(d.panel_voltages_stdev);
//   bm_free(d.panel_currents_average);
//   bm_free(d.panel_currents_max);
//   bm_free(d.panel_currents_min);
//   bm_free(d.panel_currents_stdev);
// }

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  // Subscribe to the topic and provide the callback function to be called when a message is received.
  bm_sub(POWER_BATTERY_AVGS_TOPIC, power_battery_avgs_callback);
  // bm_sub(POWER_SOLAR_AVGS_TOPIC, power_solar_avgs_callback);
  bm_sub(POWER_SOLAR_TOPIC, power_solar_callback);
}

void loop(void) {
  // Nothing to do!
  // The callback function will be called independently from this loop
  // when a message is received.
}
