#include "differenceSignal.h"
#include "FreeRTOS.h"

/*!
 * @brief Constructor for DifferenceSignal class
 * @param numTotalSamples[in] Number of total samples of the signal to be stored
 */
DifferenceSignal::DifferenceSignal(uint32_t numTotalSamples) : r_i(0) {
  configASSERT(numTotalSamples > 0);
  r_n = numTotalSamples;
  r = static_cast<double *>(pvPortMalloc((r_n) * sizeof(double)));
  configASSERT(r);
}

/*!
 * @brief Adds a sample to the signal
 * @param sample[in] The sample to be added
 * @return True if the sample was added, false if the buffer is full
 */
bool DifferenceSignal::addSample(double sample) {
  if (r_i < r_n) {
    r[r_i++] = sample;
    return true;
  }
  return false;
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
  do {
    if (!r_i) {
      break;
    }
    size_t dn_len = r_i - 1;
    numSamples = (numSamples < dn_len) ? numSamples : dn_len;
    for (uint32_t i = 1; i < numSamples+1; i++) {
      d_n[i-1] = r[i] - r[i - 1];
    }
    rval = true;
  } while (0);
  if (!rval) {
    numSamples = 0;
  }
  return rval;
}

/*!
 * @brief Clears the signal buffer
 */
void DifferenceSignal::clear() { r_i = 0; }

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
    if(!r_i) {
        return 0.0;
    }
    double sum = 0.0;
    for (uint32_t i = 0; i < r_i; i++) {
        sum += r[i];
    }
    return sum / r_i;
}

/*!
 * @brief Gets the reference signal
 * @param r0[out] The reference signal
 * @return True if the reference signal was retrieved, false otherwise
 */
bool DifferenceSignal::getReferenceSignal(double &r0) {
    if (!r_i) {
        return false;
    }
    r0 = r[0];
    return true;
}
