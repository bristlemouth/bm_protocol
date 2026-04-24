#include "user_code.h"
#include "bm_os.h"
#include "power_battery_averages_msg.h"
#include "power_solar_averages_msg.h"
#include "pubsub.h"
#include "uptime.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
This application demonstrates how to subscribe to power averages and solar averages topics
over bristlemouth via the bm_sub function.
*/

// This is the topic to subscribe to, the publisher will need to publish to this topic (see the application pub_example)
static const char *const POWER_BATTERY_AVGS_TOPIC = "sensor/*/power/battery/avgs";
static const char *const POWER_SOLAR_AVGS_TOPIC = "sensor/*/power/solar/avgs";

static void power_battery_avgs_callback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                        const uint8_t *data, uint16_t data_len, uint8_t type,
                                        uint8_t version) {
  (void)topic_len;
  (void)topic;
  (void)type;

  /*
      Print out the data. Here you could take actions based on the power data.
      If you are passing variables in/out of this function they must be mutex
      protected since this callback is run in the middleware task.
    */

  if (version != PowerBatteryAveragesMsg::VERSION) {
    printf("version incorrect\n");
    return;
  }

  PowerBatteryAveragesMsg::Data d = {};
  CborError err = PowerBatteryAveragesMsg::decode(d, data, data_len);
  if (err == CborNoError) {
    printf("Node ID %" PRIx64 " battery averages data:\n", node_id);
    printf("\tpower_reading_type: %d\n", d.power_reading_type);
    printf("\tstatus: %d\n", d.status);
    printf("\tnum_samples: %lu\n", d.num_samples);
    printf("\taveraging_window_length_s: %f\n", d.averaging_window_length_s);
    printf("\tnum_cell_voltages: %d\n", d.num_cell_voltages);
    for (uint8_t i = 0; i < d.num_cell_voltages; i++) {
      printf("\tcell %d - voltage avg: %.3f, max: %.3f, min: %.3f, stdev: %.3f\n", i,
             d.cell_voltage_v_avg[i], d.cell_voltage_v_max[i], d.cell_voltage_v_min[i],
             d.cell_voltage_v_stdev[i]);
    }
    printf("\tnum_temp_sensors: %d\n", d.num_temp_sensors);
    for (uint8_t i = 0; i < d.num_temp_sensors; i++) {
      printf("\tcell %d - temp avg: %.3f, max: %.3f, min: %.3f, stdev: %.3f\n", i,
             d.cell_temperature_c_avg[i], d.cell_temperature_c_max[i],
             d.cell_temperature_c_min[i], d.cell_temperature_c_stdev[i]);
    }
  } else {
    printf("Failed to decode the battery averages message!\n");
  }
  PowerBatteryAveragesMsg::free(d);
}

static void power_solar_avgs_callback(uint64_t node_id, const char *topic, uint16_t topic_len,
                                      const uint8_t *data, uint16_t data_len, uint8_t type,
                                      uint8_t version) {
  (void)topic_len;
  (void)topic;
  (void)type;

  if (version != PowerSolarAveragesMsg::VERSION) {
    printf("version or type incorrect\n");
    return;
  }

  PowerSolarAveragesMsg::Data d = {};
  CborError err = PowerSolarAveragesMsg::decode(d, data, data_len);
  if (err == CborNoError) {

    /*
      Print out the data. Here you could take actions based on the power data.
      If you are passing variables in/out of this function they must be mutex
      protected since this callback run in the middleware task.
    */

    printf("Node ID %" PRIx64 " solar averages data:\n", node_id);
    printf("\tnum_samples: %lu\n", d.num_samples);
    printf("\taveraging_window_length_s: %.3f\n", d.averaging_window_length_s);
    printf("\tnum_temp_sensors: %d\n", d.num_temp_sensors);
    printf("\tnum_lines: %d\n", d.num_lines);

    printf("\tPanel Temperatures:\n");
    for (uint8_t i = 0; i < d.num_temp_sensors; i++) {
      printf("\t  sensor %d - avg: %.3f, max: %.3f, min: %.3f, stdev: %.3f\n", i,
             d.panel_temperatures_average[i], d.panel_temperatures_max[i],
             d.panel_temperatures_min[i], d.panel_temperatures_stdev[i]);
    }

    printf("\tPanel Voltages:\n");
    for (uint8_t i = 0; i < d.num_lines; i++) {
      printf("\t  line %d - avg: %.3f, max: %.3f, min: %.3f, stdev: %.3f\n", i,
             d.panel_voltages_average[i], d.panel_voltages_max[i], d.panel_voltages_min[i],
             d.panel_voltages_stdev[i]);
    }

    printf("\tPanel Currents:\n");
    for (uint8_t i = 0; i < d.num_lines; i++) {
      printf("\t  line %d - avg: %.3f, max: %.3f, min: %.3f, stdev: %.3f\n", i,
             d.panel_currents_average[i], d.panel_currents_max[i], d.panel_currents_min[i],
             d.panel_currents_stdev[i]);
    }
  } else {
    printf("Failed to decode the solar averages message!\n");
  }
  PowerSolarAveragesMsg::free(d);
}

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  // Subscribe to the topics and provide the callback functions to be called when a message is received.
  bm_sub(POWER_BATTERY_AVGS_TOPIC, power_battery_avgs_callback);
  bm_sub(POWER_SOLAR_AVGS_TOPIC, power_solar_avgs_callback);
}

void loop(void) {
  // Nothing to do!
  // The callback function will be called independently from this loop
  // when a message is received.
}
