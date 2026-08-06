#ifndef __AANDERAA_SENSOR_H__
#define __AANDERAA_SENSOR_H__

extern "C" {
#include "util.h"
}
#include "debug.h"
#include "payload_uart.h"
#include "task.h"
#include "uptime.h"
#include <string.h>

#define CMD_STOP "Stop\r\n"
#define CMD_START "Start\r\n"

#define CMD_SET_PASSKEY_1 "Set Passkey(1)\r\n"
#define CMD_SET_PASSKEY_1000 "Set Passkey(1000)\r\n"

#define CMD_FLOW_CONTROL "Flow Control"
#define CMD_MODE "Mode"

#define CMD_ENABLE_SLEEP "Enable Sleep"
#define CMD_ENABLE_POLLEDMODE "Enable Polled Mode"
#define CMD_ENABLE_TEXT "Enable Text"
#define CMD_ENABLE_DECIMALFORMAT "Enable Decimalformat"
#define CMD_INTERVAL "Interval"

#define CMD_COMM_TIMEOUT "Comm TimeOut"

#define CMD_GET_ALL "Get_All\r\n"
#define CMD_GET_ALL_PARAMS "Get_All Parameters\r\n"

#define CMD_GET_SERIAL_NUMBER "Get Serial Number\r\n"

#define CMD_SAVE "Save\r\n"
#define CMD_RESET "Reset\r\n"

#define CMD_GET "Get"
#define CMD_SET "Set"

#define CMD_YES "Yes"
#define CMD_NO "No"

#define ACK "#"

#define CMD_WAKE "\r\n"

class AanderaaSensor {

public:
  typedef uint32_t AanderaaUint;
  typedef float AanderaaFloat;
  typedef char AanderaaString[64];

  BmErr wakeSensor(void);
  void startStreaming(void);
  void stopStreaming(void);
  BmErr setDefaultConfigs(void);
  BmErr saveConfiguration(void);
  void resetSensor(uint32_t timeout_ms = 5000);

  void getSensorHelp(void);
  void getAllConfigurationParameters(void);

private:
  void clearCmdBuffer(void);

  void printLongOutput(const char *command);

  bool compareValuesPopulateBuffer(const char *parameter, AanderaaString *buf,
                                   const AanderaaUint read, const AanderaaUint validate);
  bool compareValuesPopulateBuffer(const char *parameter, AanderaaString *buf,
                                   const AanderaaFloat read, const AanderaaFloat validate);
  bool compareValuesPopulateBuffer(const char *parameter, AanderaaString *buf,
                                   const AanderaaString read, const AanderaaString validate);
  char _cmd_buffer[248] = {};
  bool _sensorConfigDirty = false;

protected:
  // The save procedure may take up to 20 seconds according to Table 5-2 in the
  // TD321 Operation Manual
  static constexpr uint16_t _saveTimeMs = 20000;
  // Commands are ended with \r\n
  static constexpr char LINE_TERM = '\n';

  BmErr sendCommand(const char *command, uint32_t timeout_ms = 1000);
  void checkTypeAndAssign(const char *output, uint16_t length, AanderaaUint *value);
  void checkTypeAndAssign(const char *output, uint16_t length, AanderaaFloat *value);
  void checkTypeAndAssign(const char *output, uint16_t length, AanderaaString *value);
  BmErr readValidateWriteValue(const char *parameter, const char *expected_val,
                               uint8_t retries = 3);

  /*!
   @brief Send Command To Aanderaa Sensor And Optionally Retrieve Response Value

   @details Sends a command string to the Aanderaa sensor via UART and waits for
            an acknowledgment response. If a value pointer is provided, this function
            will parse the sensor's response and extract the value after the last tab
            character. The function waits byte-by-byte for responses, handling both
            simple acknowledgments, and full data responses with values.
            Acknowledgment codes follow TD321 Operation Manual section 5.5.

   @param command the command string to send to the sensor
   @param value pointer to store the parsed response value (can be NULL if no value needed)
   @param timeout_ms timeout in milliseconds to wait for sensor acknowledgment

   @return BmOK on success
   @return BmEINVAL if command is NULL or timeout_ms is 0
   @return BmETIMEDOUT if no response received within timeout
   @return BmEBADMSG if sensor responds with error acknowledgment ('*')
   */
  template <typename T>
  BmErr sendCommand(const char *command, T *value, uint32_t timeout_ms = 1000) {
    BmErr err = BmEINVAL;

    if (!command || !timeout_ms) {
      return err;
    }

    constexpr uint8_t wait_read_tick = pdMS_TO_TICKS(1);
    PLUART::flush();
    clearCmdBuffer();
    uint32_t start_time = uptimeGetMs();
    err = BmETIMEDOUT;
    uint16_t buf_idx = 0;

    debug_printf("command: %s", command);
    // Send the command and wait for acknowledgement
    PLUART::write((uint8_t *)command, strlen(command));
    while ((uptimeGetMs() - start_time) < timeout_ms) {
      if (!PLUART::byteAvailable()) {
        // Delay for UART task to process incoming bytes
        vTaskDelay(wait_read_tick);
        continue;
      }

      _cmd_buffer[buf_idx] = PLUART::readByte();
      // Some responses do not finish with a new line such as '!' and '%'
      // see section 5.4 in TD321 Operation Manual
      if (!buf_idx) {
        if (_cmd_buffer[buf_idx] == '!') {
          err = BmOK;
          break;
        }
      }

      if (_cmd_buffer[buf_idx] == '\n') {
        debug_printf("command response: %.*s\n", buf_idx, _cmd_buffer);

        // read value after 3rd tab if it is a get command
        char *last_tab = strrchr(_cmd_buffer, '\t');
        if (last_tab != NULL && value) {
          // +1 to skip the tab
          last_tab++;
          last_tab[strcspn(last_tab, "\r\n")] = '\0';
          checkTypeAndAssign(last_tab, strlen(last_tab), value);
        }

        // Acknowledge for message reports '*' followed by a string for a failure and
        // '#' for success, see section 5.5 in TD321 Operation Manual
        // Sometimes there is junk before # on CMD_SAVE, the ack_idx accounts for that
        uint8_t ack_idx = buf_idx > sizeof("\r\n") ? buf_idx - 2 : 0;
        if (_cmd_buffer[ack_idx] == '#') {
          err = BmOK;
          break;
        } else if (_cmd_buffer[0] == '*') {
          err = BmEBADMSG;
          break;
        }

        buf_idx = 0;
      } else {
        buf_idx++;
      }
    }

    debug_printf("%s err: %d\n", __func__, err);

    return err;
  }

  /*!
   @brief Reads/Validate/Write a Parameter On The 5990

   @details This function will get a specified parameter from the 5990, compare that
            parameter to an expected value and then proceed to write that parameter
            if the read parameter is not expected. This will then mark the 5990's 
            config parameters as dirty and will save the configuration to the
            5990 at the end of configureSensor. Retries are implemented to
            ensure that the get/set commands are able to be invoked properly.

   @param parameter parameter to get/set
   @param expected_val expected value from get command
   @param retries number of times to retry getting/setting the parameter

   @return BmOK on successful write or if the expected value matches the read value
   @return BmEINVAL if command is NULL
   @return BmETIMEDOUT if no response received within timeout
   @return BmEBADMSG if sensor responds with error acknowledgment ('*')
   */
  template <typename T>
  BmErr readValidateWriteValue(const char *parameter, T expected_val, uint8_t retries = 3) {
    T read_val;
    AanderaaString command_buf = {};
    BmErr ret = BmOK;

    do {
      snprintf(command_buf, sizeof(command_buf), "%s %s\r\n", CMD_GET, parameter);

      ret = sendCommand(command_buf, &read_val);
      if (ret != BmOK) {
        continue;
      }

      if (compareValuesPopulateBuffer(parameter, &command_buf, read_val, expected_val)) {
        ret = BmOK;
        break;
      }

      ret = sendCommand(command_buf);

      if (ret == BmOK) {
        _sensorConfigDirty = true;
      }
    } while (ret != BmOK && retries--);

    return ret;
  }
};

#endif
