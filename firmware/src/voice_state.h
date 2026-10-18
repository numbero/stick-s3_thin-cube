#pragma once
#include <stdint.h>

// Hardware-independent policy. Only the main loop owns this object.
struct VoiceState {
  bool allowed = false, muted = false, localSound = true;
  bool inverted = false, sleeping = false, busy = false, peer = false;
  bool streaming = false, manual = false, held = false, armed = false;
  bool pendingGesture = false;
  bool pendingManual = false, manualButtonDown = false;
  uint32_t manualRequestedAt = 0;
  bool captureReady = false;
  uint32_t leaseAt = 0;

  bool voiceEnabled() const { return allowed && !muted && !sleeping && !busy && peer; }
  bool canStream() const { return voiceEnabled() && captureReady; }
  bool poseExitGuarded() const { return inverted && (streaming || pendingGesture); }
  bool captureWanted(bool preparing=false,bool localVisible=false) const {
    return !muted && !sleeping && !busy &&
      ((localSound&&localVisible) || (allowed && peer && (streaming || pendingGesture || pendingManual || preparing)));
  }
  void stop() { streaming = manual = held = armed = pendingGesture = pendingManual = false; }
  void setPeer(bool ready, uint32_t now) {
    peer = ready; leaseAt = now;
    if (!ready) stop();
  }
  void tick(uint32_t now) {
    if (peer && uint32_t(now - leaseAt) > 1000) { peer = false; stop(); }
    if(pendingManual){
      if(!voiceEnabled()||uint32_t(now-manualRequestedAt)>=5000)stop();
      else if(canStream()){pendingManual=false;streaming=manual=true;armed=!manualButtonDown;}
    }
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
    else if (!next && !manual && !pendingManual) stop();
    // Even a manual meeting releases shortcuts on pose exit.
    if (!next) { held = false; armed = false; }
  }
  bool startManual(bool buttonDown) {
    if (!canStream()) return false;
    pendingGesture = false; streaming = manual = true; armed = !buttonDown; return true;
  }
  bool requestManual(bool buttonDown,uint32_t now){
    if(!voiceEnabled())return false;
    if(startManual(buttonDown))return true;
    pendingGesture=false;pendingManual=true;manualButtonDown=buttonDown;manualRequestedAt=now;return true;
  }
  void button(bool down) {
    if (!down) { held = false; if (streaming) armed = true; }
    else if (streaming && inverted && armed && canStream()) held = true;
  }
};
