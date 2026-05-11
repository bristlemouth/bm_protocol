#pragma once

/// @file composite_device.h
/// @brief Composite NetworkDevice that multiplexes ADIN2111 (ports 1-2) and
///        UART (port 3) into a single 3-port Bristlemouth network device.
///
/// Port mapping:
///   Global port 1 → ADIN port 1
///   Global port 2 → ADIN port 2
///   Global port 3 → UART port 1
///
/// Usage:
///   NetworkDevice adin = adin2111_network_device();
///   NetworkDevice uart = uart_network_device(task_priority);
///   NetworkDevice dev  = composite_network_device(adin, uart);
///   bm_l2_init(dev);   // bm_l2_init fills in dev.callbacks

#include "network_device.h"

/// Create a 3-port composite NetworkDevice from an ADIN and a UART device.
///
/// Installs wrapper callbacks on @p adin and @p uart that remap port numbers
/// to the composite namespace before forwarding to the composite's own
/// callbacks (which bm_l2_init later fills in).
///
/// @param adin  A NetworkDevice returned by adin2111_network_device().
/// @param uart  A NetworkDevice returned by uart_network_device().
/// @return A 3-port NetworkDevice suitable for passing to bm_l2_init.
NetworkDevice composite_network_device(NetworkDevice adin, NetworkDevice uart);
