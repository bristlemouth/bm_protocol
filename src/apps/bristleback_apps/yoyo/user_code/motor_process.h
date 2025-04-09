#ifndef __MOTOR_PROCESS_H__
#define __MOTOR_PROCESS_H__

#include "bsp.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  MOTOR_FORWARD,
  MOTOR_BACKWARD,
  MOTOR_OFF,
} MotorState_t;

#ifdef __cplusplus
extern "C" {
#endif

void motor_init(void);
bool set_pwm_duty(uint8_t duty, IOPinHandle_t *handle);
void set_motor_state(MotorState_t state);

#ifdef __cplusplus
}
#endif

#endif
