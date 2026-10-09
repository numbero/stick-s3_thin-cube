#pragma once
#include <stdint.h>

// Hardware-independent policy. Only the main loop owns this object.
struct VoiceState {
  bool allowed = false, muted = false, localSound = true;
  bool inverted = false, sleeping = false, busy = false, peer = false;
  bool streaming = false, manual = false, held = false, armed = false;
  bool pendingGesture = false;
  bool captureReady = false;
  uint32_t leaseAt = 0;

  bool voiceEnabled() const { return allowed && !muted && !sleeping && !busy && peer; }
  bool canStream() const { return voiceEnabled() && captureReady; }
  bool captureWanted() const { return !muted && !sleeping && !busy && (localSound || streaming || (allowed && peer)); }
  void stop() { streaming = manual = held = armed = pendingGesture = false; }
  void setPeer(bool ready, uint32_t now) {
    peer = ready; leaseAt = now;
    if (!ready) stop();
  }
  void tick(uint32_t now) {
    if (peer && uint32_t(now - leaseAt) > 1000) { peer = false; stop(); }
    if (pendingGesture && (!inverted || !voiceEnabled())) stop();
    if (pendingGesture && canStream()) { pendingGesture = false; streaming = true; manual = false; }
    if (streaming && !canStream()) stop();
  }
  void setPose(bool next, bool buttonDown) {
    if (next == inverted) return;
    inverted = next;
    if (next && voiceEnabled()) {
      if (!streaming) { pendingGesture = !captureReady; streaming = captureReady; manual = false; }
      armed = !buttonDown;
    }
    else if (!next && !manual) stop();
    // Even a manual meeting releases shortcuts on pose exit.
    if (!next) { held = false; armed = false; }
  }
  bool startManual(bool buttonDown) {
    if (!canStream()) return false;
    pendingGesture = false; streaming = manual = true; armed = !buttonDown; return true;
  }
  void button(bool down) {
    if (!down) { held = false; if (streaming) armed = true; }
    else if (streaming && inverted && armed && canStream()) held = true;
  }
};
