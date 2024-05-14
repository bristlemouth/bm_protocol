#pragma once
#include <stdint.h>
#include <stdlib.h>

class DifferenceSignal {
public:
  DifferenceSignal(uint32_t numTotalSamples);
  bool addSample(double sample);
  bool encodeDifferenceSignalToBuffer(double *d_n, size_t &numSamples);
  void clear();

private:
  uint32_t r_i;
  uint32_t r_n;
  double *r;
};