#include "user_code.h"
#include "spotter.h"
#include "pubsub.h"
#include "bsp.h"
#include "configuration.h"
#include "debug.h"
#include "lwip/inet.h"
#include "sensors.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "app_util.h"
#include "bno085.h"
#include "pa7ld.h"

extern "C" {
#include "sh2.h"
#include "sh2_util.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"
}


double gravity = 9.80665; // m/s^2
double rho_salt = 1023.6; // kg/m^3
double rho_fresh =  997.0474; // kg/m^3

#define PRESSURE_IN_PASCALS(pressure) ((pressure * 100000))
static constexpr char FRESH_WATER_FLAG[]  = "fresh_water";
static constexpr uint8_t DEPTH_SENSOR_ADDR = 0x40;

static uint32_t fresh_water = 0;


static Bno085 bno085_imu(&i2c1, IMU_ADDR);
// C-style wrapper functions for the HAL driver
static int bno085_open_wrapper(sh2_Hal_t* hal) {
    return bno085_imu.open(hal);
}

static void bno085_close_wrapper(sh2_Hal_t* hal) {
    bno085_imu.close(hal);
}

static int bno085_read_wrapper(sh2_Hal_t* hal, uint8_t* pBuffer, unsigned len, uint32_t* t_us) {
    return bno085_imu.read(hal, pBuffer, len, t_us);
}

static int bno085_write_wrapper(sh2_Hal_t* hal, uint8_t* pBuffer, unsigned len) {
    return bno085_imu.write(hal, pBuffer, len);
}

static uint32_t bno085_getTimeUs_wrapper(sh2_Hal_t* hal) {
    return bno085_imu.getTimeUs(hal);
}

sh2_Hal_t sh2_hal_driver = {
    .open = bno085_open_wrapper,
    .close = bno085_close_wrapper,
    .read = bno085_read_wrapper,
    .write = bno085_write_wrapper,
    .getTimeUs = bno085_getTimeUs_wrapper
};

void event_callback(void *cookie, sh2_AsyncEvent_t *pEvent) {
  (void) cookie;
  (void) pEvent;

  // TODO - handle the event
  // This is where we would handle the events from the sensor hub
  // For now we will just print the event ID
  printf("Received event with ID: %" PRIu32 "\n", pEvent->eventId);
  if (pEvent->eventId == SH2_RESET) {
    printf("EventHandler id: RESET\n");
  } else if (pEvent->eventId == SH2_SHTP_EVENT) {
    printf("EventHandler id: SHTP, %d\n", pEvent->shtpEvent);
  } else if (pEvent->eventId == SH2_GET_FEATURE_RESP) {
    printf("EventHandler id: Sensor config, %d\n", pEvent->sh2SensorConfigResp.sensorId);
  }
}

// __attribute((unused))
static void printEvent(void * cookie, sh2_SensorEvent_t * event)
{
    (void)cookie;
    int rc;
    sh2_SensorValue_t value;
    float scaleRadToDeg = 180.0 / 3.14159265358;
    float r, i, j, k, acc_deg, x, y, z;
    float t;
    static int skip = 0;

    rc = sh2_decodeSensorEvent(&value, event);
    if (rc != SH2_OK) {
        printf("Error decoding sensor event: %d\n", rc);
        return;
    }

    t = value.timestamp / 1000000.0;  // time in seconds.
    switch (value.sensorId) {
        case SH2_RAW_ACCELEROMETER:
            printf("%8.4f Raw acc: %" PRIi16 " %" PRIi16 " %" PRIi16 " time_us:%" PRIu32 "\n",
                   (double)t,
                   value.un.rawAccelerometer.x,
                   value.un.rawAccelerometer.y,
                   value.un.rawAccelerometer.z,
                   value.un.rawAccelerometer.timestamp);
            break;

        case SH2_ACCELEROMETER:
            printf("%8.4f Acc: %f %f %f\n",
                   (double)t,
                   (double)value.un.accelerometer.x,
                   (double)value.un.accelerometer.y,
                   (double)value.un.accelerometer.z);
            break;

        case SH2_RAW_GYROSCOPE:
            printf("%8.4f Raw gyro: x:%d y:%d z:%d temp:%d time_us:%" PRIu32 "\n",
                   (double)t,
                   value.un.rawGyroscope.x,
                   value.un.rawGyroscope.y,
                   value.un.rawGyroscope.z,
                   value.un.rawGyroscope.temperature,
                   value.un.rawGyroscope.timestamp);
            break;

        case SH2_ROTATION_VECTOR:
            r = value.un.rotationVector.real;
            i = value.un.rotationVector.i;
            j = value.un.rotationVector.j;
            k = value.un.rotationVector.k;
            acc_deg = scaleRadToDeg *
                value.un.rotationVector.accuracy;
            printf("%8.4f Rotation Vector: "
                   "r:%0.6f i:%0.6f j:%0.6f k:%0.6f (acc: %0.6f deg)\n",
                   (double)t,
                   (double)r, (double)i, (double)j, (double)k, (double)acc_deg);
            break;
        case SH2_GAME_ROTATION_VECTOR:
            r = value.un.gameRotationVector.real;
            i = value.un.gameRotationVector.i;
            j = value.un.gameRotationVector.j;
            k = value.un.gameRotationVector.k;
            printf("%8.4f GRV: "
                   "r:%0.6f i:%0.6f j:%0.6f k:%0.6f\n",
                   (double)t,
                   (double)r, (double)i, (double)j, (double)k);
            break;
        case SH2_GYROSCOPE_CALIBRATED:
            x = value.un.gyroscope.x;
            y = value.un.gyroscope.y;
            z = value.un.gyroscope.z;
            printf("%8.4f GYRO: "
                   "x:%0.6f y:%0.6f z:%0.6f\n",
                   (double)t,
                   (double)x, (double)y, (double)z);
            break;
        case SH2_GYROSCOPE_UNCALIBRATED:
            x = value.un.gyroscopeUncal.x;
            y = value.un.gyroscopeUncal.y;
            z = value.un.gyroscopeUncal.z;
            printf("%8.4f GYRO_UNCAL: "
                   "x:%0.6f y:%0.6f z:%0.6f\n",
                   (double)t,
                   (double)x, (double)y, (double)z);
            break;
        case SH2_GYRO_INTEGRATED_RV:
            // These come at 1kHz, too fast to print all of them.
            // So only print every 10th one
            skip++;
            if (skip == 10) {
                skip = 0;
                r = value.un.gyroIntegratedRV.real;
                i = value.un.gyroIntegratedRV.i;
                j = value.un.gyroIntegratedRV.j;
                k = value.un.gyroIntegratedRV.k;
                x = value.un.gyroIntegratedRV.angVelX;
                y = value.un.gyroIntegratedRV.angVelY;
                z = value.un.gyroIntegratedRV.angVelZ;
                printf("%8.4f Gyro Integrated RV: "
                       "r:%0.6f i:%0.6f j:%0.6f k:%0.6f x:%0.6f y:%0.6f z:%0.6f\n",
                       (double)t,
                       (double)r, (double)i, (double)j, (double)k,
                       (double)x, (double)y, (double)z);
            }
            break;
        case SH2_IZRO_MOTION_REQUEST:
            printf("IZRO Request: intent:%d, request:%d\n",
                   value.un.izroRequest.intent,
                   value.un.izroRequest.request);
            break;
        case SH2_SHAKE_DETECTOR:
            printf("Shake Axis: %c%c%c\n",
                   (value.un.shakeDetector.shake & SHAKE_X) ? 'X' : '.',
                   (value.un.shakeDetector.shake & SHAKE_Y) ? 'Y' : '.',
                   (value.un.shakeDetector.shake & SHAKE_Z) ? 'Z' : '.');

            break;
        case SH2_STABILITY_CLASSIFIER:
            printf("Stability Classification: %d\n",
                   value.un.stabilityClassifier.classification);
            break;
        case SH2_STABILITY_DETECTOR:
            printf("Stability Detector: %d\n",
                   value.un.stabilityDetector.stability);
            break;
        default:
            printf("Unknown sensor: %d\n", value.sensorId);
            break;
    }
}

static PA7LD depth_sensor(&i2c1, DEPTH_SENSOR_ADDR);

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  vTaskDelay(10000);
  // depth_sensor.init();

  get_config_uint(BM_CFG_PARTITION_SYSTEM, FRESH_WATER_FLAG, strlen(FRESH_WATER_FLAG), &fresh_water);

  int res = sh2_open(&sh2_hal_driver, event_callback, NULL);
  if (res != 0) {
    printf("Failed to open sensor hub, res: %d\n", res);
  } else {
    printf("Sensor hub opened successfully\n");
  }

  // Enable sensor callback
    sh2_setSensorCallback(printEvent, NULL);

    // Configure sensors to report at specific intervals
    static sh2_SensorConfig_t config;

    // Enable rotation vector at 100Hz (10ms interval)
    config.changeSensitivityEnabled = false;
    config.wakeupEnabled = false;
    config.changeSensitivityRelative = false;
    config.alwaysOnEnabled = false;
    config.changeSensitivity = 0;
    config.reportInterval_us = 100000; // 100ms = 10Hz
    config.batchInterval_us = 0;

    res = sh2_setSensorConfig(SH2_ROTATION_VECTOR, &config);
    if (res != SH2_OK) {
        printf("Failed to configure rotation vector: %d\n", res);
    }

    // Enable accelerometer at 100Hz
    res = sh2_setSensorConfig(SH2_ACCELEROMETER, &config);
    if (res != SH2_OK) {
        printf("Failed to configure accelerometer: %d\n", res);
    }

    // Enable gyroscope at 100Hz
    res = sh2_setSensorConfig(SH2_GYROSCOPE_CALIBRATED, &config);
    if (res != SH2_OK) {
        printf("Failed to configure gyroscope: %d\n", res);
    }

}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  uint32_t curr_time_ms = bno085_imu.getTimeUs(&sh2_hal_driver)/1000;
  static uint32_t prev_time_ms = 0;
  if (curr_time_ms - 5000 > prev_time_ms) {
    printf("time: %" PRIu32 " ms\n", curr_time_ms);
    prev_time_ms = curr_time_ms;
    // float pressure, temp;
    // depth_sensor.readPTRaw(pressure, temp);
    // double depth = 0.0;
    // if (fresh_water) {
    //   depth = PRESSURE_IN_PASCALS(pressure) / (gravity * rho_fresh);
    // } else {
    //   depth = PRESSURE_IN_PASCALS(pressure) / (gravity * rho_salt);
    // }

    // printf("pressure: %f bar, temp: %f °C, depth: %fm\n", pressure, temp, depth);

    // TODO - publis the depth on the BM bus so we can use it to make decisions in other systems?
    // Or we subscribe to other systems and tell them what to do to make sure we are correct here?
    sh2_service();

  }
}
