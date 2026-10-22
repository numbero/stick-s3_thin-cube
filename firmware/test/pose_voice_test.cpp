#include "../src/pose_voice.h"
#include <assert.h>
#include <stdio.h>

VoiceState ready(){VoiceState v;v.allowed=true;v.captureReady=true;v.setPeer(true,100);return v;}
int main(){
  {
    // The first inversion after face-up calibration used to be swallowed as
    // initialization. Exercise the same controller used by deviceTick.
    PoseDetector p;auto v=ready();PoseVoiceController controller;
    assert(p.calibrateInverted(0,-1,0));controller.calibrated(v,p.stable);
    assert(!v.streaming&&v.inverted);
    assert(p.calibrateFaceUp(0,0,1));controller.calibrated(v,p.stable);
    assert(!v.streaming&&!v.inverted);
    p.update(0,-1,0,0,100);controller.update(v,p.stable,false);assert(!v.streaming);
    p.update(0,-1,0,0,319);controller.update(v,p.stable,false);assert(!v.streaming);
    p.update(0,-1,0,0,320);controller.update(v,p.stable,false);assert(v.streaming);
  }
  {
    PoseDetector p;auto v=ready();PoseVoiceController controller;
    assert(p.calibrateInverted(0,-1,0));assert(p.calibrateFaceUp(0,0,1));
    assert(p.calibrateInverted(0,-1,0));controller.calibrated(v,p.stable);
    controller.update(v,DevicePose::Inverted,false);assert(!v.streaming);
    p.update(0,0,1,0,100);controller.update(v,p.stable,false);
    p.update(0,0,1,0,800);controller.update(v,p.stable,false);assert(!v.inverted);
    p.update(0,-1,0,0,900);controller.update(v,p.stable,false);
    p.update(0,-1,0,0,1600);controller.update(v,p.stable,false);assert(v.streaming);
  }
  {
    auto v=ready();PoseVoiceController controller;
    controller.update(v,DevicePose::Other,false);assert(!controller.initialized);
    controller.update(v,DevicePose::FaceUp,false);assert(controller.initialized&&!v.streaming);
    controller.update(v,DevicePose::Inverted,false);assert(v.streaming);
  }
  {
    auto v=ready();PoseVoiceController controller;
    controller.update(v,DevicePose::Inverted,false);assert(v.inverted&&v.streaming);
    v.setPeer(false,200);v.setPeer(true,300);
    controller.update(v,DevicePose::Inverted,false);assert(v.streaming);
    controller.update(v,DevicePose::FaceUp,false);
    controller.update(v,DevicePose::Inverted,false);assert(v.streaming);
    v.button(true);assert(v.held);
    controller.update(v,DevicePose::FaceUp,false);assert(!v.streaming&&!v.held&&!v.inverted);
  }
  {
    auto v=ready();PoseVoiceController controller;
    controller.update(v,DevicePose::FaceUp,false);assert(v.startManual(false));
    controller.update(v,DevicePose::Inverted,false);v.button(true);assert(v.held);
    controller.update(v,DevicePose::FaceUp,false);assert(v.streaming&&v.manual&&!v.held);
    controller.calibrated(v,DevicePose::FaceUp);assert(!v.streaming&&!v.manual);
  }
  {
    PoseDetector p;assert(p.calibrateInverted(0,-1,0));
    auto v=ready();PoseVoiceController controller;controller.update(v,DevicePose::FaceUp,false);
    controller.update(v,DevicePose::Inverted,false);v.button(true);assert(v.streaming&&v.held);
    p.update(1,0,0,0,100);assert(p.update(1,0,0,0,259)==DevicePose::Inverted);
    assert(p.update(1,0,0,0,260)==DevicePose::Landscape);
    controller.update(v,p.stable,true);assert(!v.streaming&&!v.held&&!v.inverted);
    v.button(true);assert(!v.held);
    controller.update(v,DevicePose::Inverted,true);v.button(true);assert(v.streaming&&!v.held);
    v.button(false);v.button(true);assert(v.held);
  }
  {
    auto v=ready();PoseVoiceController controller;controller.update(v,DevicePose::FaceUp,false);
    assert(v.startManual(false));controller.update(v,DevicePose::Inverted,false);v.button(true);assert(v.held);
    controller.update(v,DevicePose::Landscape,true);assert(v.streaming&&v.manual&&!v.held&&!v.inverted);
    v.button(true);assert(!v.held); // Horizontal A can play without stopping a manual meeting.
  }
  {
    auto v=ready();PoseVoiceController c;c.update(v,DevicePose::Landscape,false);
    c.update(v,DevicePose::Inverted,true,true);v.button(false);assert(!v.streaming&&!v.held&&c.suppressed);
    c.update(v,DevicePose::Inverted,false,false);assert(!v.streaming&&!c.suppressed);
    c.update(v,DevicePose::Inverted,false);assert(!v.streaming);
    c.update(v,DevicePose::Landscape,false);c.update(v,DevicePose::Inverted,false);assert(v.streaming);
  }
  {
    auto v=ready();PoseVoiceController c;c.update(v,DevicePose::Landscape,false);
    c.update(v,DevicePose::Other,false,true);
    c.update(v,DevicePose::Other,false,false);assert(c.suppressed&&!v.streaming);
    // Do not re-arm from the old stable landscape while an inversion is still forming.
    c.update(v,DevicePose::Landscape,false,false,false);assert(c.suppressed&&!v.streaming);
    c.update(v,DevicePose::Inverted,false,false,true);assert(!c.suppressed&&!v.streaming);
    c.update(v,DevicePose::Inverted,false);assert(!v.streaming);
    c.update(v,DevicePose::FaceUp,false);c.update(v,DevicePose::Inverted,false);assert(v.streaming);
  }
  {
    auto v=ready();PoseVoiceController c;c.update(v,DevicePose::FaceUp,false);
    assert(v.startManual(false));c.update(v,DevicePose::Inverted,false);v.button(true);assert(v.held);
    c.update(v,DevicePose::Inverted,true,true);assert(v.streaming&&v.manual&&!v.held&&!v.armed);
    c.update(v,DevicePose::Landscape,false,false);assert(v.streaming&&v.manual&&!v.held);
  }
  {
    auto v=ready();PoseVoiceController c;c.update(v,DevicePose::FaceUp,false);
    c.update(v,DevicePose::Inverted,false);v.button(true);assert(v.streaming&&v.held);
    c.update(v,DevicePose::Inverted,true,true);assert(!v.streaming&&!v.held&&!v.pendingGesture);
    c.calibrated(v,DevicePose::Landscape);assert(!c.suppressed&&!v.streaming);
    c.update(v,DevicePose::Inverted,true);assert(v.streaming&&!v.held);
  }
  puts("11 pose/voice transition and exploration isolation scenarios passed");
}
