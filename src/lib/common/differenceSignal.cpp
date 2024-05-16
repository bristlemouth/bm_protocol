#include "differenceSignal.h"
#include "FreeRTOS.h"

DifferenceSignal::DifferenceSignal(uint32_t numTotalSamples) : r_i(0) {
  configASSERT(numTotalSamples > 0);
  r_n = numTotalSamples;
  r = static_cast<double *>(pvPortMalloc((r_n) * sizeof(double)));
  configASSERT(r);
}

bool DifferenceSignal::addSample(double sample) {
  if (r_i < r_n) {
    r[r_i++] = sample;
    return true;
  }
  return false;
}

bool DifferenceSignal::encodeDifferenceSignalToBuffer(double *d_n, size_t &numSamples) {
  configASSERT(d_n);
  configASSERT(numSamples > 0);
  bool rval = false;
  do {
    if (numSamples < (r_i)) {
      break;
    }
    if (!r_i) {
      break;
    }
    d_n[0] = 0.0;
    for (uint32_t i = 1; i < r_i; i++) {
      d_n[i] = r[i] - r[i - 1];
    }
    numSamples = r_i;
    rval = true;
  } while (0);
  if (!rval) {
    numSamples = 0;
  }
  return rval;
}

void DifferenceSignal::clear() { r_i = 0; }

bool DifferenceSignal::isFull() { return r_i == r_n; }
