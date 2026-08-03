#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

//  Drop-in for the mean only usage of AveragingSampler
class AveragingSamplerUnbuffered {
public:
  AveragingSamplerUnbuffered() { clear(); }

  bool addSample(double sample) {
    double y = sample - _comp;
    double t = _sum + y;
    _comp = (t - _sum) - y;
    _sum = t;
    _numSamples++;
    return true;
  }

  double getMean() const {
    if (_numSamples == 0) {
      return NAN;
    }
    return _sum / static_cast<double>(_numSamples);
  }

  uint32_t getNumSamples() const { return _numSamples; }

  void clear() {
    _sum = 0.0;
    _comp = 0.0;
    _numSamples = 0;
  }

private:
  double _sum;
  double _comp;        // Kahan compensation term
  uint32_t _numSamples;
};
