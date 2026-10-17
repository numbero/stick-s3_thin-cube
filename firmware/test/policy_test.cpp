#include "../src/voice_state.h"
#include "../src/pose_detector.h"
#include <assert.h>
#include <stdio.h>

VoiceState ready(){VoiceState v;v.allowed=true;v.captureReady=true;v.setPeer(true,100);return v;}
int main(){
  {
    auto v=ready();v.setPose(true,false);assert(v.streaming);v.button(true);assert(v.held);v.button(false);assert(!v.held&&v.streaming);
    v.setPose(false,false);assert(!v.streaming);
  }
  {
    auto v=ready();v.setPose(true,true);v.button(true);assert(v.streaming&&!v.held);v.button(false);v.button(true);assert(v.held);
  }
  {
    auto v=ready();v.setPose(true,false);v.button(true);v.muted=true;v.tick(200);
    assert(!v.streaming&&!v.held&&!v.captureWanted());v.muted=false;v.setPose(true,false);assert(!v.streaming);
    v.setPose(false,false);v.setPose(true,false);assert(v.streaming);
  }
  {
    auto v=ready();v.setPose(true,false);v.button(true);v.setPeer(false,200);v.setPeer(true,300);v.setPose(true,false);
    assert(!v.streaming&&!v.held); // Reconnect never restarts an inverted session.
  }
  {
    auto v=ready();v.setPose(true,false);v.stop();v.setPose(true,false);assert(!v.streaming);
    v.allowed=false;assert(!v.captureWanted());assert(v.captureWanted(false,true)); // Local ambient meter is independent of network voice permission.
    v.localSound=false;assert(!v.captureWanted());
  }
  {
    auto v=ready();assert(v.startManual(false));v.setPose(true,false);v.button(true);assert(v.held&&v.manual);
    v.setPose(false,false);assert(v.streaming&&v.manual&&!v.held); // Pose exit does not kill a manually-started meeting.
  }
  {
    auto v=ready();v.setPose(true,false);v.button(true);v.tick(1101);assert(!v.peer&&!v.streaming&&!v.held);
    v=ready();v.leaseAt=0xfffffff0;v.tick(20);assert(v.peer);v.tick(1100);assert(!v.peer); // Millis wrap
  }
  {
    auto v=ready();v.setPose(true,false);v.busy=true;v.tick(200);assert(!v.streaming&&!v.captureWanted());
    v.busy=false;v.setPose(true,false);assert(!v.streaming);v.sleeping=true;assert(!v.captureWanted());
  }
  {
    PoseDetector p;assert(!p.calibrateUpright(0,0,1));assert(p.calibrateUpright(0,-1,0));assert(p.calibrateFaceUp(0,0,1));
    p.update(0,-1,0,0,100);assert(p.update(0,-1,0,0,800)==DevicePose::Upright);
    p.update(0,1,0,0,900);assert(p.update(0,1,0,0,1119)==DevicePose::Upright);
    assert(p.update(0,1,0,0,1120)==DevicePose::Inverted);
    p.update(0,0,1,0,1200);assert(p.update(0,0,1,0,1360)==DevicePose::FaceUp); // flat rotation is not inversion
    p.update(0,0,-1,0,1400);assert(p.update(0,0,-1,0,2899)==DevicePose::FaceUp);
    assert(p.update(0,0,-1,0,2900)==DevicePose::FaceDown);
  }
  {
    // Calibrating from the inverted USB-friendly pose must preserve the meaning
    // of upright/inverted for either physical sensor axis direction.
    for(int sign=-1;sign<=1;sign+=2) {
      PoseDetector p;p.stable=p.candidate=DevicePose::Upright;
      assert(p.calibrateInverted(0,float(sign),0));assert(p.uprightSign==-sign);
      assert(p.stable==DevicePose::Inverted&&p.candidate==DevicePose::Inverted);
      p.update(0,float(sign),0,0,100);
      assert(p.update(0,float(sign),0,0,799)==DevicePose::Inverted);
      assert(p.update(0,float(sign),0,0,800)==DevicePose::Inverted);
      p.update(0,float(-sign),0,0,900);
      assert(p.update(0,float(-sign),0,0,1600)==DevicePose::Upright);
      assert(p.calibrateFaceUp(0,0,1));
      p.update(0,0,1,0,1700);assert(p.update(0,0,1,0,2400)==DevicePose::FaceUp);
    }
  }
  {
    PoseDetector p;assert(p.calibrateInverted(0,1,0));
    p.stable=DevicePose::Inverted;
    assert(!p.calibrateInverted(0,0,1)); // Screen-up flat is not vertical.
    assert(!p.calibrateInverted(1,0,0)); // Landscape is not vertical.
    assert(!p.calibrateInverted(0,.7f,.7f));
    assert(!p.calibrateInverted(0,NAN,0));
    assert(p.uprightSign==-1&&p.stable==DevicePose::Inverted); // Failed attempts preserve valid calibration.
    PoseDetector legacy;assert(legacy.calibrateUpright(0,-1,0));
    assert(legacy.uprightSign==p.uprightSign); // Existing NVS values keep their meaning.
  }
  {
    auto v=ready();v.captureReady=false;v.setPose(true,false);
    assert(v.pendingGesture&&!v.streaming);v.tick(200);assert(!v.streaming);
    v.captureReady=true;v.tick(300);assert(v.streaming&&!v.pendingGesture);
  }
  {
    auto v=ready();v.captureReady=false;v.setPose(true,false);assert(v.pendingGesture);
    v.muted=true;v.tick(200);assert(!v.pendingGesture&&!v.streaming);
    v.muted=false;v.captureReady=true;v.tick(300);assert(!v.streaming);
    v.setPose(true,false);assert(!v.streaming); // Recovery cannot replay a cancelled gesture.
    v.setPose(false,false);v.setPose(true,false);assert(v.streaming);
  }
  puts("13 voice/pose policy scenarios passed");
}
