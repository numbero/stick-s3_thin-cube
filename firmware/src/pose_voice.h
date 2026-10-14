#pragma once
#include "pose_detector.h"
#include "voice_state.h"

// Boot observes the initial pose without opening a microphone session. A
// successful calibration already identifies the current pose, so the user's
// next face-up -> inverted transition must be processed as a new gesture.
struct PoseVoiceController {
  bool initialized = false, suppressed = false;
  bool prepareCapture(const VoiceState& voice,DevicePose candidate,bool exploring)const{
    // Only prewarm a fresh forming inversion. Boot/reconnect/stop while held
    // inverted must not start capture or replay the cancelled voice gesture.
    return initialized&&!suppressed&&!exploring&&voice.voiceEnabled()&&
      !voice.inverted&&candidate==DevicePose::Inverted;
  }
  void calibrated(VoiceState& voice, DevicePose pose) {
    voice.stop();
    voice.inverted = pose == DevicePose::Inverted;
    initialized = true;suppressed=false;
  }
  void update(VoiceState& voice, DevicePose pose, bool buttonDown, bool exploring=false, bool poseSettled=true) {
    if(exploring||suppressed){
      if(exploring)suppressed=true;
      // Keep manual meetings alive, but never interpret play as a new gesture.
      if(!voice.manual)voice.stop();
      voice.held=voice.armed=false;
      if(pose!=DevicePose::Other){
        voice.inverted=pose==DevicePose::Inverted;initialized=true;
        if(!exploring&&poseSettled)suppressed=false;
      }
      return;
    }
    if(pose == DevicePose::Other) return;
    if(!initialized) {
      voice.inverted = pose == DevicePose::Inverted;
      initialized = true;
      return;
    }
    voice.setPose(pose == DevicePose::Inverted, buttonDown);
  }
};
