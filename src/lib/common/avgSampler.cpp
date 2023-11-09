#include "avgSampler.h"

AveragingSampler::AveragingSampler() {
  _lastSampleTime = 0;
  _sampleIdx = 0;
  _fullBuffer = false;
  _maxSamples = 0;
  _samples = NULL;
  _runningMean = 0.0;
  _runningVarianceSum = 0.0;
  _kahanC = 0.0;
  _runningCosSum = 0.0;
  _kahanCosC = 0.0;
  _runningSinSum = 0.0;
  _kahanSinC = 0.0;
  _sampleCount = 0;

}

AveragingSampler::~AveragingSampler() {
  if (_samples != NULL) {
    vPortFree(_samples);
  }
}

/*!
  Allocate the circular buffer. MUST be called before using the sampler class!

  \param[in] maxSample maximum number of samples to store
*/
void AveragingSampler::initBuffer(uint32_t maxSamples) {
  configASSERT(maxSamples > 0);
  _maxSamples = maxSamples;
  if (_samples != NULL) {
    vPortFree(_samples);
  }
  _samples = NULL;
  _samples = static_cast<double *>(pvPortMalloc(sizeof(double) * maxSamples));
  configASSERT(_samples != NULL);
}

/*!
  Add sample to circular buffer

  \param[in] sample Sample to add to buffer
  \param[in] timestamp (optional) timestamp of the sample
  \return true if sucessful, false otherwise (repeated timestamp)
*/
bool AveragingSampler::addSample(double sample) {
  return addSampleTimestamped(sample, 0);
}

/*!
  Add sample to circular buffer. Use timestamp 0 to skip check

  \param[in] sample Sample to add to buffer
  \param[in] timestamp (optional) timestamp of the sample
  \return true if sucessful, false otherwise (repeated timestamp)
*/
bool AveragingSampler::addSampleTimestamped(double sample, uint32_t timestamp) {
  bool rval = false;
  configASSERT(_samples != NULL);
  // Ignore timestamp if it is zero
  if (!timestamp || (timestamp != _lastSampleTime)) {
    _samples[_sampleIdx] = sample;
    _lastSampleTime = timestamp;

    // Wrap around
    if (++_sampleIdx == _maxSamples) {
      _fullBuffer = true;
      _sampleIdx = 0;
    }

    if(_sampleCount++ == 0) {
      _runningMean = sample;
      _runningCosSum = cos(sample);
      _runningSinSum = sin(sample);
      _runningVarianceSum = 0.0;
    } else {
      double oldMean = _runningMean;
      double oldVariance = _runningVarianceSum;
      double averagedNewData = (sample - oldMean) / _sampleCount;
      _runningMean = kahanSum(oldMean, averagedNewData, _kahanC);
      _runningCosSum = kahanSum(_runningCosSum, cos(sample), _kahanCosC);
      _runningSinSum = kahanSum(_runningSinSum, sin(sample), _kahanSinC);
      _runningVarianceSum = oldVariance + (sample - oldMean) * (sample - _runningMean);
    }
    rval = true;
  }

  return rval;
}

/*!
  Get number of samples in circular buffer

  \return Number of samples in circular buffer
*/
uint32_t AveragingSampler::getNumSamples() {
  if (!_fullBuffer) {
    return _sampleIdx;
  } else {
    return _maxSamples;
  }
}

/*!
  Get maximum number of samples to store

  \return _maxSamples
*/
uint32_t AveragingSampler::getMaxSamples() {
  return _maxSamples;
}

/*!
  Clear all samples and reset sample index
*/
void AveragingSampler::clear() {
  configASSERT(_samples != NULL);
  memset(_samples, 0, sizeof(double) * _maxSamples);
  _sampleIdx = 0;
  _lastSampleTime = 0;
  _fullBuffer = false;
}

/*!
  Compute mean of all the samples

  \return mean of all samples
*/
double AveragingSampler::getMean(bool useKahan) {
  configASSERT(_samples != NULL);
  double total = 0.0;
  double mean = 0.0;

  uint32_t numSamples = getNumSamples();

  if (useKahan) {
    double c = 0.0;
    for (uint32_t i = 0; i < numSamples; i++) {
      total = kahanSum(total, _samples[i], c);
    }
  } else {
    for (uint32_t i = 0; i < numSamples; i++) {
      total += _samples[i];
    }
  }

  // Don't divide by zero!
  if (numSamples) {
    mean = total / numSamples;
  } else {
    mean = NAN;
  }

  return mean;
}

/*!
  Compute standard deviation of all the samples

  \param[in] mean - (Optional) Computed mean of all samples
  \param[in] variance - (Optional) Computed variance of all samples
  \param[in] useKahan - (Optional) Use Kahan summation algorithm for mean calculation
  \return standard deviation of all the samples
*/
double AveragingSampler::getStd(double mean, double variance, bool useKahan) {
  if (variance != 0.0) {
    return sqrt(variance);
  }

  double avg = mean != 0.0 ? mean : getMean(useKahan);
  return sqrt(getVariance(avg, useKahan));
}

/*!
  Get the maximum value in the circular buffer

  \return maximum value in the circular buffer
*/
double AveragingSampler::getMax() {
  uint32_t numSamples = getNumSamples();
  if (!numSamples) {
    return NAN;
  }
  double max = _samples[0];
  for (uint32_t i = 0; i < numSamples; i++) {
    if (_samples[i] > max) {
      max = _samples[i];
    }
  }
  return max;
}

/*!
  Get the minimum value in the circular buffer

  \return minimum value in the circular buffer
*/
double AveragingSampler::getMin() {
  uint32_t numSamples = getNumSamples();
  if (!numSamples) {
    return NAN;
  }
  double min = _samples[0];
  for (uint32_t i = 0; i < numSamples; i++) {
    if (_samples[i] < min) {
      min = _samples[i];
    }
  }
  return min;
}

/*!
  Compute the running kahan sum (i.e. needs to be called in a loop, see getMean() for an example)

  \param[in] total - running total of all samples (accumulator)
  \param[in] input - sample to add to total
  \param[in/out] c - running compensation
  \return true if successful, false otherwise (no samples available)
*/
double AveragingSampler::kahanSum(double total, double input, double &c) {
  volatile double sum = total;
  double y = input - c;
  volatile double t = sum + y;
  c = (t - sum) - y;
  return t;
}

/*!
  Compute variance of all the samples

  \param[in] mean - (Optional) Computed mean of all samples
  \param[in] useKahan - (Optional) Use Kahan summation algorithm for mean calculation
  \return variance of all the samples
*/
double AveragingSampler::getVariance(double mean, bool useKahan) {

  double avg = mean != 0.0 ? mean : getMean(useKahan);
  double var = 0.0;
  uint32_t numSamples = getNumSamples();

  for (uint16_t i = 0; i < numSamples; i++) {
    var = var + ((_samples[i] - avg) * (_samples[i] - avg));
  }
  // Don't divide by zero!
  if (numSamples) {
    var = var / numSamples;
  } else {
    var = NAN;
  }
  return var;
}

/*!
  Compute the running circular mean of all the samples
  https://en.wikipedia.org/wiki/Circular_mean

  \return circular mean of all the samples
*/
double AveragingSampler::getCircularMean() {
  return atan2(_runningSinSum, _runningCosSum);
}

/*!
  Compute the running circular standard deviation

  \return circular standard deviation
 */
double AveragingSampler::getCircularStd(void) {
  double a1 = _runningCosSum / _sampleCount;
  double b1 = _runningSinSum / _sampleCount;
  if(a1 == NAN || b1 == NAN){
    return NAN;
  }

  return sqrt(2 - 2 * sqrt(pow(a1, 2) + pow(b1,2)));
}

/*!
 * @brief Get the running mean (Kahan summation)
 * @return The running mean
 */
double AveragingSampler::getRunningKahanMean(void) {
  return _runningMean;
}

/*!
 * @brief Get the running variance (Welford's algorithm)
 * @return The running variance
 */
double AveragingSampler::getRunningWelfordVariance(void) {
  return (_sampleCount > 1) ? (_runningVarianceSum / (_sampleCount - 1)) : 0;
}
