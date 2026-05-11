#pragma once

/// @file uart_network_device.h
/// @brief UART (PLUART) implementation of NetworkDeviceTrait.
///
/// Wraps the firmware's PLUART driver as a single-port Bristlemouth network
/// device. Port 1 maps to the PLUART serial link.
///
/// Frames are COBS+CRC-32C encoded/decoded by frame_codec.h.
///
/// Usage:
///   NetworkDevice uart_dev = uart_network_device(task_priority);
///   // Pass uart_dev to your composite device or bm_l2_init.

#include "network_device.h"
#include <stdint.h>

/// Create a UART (PLUART) NetworkDevice.
///
/// Calls PLUART::init(), sets baud to 115200, and enables byte-stream mode.
/// Does NOT enable the link — the link comes up when the enable() trait is
/// called (typically triggered by bm_l2_init).
///
/// @param task_priority  FreeRTOS priority for PLUART's internal HAL task.
///                       The frame RX decoder task runs at this priority too.
/// @return A NetworkDevice wrapping PLUART as a single-port (port 1) device.
NetworkDevice uart_network_device(uint8_t task_priority);
