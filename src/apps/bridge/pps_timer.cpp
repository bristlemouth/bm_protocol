//
// TIM2 disciplined to the GPS PPS signal on PA0 (TIM2_CH1).
//
// TIM2_CH1 captures each rising PPS edge, and a PI loop
// trims the timer period (ARR) every second so the counter wraps on the PPS
// edge and its frequency matches the PPS. Between edges TIM2->CNT counts the
// timer clock ticks since the start of the second.
//
// - The first PPS edges after boot measure the frequency and step the phase.
// - Losing PPS holds the last frequency estimate (holdover), the period is
//   never left trimmed by a stale phase correction.
// - Regaining PPS slews the phase back at a limited rate, the frequency estimate
//   is rate limited, so the period never jumps.
// - Edges that are not one second after the previous edge, or that land far
//   from the expected time while locked, are rejected as glitches.
//
#include "FreeRTOS.h"
#include "task.h"

#include "lpm.h"
#include "main.h"
#include "pps_timer.h"

static_assert(PPS_TIMER_INPUT_FILTER >= 0 && PPS_TIMER_INPUT_FILTER <= 15,
              "PPS_TIMER_INPUT_FILTER must be 0-15");
static_assert(PPS_TIMER_INPUT_FILTER_CKD >= 0 && PPS_TIMER_INPUT_FILTER_CKD <= 2,
              "PPS_TIMER_INPUT_FILTER_CKD must be 0-2");

// PI loop gains, applied as divisors on the phase error (in ticks)
static constexpr int64_t PPS_TIMER_PI_KP_DIV = 4;
static constexpr int64_t PPS_TIMER_PI_KI_DIV = 64;
// The frequency estimate moves this fraction of the way to each PPS interval measurement
static constexpr int64_t PPS_TIMER_FLL_DIV = 16;
// Largest frequency offset from nominal the timer clock (MSI based) is expected to have.
// PPS intervals further than this from one second are rejected.
static constexpr int64_t PPS_TIMER_MAX_FREQ_PPM = 10000;
// Largest change of the frequency estimate per PPS edge from the interval measurement
static constexpr int64_t PPS_TIMER_MAX_FREQ_STEP_PPM = 1;
// Largest period change from the proportional term. Larger phase errors are slewed
// out at this rate.
static constexpr int64_t PPS_TIMER_MAX_SLEW_PPM = 100;
// Missed PPS edges before the timer is no longer considered locked
static constexpr uint32_t PPS_TIMER_HOLDOVER_PERIODS = 3;
// Consecutive edges outside the lock window that are rejected while locked
static constexpr uint8_t PPS_TIMER_MAX_OUTLIERS = 3;
// ARR is only written directly when the counter is at least this far from it
static constexpr int64_t PPS_TIMER_ARR_MARGIN_US = 1000;
// Fractional bits of the frequency estimate
static constexpr int64_t PPS_TIMER_FREQ_FRAC_BITS = 8;
static constexpr int64_t PPS_TIMER_NS_PER_S = 1000000000LL;

typedef struct {
  // Derived from the timer clock at init
  int64_t nominal_ticks;
  int64_t max_freq_ticks;
  int64_t max_freq_step_q;
  int64_t max_slew_ticks;
  int64_t lock_window_ticks;
  int64_t arr_margin_ticks;
  // Frequency offset estimate in ticks per second, with PPS_TIMER_FREQ_FRAC_BITS
  int64_t freq_q;
  // Phase correction for the period started by the next update
  int64_t pending_trim;
  uint64_t ticks_since_capture;
  uint32_t last_capture;
  bool have_last_capture;
  bool last_capture_accepted;
  uint32_t periods_since_pps;
  uint8_t outliers;
  bool acquired;
  bool locked;
  int64_t phase_err;
  uint32_t pps_count;
  uint32_t rejected_count;
} PpsTimerContext_t;
static PpsTimerContext_t pps_ctx = {};

static int64_t clamp(int64_t val, int64_t limit) {
  if (val > limit) {
    return limit;
  }
  if (val < -limit) {
    return -limit;
  }
  return val;
}

static int64_t abs64(int64_t val) { return (val < 0) ? -val : val; }

static int64_t base_arr(void) {
  return pps_ctx.nominal_ticks - 1 + (pps_ctx.freq_q >> PPS_TIMER_FREQ_FRAC_BITS);
}

// Counter wrapped, start the next period at the current frequency estimate
static void handle_update(void) {
  // ARR preload is disabled, so ARR still holds the length of the period that just ended
  pps_ctx.ticks_since_capture += TIM2->ARR + 1ULL;
  TIM2->ARR = static_cast<uint32_t>(base_arr() + pps_ctx.pending_trim);
  pps_ctx.pending_trim = 0;

  if (pps_ctx.periods_since_pps < UINT32_MAX) {
    pps_ctx.periods_since_pps++;
  }
  if (pps_ctx.periods_since_pps > PPS_TIMER_HOLDOVER_PERIODS) {
    pps_ctx.locked = false;
  }
}

static void handle_capture(uint32_t capture, bool overcapture) {
  int64_t period = static_cast<int64_t>(TIM2->ARR) + 1;
  // Positive error means the PPS edge came after the wrap (timer ahead)
  int64_t err = (capture < period / 2) ? static_cast<int64_t>(capture)
                                       : static_cast<int64_t>(capture) - period;
  int64_t interval = static_cast<int64_t>(pps_ctx.ticks_since_capture) + capture -
                     pps_ctx.last_capture;
  bool have_interval = pps_ctx.have_last_capture && !overcapture;
  bool prev_accepted = pps_ctx.last_capture_accepted;
  pps_ctx.ticks_since_capture = 0;
  pps_ctx.last_capture = capture;
  pps_ctx.have_last_capture = true;
  pps_ctx.last_capture_accepted = false;

  // Measured timer frequency offset over the last PPS interval
  int64_t meas_freq = interval - pps_ctx.nominal_ticks;
  if (!have_interval || abs64(meas_freq) > pps_ctx.max_freq_ticks) {
    pps_ctx.rejected_count++;
    return;
  }
  // While locked, a lone edge far from the expected time is a glitch
  if (pps_ctx.locked && abs64(err) > pps_ctx.lock_window_ticks &&
      ++pps_ctx.outliers <= PPS_TIMER_MAX_OUTLIERS) {
    pps_ctx.rejected_count++;
    return;
  }
  pps_ctx.outliers = 0;
  pps_ctx.last_capture_accepted = true;
  pps_ctx.periods_since_pps = 0;
  pps_ctx.pps_count++;

  int64_t trim;
  if (!pps_ctx.acquired) {
    // Nothing is using the timer yet, so jump straight to the PPS frequency and phase
    pps_ctx.freq_q = meas_freq << PPS_TIMER_FREQ_FRAC_BITS;
    trim = err;
    pps_ctx.acquired = true;
  } else {
    // Frequency locked loop on the PPS interval, only when both of its edges were good
    if (prev_accepted) {
      pps_ctx.freq_q += clamp(((meas_freq << PPS_TIMER_FREQ_FRAC_BITS) - pps_ctx.freq_q) /
                                  PPS_TIMER_FLL_DIV,
                              pps_ctx.max_freq_step_q);
    }
    // Integral term, only for small errors so it doesn't wind up while slewing
    if (abs64(err) <= pps_ctx.lock_window_ticks) {
      pps_ctx.freq_q += (err << PPS_TIMER_FREQ_FRAC_BITS) / PPS_TIMER_PI_KI_DIV;
    }
    pps_ctx.freq_q =
        clamp(pps_ctx.freq_q, pps_ctx.max_freq_ticks << PPS_TIMER_FREQ_FRAC_BITS);
    // Proportional term
    trim = clamp(err / PPS_TIMER_PI_KP_DIV, pps_ctx.max_slew_ticks);
  }
  pps_ctx.locked = abs64(err) <= pps_ctx.lock_window_ticks;
  pps_ctx.phase_err = err;

  // The trim goes on the period that ends at the next PPS edge. For an edge after
  // the wrap that is the current period. For an edge before the wrap it is the
  // period the wrap starts, which handle_update() sets up.
  int64_t arr = base_arr() + trim;
  if (err >= 0 && static_cast<int64_t>(TIM2->CNT) + pps_ctx.arr_margin_ticks < arr) {
    TIM2->ARR = static_cast<uint32_t>(arr);
  } else {
    pps_ctx.pending_trim = trim;
  }
}

extern "C" void TIM2_IRQHandler(void) {
  uint32_t sr = TIM2->SR;
  bool update = sr & TIM_SR_UIF;
  bool capture = sr & TIM_SR_CC1IF;
  uint32_t capture_val = 0;
  if (capture) {
    capture_val = TIM2->CCR1;
  }
  TIM2->SR = ~(sr & (TIM_SR_UIF | TIM_SR_CC1IF | TIM_SR_CC1OF));

  // When both are pending, a capture late in the period happened before the wrap
  if (update && capture && capture_val >= (TIM2->ARR + 1ULL) / 2) {
    handle_capture(capture_val, sr & TIM_SR_CC1OF);
    handle_update();
  } else {
    if (update) {
      handle_update();
    }
    if (capture) {
      handle_capture(capture_val, sr & TIM_SR_CC1OF);
    }
  }
}

void ppsTimerInit(void) {
  // TIM2 kernel clock is PCLK1, doubled when APB1 is divided
  int64_t tim_clk = HAL_RCC_GetPCLK1Freq();
  if (RCC->CFGR2 & RCC_CFGR2_PPRE1_2) {
    tim_clk *= 2;
  }
  pps_ctx.nominal_ticks = tim_clk;
  pps_ctx.max_freq_ticks = tim_clk * PPS_TIMER_MAX_FREQ_PPM / 1000000;
  pps_ctx.max_freq_step_q =
      (tim_clk * PPS_TIMER_MAX_FREQ_STEP_PPM << PPS_TIMER_FREQ_FRAC_BITS) / 1000000;
  pps_ctx.max_slew_ticks = tim_clk * PPS_TIMER_MAX_SLEW_PPM / 1000000;
  pps_ctx.lock_window_ticks = pps_ctx.max_slew_ticks * PPS_TIMER_PI_KP_DIV;
  pps_ctx.arr_margin_ticks = tim_clk * PPS_TIMER_ARR_MARGIN_US / 1000000;

  // TIM2 does not run in STOP modes
  lpmPeripheralActive(LPM_TIM2);

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_GPIO_InitTypeDef gpio = {};
  gpio.Pin = LL_GPIO_PIN_0;
  gpio.Mode = LL_GPIO_MODE_ALTERNATE;
  gpio.Speed = LL_GPIO_SPEED_FREQ_LOW;
  gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  gpio.Pull = LL_GPIO_PULL_NO;
  gpio.Alternate = LL_GPIO_AF_1; // TIM2_CH1
  LL_GPIO_Init(GPIOA, &gpio);

  __HAL_RCC_TIM2_CLK_ENABLE();
  __HAL_RCC_TIM2_FORCE_RESET();
  __HAL_RCC_TIM2_RELEASE_RESET();

  // ARR preload disabled so period trims apply to the current period
  TIM2->CR1 = PPS_TIMER_INPUT_FILTER_CKD << TIM_CR1_CKD_Pos;
  TIM2->PSC = 0;
  TIM2->ARR = static_cast<uint32_t>(base_arr());
  // CH1 captures TI1 (CC1S = TI1) through the IC1F input filter
  TIM2->CCMR1 = (1UL << TIM_CCMR1_CC1S_Pos) | (PPS_TIMER_INPUT_FILTER << TIM_CCMR1_IC1F_Pos);
  // CC1P = CC1NP = 0, rising edge
  TIM2->CCER = TIM_CCER_CC1E;
  // Load PSC and ARR and clear the counter
  TIM2->EGR = TIM_EGR_UG;
  TIM2->SR = 0;
  TIM2->DIER = TIM_DIER_CC1IE | TIM_DIER_UIE;

  NVIC_SetPriority(TIM2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),
                                                  configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
                                                  0));
  NVIC_EnableIRQ(TIM2_IRQn);

  TIM2->CR1 |= TIM_CR1_CEN;
}

void ppsTimerGetStatus(PpsTimerStatus_t *status) {
  configASSERT(status);
  taskENTER_CRITICAL();
  PpsTimerContext_t ctx = pps_ctx;
  taskEXIT_CRITICAL();

  *status = {};
  if (ctx.nominal_ticks == 0) {
    return;
  }
  status->acquired = ctx.acquired;
  status->locked = ctx.locked;
  status->phase_err_ns = ctx.phase_err * PPS_TIMER_NS_PER_S / ctx.nominal_ticks;
  status->freq_offset_ppb = ctx.freq_q * PPS_TIMER_NS_PER_S /
                            (ctx.nominal_ticks << PPS_TIMER_FREQ_FRAC_BITS);
  status->pps_count = ctx.pps_count;
  status->rejected_count = ctx.rejected_count;
}
