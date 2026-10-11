#pragma once
#include "transport_policy.h"

// Re-pairing retains the current mode. Only the single-B switch gesture
// previews its opposite; an accepted transaction already owns its target.
inline TransportMode connectionHintMode(TransportMode current, bool switchReady, bool busy, TransportMode accepted) {
  if(busy)return accepted;
  return switchReady?(current==TransportMode::BLE?TransportMode::WiFi:TransportMode::BLE):current;
}
inline const char* connectionHintLabel(TransportMode mode) { return mode==TransportMode::BLE?"BLUETOOTH":"WI-FI"; }
