#pragma once
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdint.h>
#include <stdlib.h>

class DifferenceSignal {
public:
  DifferenceSignal(uint32_t numTotalSamples);
  bool addSample(double sample);
  bool encodeDifferenceSignalToBuffer(double *d_n, size_t &numSamples);
  bool getReferenceSignal(double &r0);
  void clear();
  bool isFull();
  double signalMean();
  static bool differenceSignalFromBuffer(double *d_n, size_t &numSamples, double &r0);

private:
  SemaphoreHandle_t _mutex;
  uint32_t r_i;
  uint32_t r_n;
  double *r;
};