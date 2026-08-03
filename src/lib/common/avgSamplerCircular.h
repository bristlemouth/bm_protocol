#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

//Bufferless circular statistics via running sin/cos sums.
class AveragingSamplerCircular {
public:
  AveragingSamplerCircular() { clear(); }

  bool addSample(double sample_rad) {
    _sinSum += sin(sample_rad);
    _cosSum += cos(sample_rad);
    _numSamples++;
    return true;
  }

  double getCircularMean() const {
    if (_numSamples == 0) { return NAN; }
    double mean = atan2(_sinSum, _cosSum);
    if (mean < 0.0) { mean += 2 * M_PI; }
    return mean;
  }

  double getCircularStd() const {
    if (_numSamples == 0) { return NAN; }
    double a1 = _cosSum / static_cast<double>(_numSamples);
    double b1 = _sinSum / static_cast<double>(_numSamples);
    return sqrt(2.0 - 2.0 * sqrt(a1 * a1 + b1 * b1));
  }

  uint32_t getNumSamples() const { return _numSamples; }

  void clear() {
    _sinSum = 0.0;
    _cosSum = 0.0;
    _numSamples = 0;
  }

private:
  double _sinSum;
  double _cosSum;
  uint32_t _numSamples;
};
