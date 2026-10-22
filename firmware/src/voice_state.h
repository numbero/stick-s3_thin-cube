#pragma once
#include <stdint.h>

// Hardware-independent policy. Only the main loop owns this object.
struct VoiceState {
  bool allowed = false, muted = false, localSound = true;
  bool inverted = false, sleeping = false, busy = false, peer = false;
  bool streaming = false, manual = false, held = false, armed = false;
  bool pendingGesture = false;
  bool poseRearmRequired = false;
  bool pendingManual = false, manualButtonDown = false;
  uint32_t manualRequestedAt = 0;
  bool captureReady = false;
  uint32_t leaseAt = 0;
  uint32_t streamGeneration = 0;

  bool voiceEnabled() const { return allowed && !muted && !sleeping && !busy && peer; }
  bool canStream() const { return voiceEnabled() && captureReady; }
  bool poseExitGuarded() const { return inverted && (streaming || pendingGesture); }
  bool captureWanted(bool preparing=false,bool localVisible=false) const {
    return !muted && !sleeping && !busy &&
      ((localSound&&localVisible) || (allowed && peer && (streaming || pendingGesture || pendingManual || preparing)));
  }
  // Temporary unavailability releases audio/keys without consuming the pose.
  // Explicit cancellation stays latched until a confirmed pose exit or start.
  void pause() { streaming = manual = held = armed = pendingGesture = pendingManual = false; }
  void stop() { pause(); poseRearmRequired = true; }
  void beginStream(bool isManual) {
    if (!streaming) ++streamGeneration;
    streaming = true; manual = isManual;
  }
  void setPeer(bool ready, uint32_t now) {
    peer = ready; leaseAt = now;
    if (!ready) pause();
  }
  void tick(uint32_t now, bool poseHealthy) {
    if (peer && uint32_t(now - leaseAt) > 1000) { peer = false; pause(); }
    if (!allowed || muted) { stop(); return; }
    if(pendingManual){
      if(uint32_t(now-manualRequestedAt)>=5000)stop();
      else if(!voiceEnabled())pause();
      else if(canStream()){pendingManual=false;beginStream(true);armed=!manualButtonDown;}
    }
    if (pendingGesture && (!inverted || !voiceEnabled())) pause();
    // Pending capture can become ready before the pose controller runs, or
    // after audioTick in the same loop. Both paths need fresh motion input.
    if (pendingGesture && canStream() && poseHealthy) { pendingGesture = false; beginStream(false); }
    if (streaming && !canStream()) pause();
  }
  void setPose(bool next, bool buttonDown, bool settled=true) {
    const bool entered = next && !inverted;
    inverted = next;
    if (!next) {
      // A previous stable pose is not a new exit while another candidate is
      // forming, or when motion input has gone stale.
      if (settled) poseRearmRequired = false;
      if (!manual && !pendingManual) pause();
      // Even a manual meeting releases shortcuts on pose exit.
      held = armed = false;
      return;
    }
    if (!allowed || muted) { stop(); return; }
    // Reconcile a held inversion after boot, wake or reconnect. Do not start
    // from a stale stable pose while a different candidate is forming.
    if (!settled || poseRearmRequired || !voiceEnabled()) return;
    if (!streaming && !pendingGesture && !pendingManual) {
      pendingGesture = !captureReady;
      if (captureReady) beginStream(false);
      armed = !buttonDown;
    } else if (entered) {
      armed = !buttonDown;
    }
  }
  bool startManual(bool buttonDown) {
    if (!canStream()) return false;
    poseRearmRequired = false;
    pendingGesture = false; beginStream(true); armed = !buttonDown; return true;
  }
  bool requestManual(bool buttonDown,uint32_t now){
    if(!voiceEnabled())return false;
    if(startManual(buttonDown))return true;
    poseRearmRequired=false;
    pendingGesture=false;pendingManual=true;manualButtonDown=buttonDown;manualRequestedAt=now;return true;
  }
  void button(bool down) {
    if (!down) { held = false; if (streaming) armed = true; }
    else if (streaming && inverted && armed && canStream()) held = true;
  }
};
