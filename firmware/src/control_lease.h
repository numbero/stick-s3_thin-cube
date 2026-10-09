#pragma once
#include <stdint.h>

// The control connection is independent of the Mac's audio-ready flag.
// Owned by the device main loop, just like VoiceState.
struct ControlLease {
  bool received = false;
  uint32_t at = 0;
  void reset() { received = false; }
  void heartbeat(uint32_t now) { received = true; at = now; }
  bool connected(uint32_t now) const { return received && uint32_t(now-at) <= 1000; }
};
