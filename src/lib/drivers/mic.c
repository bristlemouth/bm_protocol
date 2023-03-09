#include <math.h>
#include "debug.h"
#include "FreeRTOS.h"
#include "lpm.h"
#include "mic.h"
#include "sai.h"
#include "task.h"

/// Flags in case of error code during interrupt
static volatile int32_t sai_flags = 0;

/// Signifies which part of the buffer we're using (upper or lower)
static volatile int32_t sai_buff = 0;

/// Number of samples to buffer in memory for each DMA half
#define I2S_NUM_SAMPLES (1024)

/// Total space to use for DMA buffer (assumes 32-bit samples)
#define I2S_BUFFER_SIZE (I2S_NUM_SAMPLES * 2 * sizeof(uint32_t))

static SAI_HandleTypeDef *sai;
static TaskHandle_t currentTask;

/// Time to wait after applying power to microphone
#define MIC_STARTUP_TIME_MS 50

/// Time to wait after starting a DMA read.(Value determined by trial and error)
#define MIC_DMA_START_DELAY_MS (100)

#define TEST_SET_SIZE 32

static IOPinHandle_t *_powerPin;

/*!
  Enable microphone and check if it is present

  \param[in] saiHandle SAI(I2S) peripheral handle
  \return true if mic is present and working, false otherwise
*/
bool micInit(SAI_HandleTypeDef *saiHandle, IOPinHandle_t *powerPin) {
  configASSERT(saiHandle != NULL);
  bool rval = false;
  sai = saiHandle;
  _powerPin = powerPin;

  int32_t *samples = (int32_t *)pvPortMalloc(TEST_SET_SIZE * sizeof(int32_t));
  configASSERT(samples != NULL);

  // Make sure processor doesn't go into STOP mode
  lpmPeripheralActive(LPM_I2S);

  if(_powerPin != NULL) {
    // Enable microphone power
    IOWrite(_powerPin, 1);
  }

  MX_SAI1_Init();
  __HAL_SAI_ENABLE(sai);

  // Wait for microphone to power up
  vTaskDelay(pdMS_TO_TICKS(MIC_STARTUP_TIME_MS));

  if(HAL_SAI_Receive(sai, (uint8_t *)samples, TEST_SET_SIZE, 100) != HAL_OK) {
    printf("Error reading from SAI");
  }

  for(uint32_t sample = 0; sample < TEST_SET_SIZE; sample++) {
    // Look for non zero (but not maxed) samples
    if(samples[sample] != 0 && samples[sample] != 0xFFFFFF) {
      rval = true;
      break;
    }
  }

  if(rval != true) {
    printf("No microphone detected\n");
  }

  __HAL_SAI_DISABLE(sai);
  HAL_SAI_DeInit(sai);

  // Make sure processor can go back into STOP mode
  lpmPeripheralInactive(LPM_I2S);

  if(_powerPin != NULL) {
    // Disable microphone power
    IOWrite(_powerPin, 0);
  }

  vPortFree(samples);

  return rval;
}

/*!
  Sample microphone for <durationSec> seconds.

  \param[in] durationSec SAI(I2S) peripheral handle
  \param[in] callback Callback function to call with new samples
  \param[in] arg Pointer to any arguments to pass to callback function
  \return false in case of errors, true otherwise
*/
bool micSample(uint32_t durationSec, micCallbackFn callback, void *args) {
  bool rval = true;
  configASSERT(callback != NULL);

  uint32_t samplesRemaining = durationSec * sai->Init.AudioFrequency;

  // Make sure processor doesn't go into STOP mode
  lpmPeripheralActive(LPM_I2S);

  if(_powerPin != NULL) {
    // Enable microphone power
    IOWrite(_powerPin, 1);
  }

  MX_SAI1_Init();
  __HAL_SAI_ENABLE(sai);

  currentTask = xTaskGetCurrentTaskHandle();

  int32_t *samples = (int32_t *)pvPortMalloc(I2S_BUFFER_SIZE);
  configASSERT(samples);

  // Wait for microphone to power up
  vTaskDelay(pdMS_TO_TICKS(MIC_STARTUP_TIME_MS));

  printf("Sampling for %lu seconds %p\n", durationSec, samples);

  // Start sampling I2S over DMA
  HAL_SAI_Receive_DMA(sai, (uint8_t *)samples, I2S_NUM_SAMPLES * 2);

  // Clear flags and buffer index
  sai_flags = 0;
  sai_buff = 0;

  // Wait a bit while ignoring the data
  // Usually the first bit of the microphone output after the clock starts
  // is noisy but settles down soon after.
  vTaskDelay(pdMS_TO_TICKS(MIC_DMA_START_DELAY_MS));

  // Clear any pending task notifications before starting
  ulTaskNotifyTakeIndexed(MIC_I2S_NOTIFY_INDEX, pdTRUE, 0);

  while(samplesRemaining) {

    // Wait for DMA interrupt
    ulTaskNotifyTakeIndexed(MIC_I2S_NOTIFY_INDEX, pdTRUE, portMAX_DELAY);

    if (sai_flags) {
      printf("SAI error %ld\n", sai_flags);
      rval = false;
      samplesRemaining = 0;
    } else {
      // How many samples to process in this round
      uint32_t sampleCount = I2S_NUM_SAMPLES;
      if(samplesRemaining <= I2S_NUM_SAMPLES) {
        sampleCount = samplesRemaining;
      }

      // process samples in callback
      callback((const uint32_t *)&samples[I2S_NUM_SAMPLES * sai_buff], sampleCount, args);

      samplesRemaining -= sampleCount;
    }
  }

  // Stop sampling
  HAL_SAI_DMAStop(sai);

  __HAL_SAI_DISABLE(sai);
  HAL_SAI_DeInit(sai);

  if(_powerPin != NULL) {
    // Disable microphone power
    IOWrite(_powerPin, 0);
  }

  // Make sure processor can go back into STOP mode
  lpmPeripheralInactive(LPM_I2S);

  // Free sample buffer
  vPortFree(samples);

  printf("Done Sampling.\n");

  return rval;
}

// Acoustic Overload Point for ICS-43434
// See https://invensense.tdk.com/wp-content/uploads/2016/02/DS-000069-ICS-43434-v1.2.pdf
#define MIC_AOP_DB (120)

// TODO - add calibration?
/*!
  Compute dB level from sample buffer

  \param[in] samples Sample buffer
  \param[in] numSamples Number of samples in buffer
  \return sound level in dB
*/
float micGetDB(const uint32_t *samples, uint32_t numSamples) {
  float accumulator = 0;

  // Compute RMS value
  for (uint32_t index=0; index < numSamples; index++) {
    // Need to left shift then back right to extend the sign bit of
    // the 24-bit sample
    float sample = ((int32_t)samples[index] << 8) >> 8;

    accumulator += sample * sample;
  }

  accumulator = sqrtf(accumulator/numSamples);

  // Compute DB value
  float db = MIC_AOP_DB + 20.0 * log10f(accumulator/(1<<24));

  return db;
}

//
// SAI IRQ handlers
//
void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  if(hsai == sai) {
    sai_buff = 1;
    vTaskNotifyGiveIndexedFromISR(currentTask, MIC_I2S_NOTIFY_INDEX, &higherPriorityTaskWoken);
  }
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  if(hsai == sai) {
    sai_buff = 0;
    vTaskNotifyGiveIndexedFromISR(currentTask, MIC_I2S_NOTIFY_INDEX, &higherPriorityTaskWoken);
  }

  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
  BaseType_t higherPriorityTaskWoken = pdFALSE;

  if(hsai == sai) {
    sai_flags = hsai->ErrorCode;
    vTaskNotifyGiveIndexedFromISR(currentTask, MIC_I2S_NOTIFY_INDEX, &higherPriorityTaskWoken);
  }

  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}
