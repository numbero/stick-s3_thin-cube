#include "../src/pose_voice.h"
#include "../src/ui_status.h"
#include <assert.h>
#include <stdio.h>

VoiceState ready(){VoiceState v;v.localSound=false;v.allowed=true;v.setPeer(true,100);return v;}
int main(){
  {
    auto v=ready();assert(!v.captureWanted());
    v.localSound=true;assert(!v.captureWanted()); // Only the visible ambient meter may request local capture.
    assert(v.captureWanted(false,true));
    v.allowed=false;assert(v.captureWanted(false,true));v.localSound=false;assert(!v.captureWanted(false,true));
  }
  {
    auto v=ready();PoseVoiceController c;PoseDetector p;
    assert(p.calibrateInverted(0,-1,0));assert(p.calibrateFaceUp(0,0,1));
    c.calibrated(v,p.stable);
    p.update(0,-1,0,0,100);
    assert(c.prepareCapture(v,p.candidate,false)&&v.captureWanted(true));
    assert(p.update(0,-1,0,0,319)!=DevicePose::Inverted); // 219ms prewarming window.
    v.captureReady=true;p.update(0,-1,0,0,320);c.update(v,p.stable,false);
    assert(v.streaming&&v.captureWanted());
    v.button(true);assert(v.held);c.update(v,DevicePose::FaceUp,false);
    assert(!v.streaming&&!v.held&&!v.captureWanted());
  }
  {
    auto v=ready();PoseVoiceController c;c.update(v,DevicePose::FaceUp,false);
    c.update(v,DevicePose::Inverted,false);assert(v.pendingGesture&&v.captureWanted());
    v.tick(200,true);assert(!v.streaming);v.captureReady=true;v.tick(300,true);
    assert(v.streaming&&v.captureWanted()); // A slow Mic/radio preparation keeps only this fresh gesture.
  }
  {
    auto v=ready();assert(v.requestManual(true,200));assert(v.pendingManual&&v.captureWanted());
    v.captureReady=true;v.tick(300,true);assert(v.streaming&&v.manual);
    v.setPose(true,true);assert(!v.held);v.button(false);v.button(true);assert(v.held);
    v.setPose(false,false);assert(v.streaming&&v.manual&&!v.held);
    v.stop();assert(!v.captureWanted());
  }
  {
    auto v=ready();v.requestManual(false,100);v.leaseAt=5100;v.tick(5100,true);assert(!v.pendingManual&&!v.captureWanted());
    v.requestManual(false,5200);v.setPeer(false,5300);assert(!v.captureWanted());
    v=ready();v.requestManual(false,0xfffffff0);v.leaseAt=0xfffffff0;v.captureReady=true;v.tick(20,true);
    assert(v.streaming); // Manual cold start survives millis wrap.
  }
  {
    auto v=ready();PoseVoiceController c;
    assert(c.prepareCapture(v,DevicePose::Inverted,false));
    c.update(v,DevicePose::Inverted,false);assert(v.pendingGesture&&v.captureWanted());
    assert(c.prepareCapture(v,DevicePose::Inverted,false));
    v.setPeer(false,200);v.setPeer(true,300);assert(!v.captureWanted());
    c.update(v,DevicePose::Inverted,false);assert(v.pendingGesture&&v.captureWanted());
    c.update(v,DevicePose::FaceUp,false);v.captureReady=true;c.update(v,DevicePose::Inverted,false);
    v.stop();assert(!v.captureWanted()&&!c.prepareCapture(v,DevicePose::Inverted,false));
  }
  {
    auto v=ready();PoseVoiceController c;c.update(v,DevicePose::FaceUp,false);
    assert(!c.prepareCapture(v,DevicePose::Inverted,true));
    c.update(v,DevicePose::Inverted,false,true);assert(!v.captureWanted());
    c.update(v,DevicePose::Inverted,false,false);assert(!v.captureWanted());
    v.captureReady=true;v.requestManual(false,200);c.update(v,DevicePose::Landscape,false,true);
    assert(v.streaming&&v.manual&&v.captureWanted()); // A manual meeting survives play.
  }
  {
    auto v=ready();v.localSound=true;v.muted=true;assert(!v.captureWanted(true));
    v.muted=false;v.sleeping=true;assert(!v.captureWanted(true));
    v.sleeping=false;v.busy=true;assert(!v.captureWanted(true));
  }
  {
    UIStatus::VoiceInputs s{false,false,true,true,true,false,true,false,false};
    assert(UIStatus::voiceIcon(s)==UIStatus::VoiceIcon::None);
    s.captureDemand=true;assert(UIStatus::voiceIcon(s)==UIStatus::VoiceIcon::Warning);
    s.captureDemand=false;s.calibrated=false;assert(UIStatus::voiceIcon(s)==UIStatus::VoiceIcon::Warning);
    s.calibrated=true;s.audioReady=false;assert(UIStatus::voiceIcon(s)==UIStatus::VoiceIcon::Warning);
  }
  puts("9 on-demand capture, prewarming, pending start, rearm, game and status scenarios passed");
}
