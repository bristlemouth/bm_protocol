#include "uart_network_device.h"

#include "FreeRTOS.h"
#include "frame_codec.h"
#include "payload_uart.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Internal context
// ---------------------------------------------------------------------------

/// FreeRTOS stack depth for the frame RX task (words = 4 B each on ARM32).
/// Must accommodate:
///   wire accumulation buffer (~1536 B) + l2 output buffer (1522 B) +
///   frame_decode() stack-local decoded buffer (1528 B) + call overhead.
static constexpr uint32_t UART_RX_TASK_STACK_DEPTH = 2048U;

/// Default UART baud rate for the SBC link (8N1, no flow control).
static constexpr uint32_t UART_SBC_BAUD = 115200U;

struct UartDeviceCtx {
  NetworkDeviceCallbacks callbacks; ///< Filled in by bm_l2_init after creation.
  TaskHandle_t rx_task_handle;
  uint8_t rx_task_priority;
};

static UartDeviceCtx s_ctx;

// ---------------------------------------------------------------------------
// RX task — COBS byte accumulation → frame_decode → callbacks->receive
// ---------------------------------------------------------------------------

static void uart_rx_task(void *arg) {
  UartDeviceCtx *ctx = static_cast<UartDeviceCtx *>(arg);

  // Wire-format accumulation buffer (COBS bytes, no delimiter).
  static uint8_t wire_buf[FRAME_CODEC_MAX_WIRE_SIZE];
  size_t wire_len = 0;

  // Decoded L2 output buffer.
  static uint8_t l2_buf[FRAME_CODEC_MAX_L2_SIZE];

  while (true) {
    if (!PLUART::byteAvailable()) {
      // Yield to other tasks; 1 ms granularity is fine for serial data rates.
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    uint8_t b = PLUART::readByte();

    if (b == 0x00) {
      // Frame delimiter — attempt to decode whatever we've accumulated.
      if (wire_len > 0) {
        size_t l2_len = frame_decode(l2_buf, sizeof(l2_buf), wire_buf, wire_len);
        if (l2_len > 0) {
          if (ctx->callbacks.receive) {
            ctx->callbacks.receive(1U, l2_buf, l2_len);
          }
        } else {
          printf("uart_rx: frame_decode failed (wire_len=%zu)\n", wire_len);
        }
      }
      wire_len = 0;
    } else {
      // Accumulate into the wire buffer.
      if (wire_len < sizeof(wire_buf)) {
        wire_buf[wire_len++] = b;
      } else {
        // Buffer overflow — discard and wait for next delimiter.
        printf("uart_rx: wire buffer overflow, discarding frame\n");
        wire_len = 0;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// NetworkDeviceTrait implementations
// ---------------------------------------------------------------------------

static BmErr uart_send(void *self, uint8_t *data, size_t length, uint8_t port) {
  (void)self;
  (void)port; // Single port — ignore port selector.

  static uint8_t wire_buf[FRAME_CODEC_MAX_WIRE_SIZE];
  size_t wire_len = frame_encode(wire_buf, sizeof(wire_buf), data, length);
  if (wire_len == 0) {
    return BmEINVAL;
  }
  PLUART::write(wire_buf, wire_len);
  return BmOK;
}

static BmErr uart_enable(void *self) {
  UartDeviceCtx *ctx = static_cast<UartDeviceCtx *>(self);

  PLUART::enable();

  // Report link-up on port index 0 (our single port).
  if (ctx->callbacks.link_change) {
    ctx->callbacks.link_change(0U, true);
  }

  // Start the frame RX decoder task.
  BaseType_t rc = xTaskCreate(uart_rx_task, "uart_rx", UART_RX_TASK_STACK_DEPTH, ctx,
                              ctx->rx_task_priority, &ctx->rx_task_handle);
  if (rc != pdPASS) {
    printf("uart_enable: failed to create uart_rx task\n");
    return BmENOMEM;
  }

  return BmOK;
}

static BmErr uart_disable(void *self) {
  UartDeviceCtx *ctx = static_cast<UartDeviceCtx *>(self);

  PLUART::disable();

  if (ctx->rx_task_handle) {
    vTaskDelete(ctx->rx_task_handle);
    ctx->rx_task_handle = nullptr;
  }

  if (ctx->callbacks.link_change) {
    ctx->callbacks.link_change(0U, false);
  }

  return BmOK;
}

static BmErr uart_enable_port(void *self, uint8_t port_num) {
  (void)self;
  (void)port_num;
  return BmOK; // Always enabled.
}

static BmErr uart_disable_port(void *self, uint8_t port_num) {
  (void)self;
  (void)port_num;
  return BmOK; // No-op.
}

static uint8_t uart_num_ports(void) { return 1U; }

static BmErr uart_handle_interrupt(void *self) {
  (void)self;
  return BmOK; // UART uses its own ISR — nothing to do here.
}

// ---------------------------------------------------------------------------
// Public factory function
// ---------------------------------------------------------------------------

NetworkDevice uart_network_device(uint8_t task_priority) {
  memset(&s_ctx, 0, sizeof(s_ctx));
  s_ctx.rx_task_priority = task_priority;

  // Configure and init the PLUART driver.
  PLUART::init(task_priority);
  PLUART::setBaud(UART_SBC_BAUD);
  PLUART::setUseLineBuffer(false);
  PLUART::setUseByteStreamBuffer(true);

  static NetworkDeviceTrait const trait = {
      .send = uart_send,
      .enable = uart_enable,
      .disable = uart_disable,
      .enable_port = uart_enable_port,
      .disable_port = uart_disable_port,
      .retry_negotiation = nullptr,
      .num_ports = uart_num_ports,
      .port_stats = nullptr,
      .handle_interrupt = uart_handle_interrupt,
  };

  return NetworkDevice{
      .self = &s_ctx,
      .trait = &trait,
      .callbacks = &s_ctx.callbacks,
  };
}
