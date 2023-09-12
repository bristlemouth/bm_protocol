#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "FreeRTOS.h"
#include <math.h>


class AveragingSampler {
public:
  AveragingSampler();
  ~AveragingSampler();
  bool addSample(double sample);
  bool addSampleTimestamped(double sample, uint32_t timestamp=0);
  double getMean(bool useKahan = false);
  double getStd(double mean = 0.0, double variance = 0.0, bool useKahan = false);
  double getVariance(double mean = 0.0, bool useKahan = false);
  double kahanSum(double total, double input, double &c);
  double getMax();
  double getMin();
  void initBuffer(uint32_t maxSamples);
  void clear();
  uint32_t getNumSamples();
  uint32_t getMaxSamples();

private:
  uint32_t _lastSampleTime;
  uint32_t _maxSamples;
  uint32_t _sampleIdx;
  bool _fullBuffer;
  double *_samples;
};
