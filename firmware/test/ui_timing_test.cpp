#include "../src/ui_timing.h"
#include "../src/pose_voice.h"
#include <assert.h>
#include <stdio.h>

int main(){
  {
    // loop() sampled its clock before I2C/UI work. goWake() records millis()
    // slightly later; that must not look like weeks of inactivity.
    const uint32_t loopAt=100000,wakeAt=100001;
    assert(petStateElapsed(loopAt,wakeAt)==0);
    assert(!petIdleSleepDue(loopAt,wakeAt,60,false,false,false));
  }
  {
    // Repeat the actual sleep -> new inverted pose -> UI wake -> deviceTick
    // sequence. A same-loop re-sleep would consume the new edge while disabled.
    VoiceState v;v.allowed=true;v.captureReady=true;v.setPeer(true,100);
    PoseVoiceController controller;controller.update(v,DevicePose::FaceUp,false);
    for(unsigned cycle=0;cycle<4;cycle++){
      const uint32_t now=100+cycle*100;
      controller.update(v,cycle%2?DevicePose::Landscape:DevicePose::FaceUp,false);
      v.sleeping=true;v.tick(now);assert(!v.streaming);
      const uint32_t wakeAt=now+1;
      v.sleeping=petIdleSleepDue(now,wakeAt,60,v.streaming,v.inverted,false);
      controller.update(v,DevicePose::Inverted,false);v.tick(now);
      assert(!v.sleeping&&v.streaming&&v.inverted&&!v.manual);
    }
  }
  {
    // A newly entered curious/wake/dizzy state must survive its entry frame.
    assert(petStateElapsed(200,203)==0);
    assert(petStateElapsed(3500,203)==3297);
    assert(petStateElapsed(3704,203)>3500);
  }
  {
    assert(!petIdleSleepDue(30100,100,31,false,false,false));
    assert(petIdleSleepDue(30101,100,31,false,false,false));
    assert(!petIdleSleepDue(30101,100,30,false,false,false));
    assert(!petIdleSleepDue(30101,100,31,true,false,false));
    assert(!petIdleSleepDue(30101,100,31,false,true,false));
    assert(!petIdleSleepDue(30101,100,31,false,false,true));
  }
  {
    assert(petStateElapsed(24,0xfffffff0u)==40);
    assert(petStateElapsed(0xfffffff0u,24)==0);
    assert(petIdleSleepDue(30000,0xfffffff0u,31,false,false,false));
  }
  {
    // Fixing wake timers must not re-open sessions after explicit stop/mute.
    VoiceState v;v.allowed=true;v.captureReady=true;v.setPeer(true,100);
    PoseVoiceController controller;controller.update(v,DevicePose::FaceUp,false);
    controller.update(v,DevicePose::Inverted,false);assert(v.streaming);
    v.stop();controller.update(v,DevicePose::Inverted,false);assert(!v.streaming);
    controller.update(v,DevicePose::FaceUp,false);v.muted=true;
    controller.update(v,DevicePose::Inverted,false);assert(!v.streaming);
    v.muted=false;controller.update(v,DevicePose::Inverted,false);assert(!v.streaming);
    controller.update(v,DevicePose::FaceUp,false);controller.update(v,DevicePose::Inverted,false);assert(v.streaming);
  }
  puts("6 UI wake clock, repeat inversion, idle guard and wrap scenarios passed");
}
