#pragma once

#include "FreeRTOS.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

class AveragingSampler {
public:
  typedef enum TrigMeanType {
    TRIG_MEAN_TYPE_SIN,
    TRIG_MEAN_TYPE_COS,
  } TrigMeanType_e;

  AveragingSampler();
  ~AveragingSampler();
  bool addSample(double sample);
  bool addSampleTimestamped(double sample, uint32_t timestamp = 0);
  double getMean(bool useKahan = false);
  double getStd(double mean = 0.0, double variance = 0.0, bool useKahan = false);
  double getVariance(double mean = 0.0, bool useKahan = false);
  double kahanSum(double total, double input, double &c);
  double getMax();
  double getMin();
  double getTrigSum(TrigMeanType_e type);
  double getTrigMean(TrigMeanType_e type);
  double getCircularMean();
  double getCircularStd();
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
