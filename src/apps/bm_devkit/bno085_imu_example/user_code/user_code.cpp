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

extern "C" {
#include "sh2.h"
#include "sh2_util.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"
}

static Bno085 bno085_imu(&i2c1, IMU_ADDR);

// ISR callback for INT pin
bool imuIntCallback(const void *pinHandle, uint8_t value, void *args) {
    (void)pinHandle;
    (void)args;


    // INT is active low, so trigger when it goes low (value == 0)
    if (value == 0) {
        bno085_imu.notifyDataReady();
    }
    return true;
}

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
        case SH2_MAGNETIC_FIELD_CALIBRATED:
            printf("%8.4f Mag Field calibrated: x:%f y:%f z:%f \n",
                  (double)t,
                  value.un.magneticField.x,
                  value.un.magneticField.y,
                  value.un.magneticField.z);
            break;

        case SH2_MAGNETIC_FIELD_UNCALIBRATED:
            printf("%8.4f Mag Field uncalibrated: x:%f y:%f z:%f \n",
                  (double)t,
                  value.un.magneticField.x,
                  value.un.magneticField.y,
                  value.un.magneticField.z);
            break;


        case SH2_RAW_ACCELEROMETER:
            printf("%8.4f Raw acc: x:%" PRIi16 " y:%" PRIi16 " z:%" PRIi16 " time_us:%" PRIu32 "\n",
                   (double)t,
                   value.un.rawAccelerometer.x,
                   value.un.rawAccelerometer.y,
                   value.un.rawAccelerometer.z,
                   value.un.rawAccelerometer.timestamp);
            break;


        case SH2_ACCELEROMETER:
            printf("%8.4f Acc: x:%f y:%f z:%f\n",
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

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  vTaskDelay(10000);

  IORegisterCallback(&BF_IMU_INT, imuIntCallback, NULL);

  // // Enable sensor callback
  // Initialize BNO085 - this creates the service task
    if (!bno085_imu.init(event_callback, printEvent)) {
        printf("Failed to initialize BNO085\n");
        return;
    }

    vTaskDelay(1000);

    // Configure sensors (10 Hz = 100ms interval)
    bno085_imu.configureSensor(SH2_ROTATION_VECTOR, 1000000);
    bno085_imu.configureSensor(SH2_ACCELEROMETER, 1000000);
    bno085_imu.configureSensor(SH2_GYROSCOPE_CALIBRATED, 1000000);
    bno085_imu.configureSensor(SH2_MAGNETIC_FIELD_CALIBRATED, 1000000);

    printf("BNO085 configured and running\n");
}


void loop(void) {
  /* USER LOOP CODE GOES HERE */
  uint32_t curr_time_ms = uptimeGetMs();
  static uint32_t prev_time_ms = 0;
  if (curr_time_ms - 5000 > prev_time_ms) {
    printf("time: %" PRIu32 " ms\n", curr_time_ms);
    prev_time_ms = curr_time_ms;
  }
}
