#pragma once
#include <stdint.h>

// Neutral IMU types shared by all IMU drivers (LSM6DSV, BNO085).
// Values are in each driver's native units; the compile-time-selected
// driver fixes the units for a given build:
//   LSM6DSV: acc = mg,    gyro = mdps
//   BNO085:  acc = m/s^2,  gyro = rad/s
typedef struct {
  uint64_t ns;
  struct {
    float x, y, z;
  } acc;
  struct {
    float x, y, z;
  } gyro;
  struct {
    float x, y, z;
  } mag;
} IMUReading;

// Compass native units -- LIS2MDL (via LSM sensor hub): mgauss;  BNO085: uT
typedef struct {
  uint64_t ns;
  float x, y, z;
} CompassReading;
