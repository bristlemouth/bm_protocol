#include "motor_process.h"
#include "bm_config.h"
#include "bm_os.h"
#include "bsp.h"
#include "powerSampler.h"
#include "sensorSampler.h"
#include "stm32u5xx_hal.h"
#include "stm32u5xx_hal_tim.h"
#include "tim.h"

#define STARTING_SPEED_PERCENT (30)
#define MAX_SPEED_PERCENT (100)
#define RAMP_TIME_MS (5000)
#define SPEED_PER_ITERATION (1)

#define NUMBER_OF_ITERATIONS                                                                   \
  (((float)MAX_SPEED_PERCENT - (float)STARTING_SPEED_PERCENT) / (float)SPEED_PER_ITERATION)
#define DELAY_TIME_MS ((uint32_t)((float)RAMP_TIME_MS / (float)NUMBER_OF_ITERATIONS))
#define MOTOR_SEMAPHORE_MAX_DELAY_MS (1000)

#define POWER_MONITOR_TASK_UPDATE_TIME_MS (100)

#define CALCULATE_DUTY(tim, duty) (duty != 0 ? (((tim->ARR + 1) * duty) / 100) : 0)

static struct {
  MotorState_t state;
  BmSemaphore sem;
  BmTimer timer;
} motor_ctx = {};

static void motor_control_task(void *arg) {
  (void)arg;
  uint8_t speed = STARTING_SPEED_PERCENT;
  IOPinHandle_t *handle_on = NULL;
  IOPinHandle_t *handle_off = NULL;
  MotorState_t prev_state = MOTOR_OFF;

  while (1) {
    bm_semaphore_take(motor_ctx.sem, MOTOR_SEMAPHORE_MAX_DELAY_MS);
    if (prev_state != motor_ctx.state) {
      speed = STARTING_SPEED_PERCENT;
      prev_state = motor_ctx.state;
    }

    switch (motor_ctx.state) {
    case MOTOR_FORWARD:
      handle_on = &MOTOR_SPEED2;
      handle_off = &MOTOR_SPEED1;
      break;
    case MOTOR_BACKWARD:
      handle_on = &MOTOR_SPEED1;
      handle_off = &MOTOR_SPEED2;
      break;
    case MOTOR_OFF:
    default:
      handle_on = NULL;
      handle_off = NULL;
      break;
    }

    if (motor_ctx.state != MOTOR_OFF && speed <= 100) {
      set_pwm_duty(speed, handle_on);
      set_pwm_duty(0, handle_off);
      speed += SPEED_PER_ITERATION;
    } else if (motor_ctx.state == MOTOR_OFF) {
      speed = STARTING_SPEED_PERCENT;
      set_pwm_duty(0, &MOTOR_SPEED1);
      set_pwm_duty(0, &MOTOR_SPEED2);
    }
    bm_semaphore_give(motor_ctx.sem);
    bm_delay(DELAY_TIME_MS);
  }
}

static void power_monitor_task(void *args) {
  (void)args;
  float voltage;
  float current;

  while (1) {

    if (powerSamplerGetLatest(I2C_INA_MAIN_ADDR, voltage, current)) {
      bm_debug("Power Draw At Address 0x%X: %.3fV %.3fmA\n", (unsigned int)I2C_INA_MAIN_ADDR,
               voltage, current);
    }
    if (powerSamplerGetLatest(I2C_INA_PODL_ADDR, voltage, current)) {
      bm_debug("Power Draw At Address 0x%X: %.3fV %.3fmA\n", (unsigned int)I2C_INA_MAIN_ADDR,
               voltage, current);
    }

    bm_delay(POWER_MONITOR_TASK_UPDATE_TIME_MS);
  }
}

void motor_init(void) {
  // Sample current monitor at a higher rate
  sensorSamplerChangeSamplingPeriodMs(POWER_SAMPLER_NAME, 100);

  motor_ctx.sem = bm_mutex_create();
  bm_task_create(motor_control_task, "motor control task", 512, NULL, 10, NULL);
  bm_task_create(power_monitor_task, "power monitor task", 512, NULL, 12, NULL);
}

bool set_pwm_duty(uint8_t duty, IOPinHandle_t *handle) {
  bool ret = false;
  TIM_HandleTypeDef *timer = NULL;
  uint32_t channel = 0;
  volatile uint32_t *pwm_reg = NULL;
  uint32_t value = 0;

  if (handle == &MOTOR_SPEED1) {
    timer = &htim3;
    channel = TIM_CHANNEL_3;
    pwm_reg = &TIM3->CCR3;
    value = CALCULATE_DUTY(TIM3, duty);
  } else if (handle == &MOTOR_SPEED2) {
    timer = &htim5;
    channel = TIM_CHANNEL_4;
    pwm_reg = &TIM5->CCR4;
    value = CALCULATE_DUTY(TIM2, duty);
  }

  if (timer && pwm_reg) {
    bm_debug("Setting duty cycle to %" PRIx8 " (%" PRIx32 ") at handle %p\n", duty, value,
             handle);
    *pwm_reg = value;
    HAL_TIM_PWM_Start(timer, channel);
    ret = true;
  }

  return ret;
}

void set_motor_state(MotorState_t state) {
  bm_semaphore_take(motor_ctx.sem, MOTOR_SEMAPHORE_MAX_DELAY_MS);
  motor_ctx.state = state;
  bm_semaphore_give(motor_ctx.sem);
}

MotorState_t get_motor_state(void) {
  MotorState_t ret = MOTOR_OFF;

  bm_semaphore_take(motor_ctx.sem, MOTOR_SEMAPHORE_MAX_DELAY_MS);
  ret = motor_ctx.state;
  bm_semaphore_give(motor_ctx.sem);

  return ret;
}
