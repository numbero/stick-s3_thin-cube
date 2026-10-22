#pragma once
#include "pose_detector.h"
#include "voice_state.h"

// A confirmed inversion is a standing request, including boot/wake/reconnect.
// Calibration and play explicitly consume it until the next pose exit.
struct PoseVoiceController {
  bool initialized = false, suppressed = false;
  bool prepareCapture(const VoiceState& voice,DevicePose candidate,bool exploring)const{
    return !suppressed&&!exploring&&!voice.poseRearmRequired&&voice.voiceEnabled()&&
      !voice.streaming&&!voice.pendingManual&&candidate==DevicePose::Inverted;
  }
  void calibrated(VoiceState& voice, DevicePose pose) {
    voice.stop();
    voice.inverted = pose == DevicePose::Inverted;
    voice.poseRearmRequired = voice.inverted;
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
        voice.poseRearmRequired=voice.inverted;
        if(!exploring&&poseSettled)suppressed=false;
      }
      return;
    }
    if(pose == DevicePose::Other) return;
    initialized = true;
    voice.setPose(pose == DevicePose::Inverted, buttonDown, poseSettled);
  }
};
