#pragma once
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

private:
  uint32_t r_i;
  uint32_t r_n;
  double *r;
};