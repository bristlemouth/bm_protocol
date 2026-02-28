#include "composite_device.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Internal context
// ---------------------------------------------------------------------------

struct CompositeDeviceCtx {
  NetworkDevice adin; ///< ADIN2111 sub-device (ports 1-2).
  NetworkDevice uart; ///< UART sub-device (port 3).
  /// Callbacks filled in by bm_l2_init — the "upward" composite callbacks.
  NetworkDeviceCallbacks composite_callbacks;
};

static CompositeDeviceCtx s_ctx;

// ---------------------------------------------------------------------------
// Wrapper callbacks — ADIN sub-device
//   ADIN port_num 1,2  → composite port_num 1,2  (no remap)
//   ADIN port_idx 0,1  → composite port_idx 0,1  (no remap)
// ---------------------------------------------------------------------------

static void adin_power(bool on) {
  if (s_ctx.composite_callbacks.power) {
    s_ctx.composite_callbacks.power(on);
  }
}

static void adin_link_change(uint8_t port_idx, bool is_up) {
  if (s_ctx.composite_callbacks.link_change) {
    s_ctx.composite_callbacks.link_change(port_idx, is_up); // forward as-is
  }
}

static void adin_receive(uint8_t port_num, uint8_t *data, size_t length) {
  if (s_ctx.composite_callbacks.receive) {
    s_ctx.composite_callbacks.receive(port_num, data, length); // forward as-is
  }
}

// ---------------------------------------------------------------------------
// Wrapper callbacks — UART sub-device
//   UART port_num 1  → composite port_num 3
//   UART port_idx 0  → composite port_idx 2
// ---------------------------------------------------------------------------

static void uart_link_change(uint8_t port_idx, bool is_up) {
  (void)port_idx;
  if (s_ctx.composite_callbacks.link_change) {
    s_ctx.composite_callbacks.link_change(2U, is_up); // remap idx 0 → 2
  }
}

static void uart_receive(uint8_t port_num, uint8_t *data, size_t length) {
  (void)port_num;
  if (s_ctx.composite_callbacks.receive) {
    s_ctx.composite_callbacks.receive(3U, data, length); // remap port 1 → 3
  }
}

// ---------------------------------------------------------------------------
// NetworkDeviceTrait implementations
// ---------------------------------------------------------------------------

static BmErr composite_send(void *self, uint8_t *data, size_t length, uint8_t port) {
  CompositeDeviceCtx *ctx = static_cast<CompositeDeviceCtx *>(self);
  BmErr err = BmOK;

  if (port == 0U) {
    // Flood: send on all ports.
    err = ctx->adin.trait->send(ctx->adin.self, data, length, 0U);
    BmErr uart_err = ctx->uart.trait->send(ctx->uart.self, data, length, 1U);
    if (err == BmOK) {
      err = uart_err;
    }
  } else if (port <= 2U) {
    err = ctx->adin.trait->send(ctx->adin.self, data, length, port);
  } else if (port == 3U) {
    err = ctx->uart.trait->send(ctx->uart.self, data, length, 1U);
  } else {
    err = BmEINVAL;
  }
  return err;
}

static BmErr composite_enable(void *self) {
  CompositeDeviceCtx *ctx = static_cast<CompositeDeviceCtx *>(self);
  BmErr err = ctx->adin.trait->enable(ctx->adin.self);
  if (err == BmOK) {
    err = ctx->uart.trait->enable(ctx->uart.self);
  }
  return err;
}

static BmErr composite_disable(void *self) {
  CompositeDeviceCtx *ctx = static_cast<CompositeDeviceCtx *>(self);
  BmErr err = ctx->adin.trait->disable(ctx->adin.self);
  BmErr uart_err = ctx->uart.trait->disable(ctx->uart.self);
  return (err != BmOK) ? err : uart_err;
}

static BmErr composite_enable_port(void *self, uint8_t port_num) {
  CompositeDeviceCtx *ctx = static_cast<CompositeDeviceCtx *>(self);
  if (port_num <= 2U) {
    return ctx->adin.trait->enable_port(ctx->adin.self, port_num);
  } else if (port_num == 3U) {
    return ctx->uart.trait->enable_port(ctx->uart.self, 1U);
  }
  return BmEINVAL;
}

static BmErr composite_disable_port(void *self, uint8_t port_num) {
  CompositeDeviceCtx *ctx = static_cast<CompositeDeviceCtx *>(self);
  if (port_num <= 2U) {
    return ctx->adin.trait->disable_port(ctx->adin.self, port_num);
  } else if (port_num == 3U) {
    return ctx->uart.trait->disable_port(ctx->uart.self, 1U);
  }
  return BmEINVAL;
}

static BmErr composite_retry_negotiation(void *self, uint8_t port_index, bool *renegotiated) {
  CompositeDeviceCtx *ctx = static_cast<CompositeDeviceCtx *>(self);
  // Only ADIN supports renegotiation; UART has no negotiation.
  if (port_index <= 1U && ctx->adin.trait->retry_negotiation) {
    return ctx->adin.trait->retry_negotiation(ctx->adin.self, port_index, renegotiated);
  }
  if (renegotiated) {
    *renegotiated = false;
  }
  return BmOK;
}

static uint8_t composite_num_ports(void) { return 3U; }

static BmErr composite_port_stats(void *self, uint8_t port_index, void *stats) {
  CompositeDeviceCtx *ctx = static_cast<CompositeDeviceCtx *>(self);
  if (port_index <= 1U && ctx->adin.trait->port_stats) {
    return ctx->adin.trait->port_stats(ctx->adin.self, port_index, stats);
  }
  // UART has no hardware stats.
  return BmEINVAL;
}

static BmErr composite_handle_interrupt(void *self) {
  CompositeDeviceCtx *ctx = static_cast<CompositeDeviceCtx *>(self);
  // ADIN2111 uses SPI interrupts; UART has its own ISR.
  if (ctx->adin.trait->handle_interrupt) {
    return ctx->adin.trait->handle_interrupt(ctx->adin.self);
  }
  return BmOK;
}

// ---------------------------------------------------------------------------
// Public factory function
// ---------------------------------------------------------------------------

NetworkDevice composite_network_device(NetworkDevice adin, NetworkDevice uart) {
  s_ctx.adin = adin;
  s_ctx.uart = uart;
  memset(&s_ctx.composite_callbacks, 0, sizeof(s_ctx.composite_callbacks));

  // Install wrapper callbacks on the ADIN sub-device.
  s_ctx.adin.callbacks->power = adin_power;
  s_ctx.adin.callbacks->link_change = adin_link_change;
  s_ctx.adin.callbacks->receive = adin_receive;

  // Install wrapper callbacks on the UART sub-device (no power callback needed).
  s_ctx.uart.callbacks->link_change = uart_link_change;
  s_ctx.uart.callbacks->receive = uart_receive;

  static NetworkDeviceTrait const trait = {
      .send = composite_send,
      .enable = composite_enable,
      .disable = composite_disable,
      .enable_port = composite_enable_port,
      .disable_port = composite_disable_port,
      .retry_negotiation = composite_retry_negotiation,
      .num_ports = composite_num_ports,
      .port_stats = composite_port_stats,
      .handle_interrupt = composite_handle_interrupt,
  };

  return NetworkDevice{
      .self = &s_ctx,
      .trait = &trait,
      .callbacks = &s_ctx.composite_callbacks,
  };
}
