#include "aanderaa_sensor.h"
#include "bm_config.h"

void AanderaaSensor::startStreaming(void) { sendCommand(CMD_START); }

void AanderaaSensor::stopStreaming(void) { sendCommand(CMD_STOP); }

BmErr AanderaaSensor::setDefaultConfigs(void) {
  // passkey command
  BmErr err = sendCommand(CMD_SET_PASSKEY_1000);

  // enable sleep
  bm_err_check(err, readValidateWriteValue(CMD_ENABLE_SLEEP, CMD_YES));

  // set sleep timeout to 10s
  bm_err_check(err, readValidateWriteValue(CMD_COMM_TIMEOUT, "10 s"));

  // set lower priveledge level
  bm_err_check(err, sendCommand(CMD_SET_PASSKEY_1));

  //  disable Polled Mode
  bm_err_check(err, readValidateWriteValue(CMD_ENABLE_POLLEDMODE, CMD_NO));

  // disable text
  bm_err_check(err, readValidateWriteValue(CMD_ENABLE_TEXT, CMD_NO));

  // enable decimalformat
  bm_err_check(err, readValidateWriteValue(CMD_ENABLE_DECIMALFORMAT, CMD_YES));

  return err;
}

BmErr AanderaaSensor::saveConfiguration(void) {
  if (!_sensorConfigDirty) {
    return BmEALREADY;
  }

  sendCommand(CMD_SAVE, _saveTimeMs);
  _sensorConfigDirty = false;

  return BmOK;
}

void AanderaaSensor::resetSensor(uint32_t timeout_ms) { sendCommand(CMD_RESET, timeout_ms); }

/*!
 @brief Parse And Assign Unsigned Integer Value From Sensor Output String

 @param output the output string from the sensor to parse
 @param length the length of the output string
 @param value pointer to store the parsed unsigned integer value
 */
void AanderaaSensor::checkTypeAndAssign(const char *output, uint16_t length,
                                        AanderaaSensor::AanderaaConductivityUint *value) {
  (void)length;

  if (value) {
    *value = (uint32_t)strtoul(output, NULL, 10);
  }
}

/*!
 @brief Parse And Assign Float Value From Sensor Output String

 @param output the output string from the sensor to parse
 @param length unused
 @param value pointer to store the parsed float value
 */
void AanderaaSensor::checkTypeAndAssign(const char *output, uint16_t length,
                                        AanderaaSensor::AanderaaConductivityFloat *value) {
  (void)length;

  if (value) {
    *value = strtof(output, NULL);
  }
}

/*!
 @brief Copy And Assign String Value From Sensor Output String

 @param output the output string from the sensor to copy
 @param length the length of the output string
 @param value pointer to string buffer to store the copied string
 */
void AanderaaSensor::checkTypeAndAssign(const char *output, uint16_t length,
                                        AanderaaSensor::AanderaaConductivityString *value) {
  const size_t copy_len = sizeof(AanderaaConductivityString) - 1;

  if (length > copy_len) {
    return;
  }

  if (value) {
    strncpy(*value, output, copy_len);
    (*value)[copy_len] = '\0';
  }
}

/*!
 @brief Send Command To Aanderaa Sensor Without Retrieving Response Value

 @details Sends a command string to the Aanderaa sensor via UART and waits for
          an acknowledgment response. This is a convenience wrapper for commands
          that don't need to retrieve a value.

 @param command the command string to send to the sensor
 @param timeout_ms timeout in milliseconds to wait for sensor acknowledgment

 @return BmOK on success
 @return BmEINVAL if command is NULL or timeout_ms is 0
 @return BmETIMEDOUT if no response received within timeout
 @return BmEBADMSG if sensor responds with error acknowledgment
 */
BmErr AanderaaSensor::sendCommand(const char *command, uint32_t timeout_ms) {
  return sendCommand(command, static_cast<uint32_t *>(nullptr), timeout_ms);
}

/*!
 @brief Clear buffer used for 
 */
void AanderaaSensor::clearCmdBuffer(void) { memset(_cmd_buffer, 0, sizeof(_cmd_buffer)); }

/*!
 @brief Compares UINT Values And Populates Set Buffer To Send Values

 @details If the values for read and expected do not match, the buffer used to
          set the parameter is populated.

 @param parameter parameter to set if values do not match
 @param buf buffer to populate to send the set command
 @param read read value from 5990
 @param expected expected value

 @return true if the read value matches the expected value
         false if the read value does not match the expected value
 */
bool AanderaaSensor::compareValuesPopulateBuffer(
    const char *parameter, AanderaaSensor::AanderaaConductivityString *buf,
    const AanderaaSensor::AanderaaConductivityUint read,
    const AanderaaSensor::AanderaaConductivityUint expected) {

  if (read == expected) {
    return true;
  }

  snprintf(*buf, sizeof(AanderaaConductivityString), "%s %s(%" PRIu32 ")\r\n", CMD_SET,
           parameter, expected);
  return false;
}

/*!
 @brief Compares Float Values And Populates Set Buffer To Send Values

 @details If the values for read and expected do not match, the buffer used to
          set the parameter is populated.

 @param parameter parameter to set if values do not match
 @param buf buffer to populate to send the set command
 @param read read value from 5990
 @param expected expected value

 @return true if the read value matches the expected value
         false if the read value does not match the expected value
 */
bool AanderaaSensor::compareValuesPopulateBuffer(
    const char *parameter, AanderaaSensor::AanderaaConductivityString *buf,
    const AanderaaSensor::AanderaaConductivityFloat read,
    const AanderaaSensor::AanderaaConductivityFloat expected) {

  constexpr float epsilon = 0.0001f;
  if (fabs(read - expected) < epsilon) {
    return true;
  }

  snprintf(*buf, sizeof(AanderaaConductivityString), "%s %s(%f)\r\n", CMD_SET, parameter,
           expected);
  return false;
}

/*!
 @brief Compares String Values And Populates Set Buffer To Send Values

 @details If the values for read and expected do not match, the buffer used to
          set the parameter is populated.

 @param parameter parameter to set if values do not match
 @param buf buffer to populate to send the set command
 @param read read value from 5990
 @param expected expected value

 @return true if the read value matches the expected value
         false if the read value does not match the expected value
 */
bool AanderaaSensor::compareValuesPopulateBuffer(
    const char *parameter, AanderaaSensor::AanderaaConductivityString *buf,
    const AanderaaSensor::AanderaaConductivityString read,
    const AanderaaSensor::AanderaaConductivityString expected) {

  if (!strncmp(read, expected, sizeof(AanderaaConductivityString))) {
    return true;
  }

  snprintf(*buf, sizeof(AanderaaConductivityString), "%s %s(%s)\r\n", CMD_SET, parameter,
           expected);
  return false;
}

/*!
 @brief Overloaded Wrapper To Accept String Literals For Expected Value

 @param parameter parameter to get/set
 @param expected_val expected value from get command
 @param retries number of times to retry getting/setting the parameter

 @return BmOk on success,
         BmEINVAL if expected value is longer than AanderaaConductivityString
 */
BmErr AanderaaSensor::readValidateWriteValue(const char *parameter, const char *expected_val,
                                             uint8_t retries) {
  AanderaaConductivityString str_buf = {};

  if (strlen(expected_val) > sizeof(str_buf)) {
    return BmEINVAL;
  }

  strncpy(str_buf, expected_val, sizeof(str_buf));

  return readValidateWriteValue<AanderaaConductivityString>(parameter, str_buf, retries);
}
