#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/*
  Welford's algorithm: keeps a running mean and M2 (sum of squared
  deviations), updated one sample at a time so there's no stored buffer.
  Per sample:
      delta = x - mean
      mean += delta / n
      M2   += delta * (x - mean)   // old delta * new deviation
  variance = M2 / n (population). Also tracks running min/max. O(1) memory.
*/
class AveragingSamplerStats {
public:
  AveragingSamplerStats() { clear(); }

  bool addSample(double sample) {
    _numSamples++;
    double delta = sample - _mean;
    _mean += delta / static_cast<double>(_numSamples);
    _m2 += delta * (sample - _mean);
    if (sample < _min) { _min = sample; }
    if (sample > _max) { _max = sample; }
    return true;
  }

  double getMean() const { return _numSamples ? _mean : NAN; }

  double getVariance() const {
    return _numSamples ? (_m2 / static_cast<double>(_numSamples)) : NAN;
  }

  double getStd() const { return _numSamples ? sqrt(getVariance()) : NAN; }

  double getMin() const { return _numSamples ? _min : NAN; }
  double getMax() const { return _numSamples ? _max : NAN; }

  uint32_t getNumSamples() const { return _numSamples; }

  void clear() {
    _numSamples = 0;
    _mean = 0.0;
    _m2 = 0.0;
    _min = HUGE_VAL;
    _max = -HUGE_VAL;
  }

private:
  uint32_t _numSamples;
  double _mean;
  double _m2;
  double _min;
  double _max;
};
