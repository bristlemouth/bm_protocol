#include "debug.h"
#include "util.h"
#include "user_code.h"
#include "bsp.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "lwip/inet.h"
#include "bm_network.h"

#define LED_ON (0)
#define LED_OFF (1)
#define LED_ON_TIME_MS 75
#define LED_PERIOD_MS 1000

static int32_t ledLinePulse = -1;

static void processLineBufferedRxByte(void *serialHandle, uint8_t byte);
static void printLine(void *serialHandle, uint8_t *line, size_t len);

#define LPUART1_LINE_BUFF_LEN 2048
static uint8_t lpUart1Buffer[LPUART1_LINE_BUFF_LEN];
// This is a "line buffer" that is intended to store all UART data from a single "line"
SerialLineBuffer_t lpUART1LineBuffer = {
    .buffer = lpUart1Buffer, // pointer to the buffer memory
    .idx = 0, // variable to store current index in the buffer
    .len = sizeof(lpUart1Buffer), // total size of buffer
    .lineCallback = printLine // lineCallback callback function to call when a line is complete (glued together in the UART handle's byte callback).
};
// This"lpuart1" is a handle to our UART channel
SerialHandle_t lpuart1 = {
    .device = LPUART1,
    .name = "payload",
    .txPin = &PAYLOAD_TX,
    .rxPin = &PAYLOAD_RX,
    .txStreamBuffer = NULL,
    .rxStreamBuffer = NULL,
    .txBufferSize = 128,
    .rxBufferSize = 2048,
    .rxBytesFromISR = serialGenericRxBytesFromISR,
    .getTxBytesFromISR = serialGenericGetTxBytesFromISR,
    .processByte = processLineBufferedRxByte, // This is where we tell it the callback to call when we get a new byte
    .data = &lpUART1LineBuffer, // Pointer to the line buffer this handle should use
    .enabled = false,
    .flags = 0,
};


void setLpuartBaud(uint32_t new_baud_rate) {
  LL_LPUART_SetBaudRate(static_cast<USART_TypeDef *>(lpuart1.device),
                      LL_RCC_GetUSARTClockFreq(LL_RCC_USART3_CLKSOURCE),
                      LL_LPUART_PRESCALER_DIV64,
                      new_baud_rate);
}

/// Called everytime we get a UART byte
static void processLineBufferedRxByte(void *serialHandle, uint8_t byte) {
  configASSERT(serialHandle != NULL);
  SerialHandle_t *handle = reinterpret_cast<SerialHandle_t *>(serialHandle);
//  printf("%c", byte);
  // // This function requires data to be a pointer to a SerialLineBuffer_t
   configASSERT(handle->data != NULL);
   SerialLineBuffer_t *lineBuffer = reinterpret_cast<SerialLineBuffer_t *>(handle->data);
  // // We need a buffer to use!
   configASSERT(lineBuffer->buffer != NULL);
   lineBuffer->buffer[lineBuffer->idx] = byte;
   if(lineBuffer->buffer[lineBuffer->idx] == '\n'){
  //   // Zero terminate the line
     if ((lineBuffer->idx + 1) < lineBuffer->len) {
       lineBuffer->buffer[lineBuffer->idx + 1] = 0;
     }
     if(lineBuffer->lineCallback != NULL) {
       lineBuffer->lineCallback(handle, lineBuffer->buffer, lineBuffer->idx);
     }
  //   // Reset buffer index
     lineBuffer->idx = 0;
   }
   else {
     lineBuffer->idx++;
  //   // Heavy handed way of dealing with overflow for now
  //   // Later we can just purge the buffer
  //   // TODO - log error and clear buffer instead
     configASSERT((lineBuffer->idx + 1) < lineBuffer->len);
   }
}

/// Called everytime we get a 'line' by processLineBufferedRxByte
static void printLine(void *serialHandle, uint8_t *line, size_t len) {
  configASSERT(serialHandle != NULL);
  SerialHandle_t *handle = reinterpret_cast<SerialHandle_t *>(serialHandle);

  configASSERT(line != NULL);

// Set timer variable to now() to trigger LED pulse turn on
  ledLinePulse = uptimeGetMs();

  RTCTimeAndDate_t timeAndDate;
  char rtcTimeBuffer[32];
  if (rtcGet(&timeAndDate) == pdPASS) {
    sprintf(rtcTimeBuffer, "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
            timeAndDate.year,
            timeAndDate.month,
            timeAndDate.day,
            timeAndDate.hour,
            timeAndDate.minute,
            timeAndDate.second,
            timeAndDate.ms);
  } else {
    strcpy(rtcTimeBuffer, "0");
  }

  bm_fprintf(0, "rbr_data.log", "tick: %llu, rtc: %s, line: %.*s\n", uptimeGetMs(), rtcTimeBuffer, len, line);
  bm_printf(0, "[%s] rbr data | tick: %llu, rtc: %s, line: %.*s", handle->name, uptimeGetMs(), rtcTimeBuffer, len, line);
  printf("[%s] rbr data | tick: %llu, rtc: %s, line: %.*s\n", handle->name, uptimeGetMs(), rtcTimeBuffer, len, line);
}

void setup(void) {
  /* USER CODE GOES HERE */
  MX_LPUART1_UART_Init();
  // Single byte trigger level for fast response time
  lpuart1.txStreamBuffer = xStreamBufferCreate(lpuart1.txBufferSize, 1);
  configASSERT(lpuart1.txStreamBuffer != NULL);

  lpuart1.rxStreamBuffer = xStreamBufferCreate(lpuart1.rxBufferSize, 1);
  configASSERT(lpuart1.rxStreamBuffer != NULL);
  BaseType_t rval = xTaskCreate(
              serialGenericRxTask,
              "LPUartRx",
              // TODO - verify stack size
              2048,
              &lpuart1,
              USER_TASK_PRIORITY,
              NULL);
  configASSERT(rval == pdTRUE);

  setLpuartBaud(9600);

  serialEnable(&lpuart1);
  IOWrite(&BF_5V_EN, 1); // 0 enables, 1 disables. Needed for SDI12 and RS485.
  IOWrite(&BF_3V3_EN, 1); // 1 enables, 0 disables. Needed for I2C and I/O control.
  IOWrite(&VBUS_BF_EN, 0); // 0 enables, 1 disables. Needed for VOUT and 5V.
  IOWrite(&BF_PL_BUCK_EN, 0); // 0 enables, 1 disables
  /* USER CODE END */
}

void loop(void) {
  /* USER CODE GOES HERE */
  static uint32_t ledPulseTimer = uptimeGetMs();
  static uint32_t ledOnTimer = 0;
  static bool statusLedState = false;
  static bool rxLedState = false;
  /// TODO - taskify led control
  if (!statusLedState && ((uint32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    IOWrite(&BF_LED_G1, LED_ON);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    statusLedState = true;
  }
  else if (statusLedState && ((uint32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    IOWrite(&BF_LED_G1, LED_OFF);
    statusLedState = false;
  }

  if (!rxLedState && ledLinePulse > -1) {
    IOWrite(&BF_LED_G2, LED_ON);
    rxLedState = true;
  }
  else if (rxLedState && ((uint32_t)uptimeGetMs() - ledLinePulse >= LED_ON_TIME_MS)) {
    IOWrite(&BF_LED_G2, LED_OFF);
    ledLinePulse = -1;
    rxLedState = false;
  }

/// TODO - placeholder for RBR command UART Tx
  //  uint8_t testByte = 0xBE;
//  serialWrite(&lpuart1, &testByte, 1);

  /*
    DO NOT REMOVE vTaskDelay

    This delay is REQUIRED for the FreeRTOS task scheduler
    to allow for lower priority tasks to be serviced

    Keep this delay in the range of 10 100)
  */
  vTaskDelay(pdMS_TO_TICKS(10));

  /* USER CODE END */
}


extern "C" void LPUART1_IRQHandler(void) {
  serialGenericUartIRQHandler(&lpuart1);
}
