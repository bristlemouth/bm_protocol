#include "differenceSignal.h"

/*!
 * @brief Constructor for DifferenceSignal class
 * @param numTotalSamples[in] Number of total samples of the signal to be stored
 */
DifferenceSignal::DifferenceSignal(uint32_t numTotalSamples) : r_i(0) {
  configASSERT(numTotalSamples > 0);
  r_n = numTotalSamples;
  r = static_cast<double *>(pvPortMalloc((r_n) * sizeof(double)));
  configASSERT(r);
  _mutex = xSemaphoreCreateMutex();
  configASSERT(_mutex);
}

/*!
 * @brief Adds a sample to the signal
 * @param sample[in] The sample to be added
 * @return True if the sample was added, false if the buffer is full
 */
bool DifferenceSignal::addSample(double sample) {
  bool rval = false;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  do {
    if (r_i < r_n) {
      r[r_i++] = sample;
      rval = true;
    }
  } while (0);
  xSemaphoreGive(_mutex);
  return rval;
}

/*!
 * @brief Encodes the difference signal to a buffer
 * @param d_n[out] The buffer to store the difference signal
 * @param numSamples[in, out] In: The buffer sample size, Out: The number of samples encoded into the buffer
 * @return True if the difference signal was encoded, false otherwise
 * @note Caller must ensure that the buffer is large enough to store the requested samples
 */
bool DifferenceSignal::encodeDifferenceSignalToBuffer(double *d_n, size_t &numSamples) {
  configASSERT(d_n);
  configASSERT(numSamples > 0);
  bool rval = false;
  xSemaphoreTake(_mutex, portMAX_DELAY);
  do {
    if (!r_i) {
      break;
    }
    size_t dn_len = r_i - 1;
    numSamples = (numSamples < dn_len) ? numSamples : dn_len;
    for (uint32_t i = 1; i < numSamples + 1; i++) {
      d_n[i - 1] = r[i] - r[i - 1];
    }
    rval = true;
  } while (0);
  xSemaphoreGive(_mutex);

  if (!rval) {
    numSamples = 0;
  }
  return rval;
}

/*!
 * @brief Clears the signal buffer
 */
void DifferenceSignal::clear() {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  r_i = 0;
  xSemaphoreGive(_mutex);
}

/*!
 * @brief Checks if the signal buffer is full
 * @return True if the buffer is full, false otherwise
 */
bool DifferenceSignal::isFull() { return r_i == r_n; }

/*!
 * @brief Calculates the mean of the signal
 * @return The mean of the signal
 */
double DifferenceSignal::signalMean() {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  double sum = 0.0;
  double result = 0.0;
  for (uint32_t i = 0; i < r_i; i++) {
    sum += r[i];
  }
  if (r_i) {
    result = sum / r_i;
  }
  xSemaphoreGive(_mutex);
  return result;
}

/*!
 * @brief Gets the reference signal
 * @param r0[out] The reference signal
 * @return True if the reference signal was retrieved, false otherwise
 */
bool DifferenceSignal::getReferenceSignal(double &r0) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  bool rval = false;
  do {
    if (!r_i) {
      break;
    }
    r0 = r[0];
    rval = true;
  } while (0);
  xSemaphoreGive(_mutex);
  return rval;
}

/*!
 * @brief Creates a difference signal from a buffer
 * @param d_n[in, out] The buffer containing the signal
 * @param numSamples[in, out] In: The num samples in the buffer, Out: The number of samples in the buffer
 * @param r0[out] The reference signal
 * @return True if the difference signal was created, false otherwise
 */
bool DifferenceSignal::differenceSignalFromBuffer(double *d_n, size_t &numSamples, double &r0) {
  configASSERT(d_n);
  configASSERT(numSamples > 0);
  bool rval = false;
  do {
    if (!numSamples) {
      break;
    }
    r0 = d_n[0];
    for (size_t i = 1; i < numSamples; i++) {
      d_n[i - 1] = d_n[i] - d_n[i - 1];
    }
    numSamples--;
    rval = true;
  } while (0);
  return rval;
}
