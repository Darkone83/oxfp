#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>

// Forward decls from your project
#include "OXFP_config.h"
#include "OXFP_orig.h"

namespace OXFP_udp {

  // Start/stop the UDP listener. If enable==true, begins immediately on |port|.
  void begin(uint16_t port = 32123, bool enable = false);

  // Call once per loop(); fully non-blocking.
  void loop();

  // Runtime controls (e.g., from web UI later)
  void setEnabled(bool en);
  void setPort(uint16_t port);

  // Optional: query current state
  bool enabled();
  uint16_t port();

} // namespace OXFP_udp
