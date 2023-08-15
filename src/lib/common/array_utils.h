//
// Created by Genton Mo on 11/28/18.
//

#ifndef ARRAY_UTILS_H
#define ARRAY_UTILS_H
#include "FreeRTOS.h"
#ifdef CI_TEST
#include <math.h>
#endif // CI_TEST

// TODO - add Welford's alg for running stable variance
// TODO - port in unit tests from Spotter
// Kahan summing, volatiles necessary to prevent compiler optimizations
// refer to: https://en.wikipedia.org/wiki/Kahan_summation_algorithm

template <class T>
double kahanSum( double total, T input, double& c ){
  volatile double sum = total;
  double y = ((double) input) - c;
  volatile double t = sum + y;
  c = (t - sum) - y;
  return t;
}

template <class T>
double getMean(T * buffer, uint16_t bufLen, bool useKahan = false){

  double total = 0.0;
  if (useKahan) {
    double c = 0.0;
    for (uint16_t i = 0; i < bufLen; i++) {
      total = kahanSum(total, (double) buffer[i], c);
    }
  }
  else {
    for( uint16_t i = 0; i < bufLen; i++ ){
      total += ((double) buffer[i]);
    }
  }
  return total / bufLen;
}

template <class T>
double getVariance(T * buffer, uint16_t bufLen, double mean = 0.0, bool useKahan = false){

  double avg = mean ? mean : getMean(buffer, bufLen, useKahan);
  double var = 0.0;

  for( uint16_t i = 0; i < bufLen; i++ ){
    var = var + ( (buffer[i] - avg) * (buffer[i] - avg) );
  }
  var = var / bufLen;
  return var;
}

template <class T>
double getStd(T * buffer, uint16_t bufLen, double mean = 0.0, double variance = 0.0, bool useKahan = false){

  if( variance ) return sqrt(variance);

  double avg = mean ? mean : getMean(buffer, bufLen, useKahan);
  return sqrt(getVariance(buffer, bufLen, avg));
}

template <class T>
T getMax(T * buffer, uint16_t bufLen, int &index) {
  configASSERT(buffer);
  configASSERT(bufLen > 0);
  T max = buffer[0];
  index = 0;
  for(int i  = 0; i < bufLen; i++) {
    if (buffer[i] > max){
      max = buffer[i];
      index = i;
    }
  }
  return max;
}

#endif //ARRAY_UTILS_H
