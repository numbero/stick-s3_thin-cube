#include "../src/face_down_rest.h"
#include "../src/motion_activity.h"
#include "../src/idle_power.h"
#include "../src/voice_state.h"
#include <assert.h>
#include <stdio.h>

using Action=FaceDownRestPolicy::Action;
constexpr auto Down=DevicePose::FaceDown,Up=DevicePose::FaceUp;

struct Fixture {
  PoseDetector pose;
  MotionActivity motion;
  FaceDownRestPolicy rest;
  Fixture(){assert(pose.calibrateFaceUp(0,0,1));}
  Action sample(uint32_t now,float x,float y,float z,float gyro=0,bool busy=false,bool button=false){
    const bool moved=motion.update(x,y,z,gyro,now);
    pose.update(x,y,z,gyro,now);
    return rest.update(now,pose.stable,pose.candidate,motion.healthy(now),busy,moved,button);
  }
  void enter(uint32_t start=100){
    assert(sample(start,0,0,-1)==Action::None);
    for(uint32_t age=40;age<1500;age+=40)assert(sample(start+age,0,0,-1)==Action::None);
    assert(sample(start+1500,0,0,-1)==Action::Sleep);
    assert(rest.active);
  }
};

int main(){
  {
    Fixture f;f.enter();
    for(uint32_t t=1640;t<10000;t+=40)assert(f.sample(t,0,0,-1)==Action::None&&f.rest.active);
    VoiceState voice;voice.allowed=voice.peer=true;voice.sleeping=f.rest.active;
    assert(!voice.captureWanted()); // Connected rest never starts the microphone.
    IdlePowerPolicy idle;idle.activity(100);
    assert(idle.update(1600,false,true,true)==IdlePowerPolicy::Action::None);
    assert(idle.update(600100,false,true,true)==IdlePowerPolicy::Action::PowerOff);
    assert(idle.update(600100,false,true,false)==IdlePowerPolicy::Action::None);
  }
  {
    // Briefly turning down does not sleep; a second placement gets a full hold.
    Fixture f;assert(f.sample(100,0,0,-1)==Action::None);
    assert(f.sample(1100,0,0,-1)==Action::None);
    assert(f.sample(1120,0,0,1)==Action::None&&!f.rest.active);
    f.sample(1300,0,0,-1); // A new handling sample resets quiet admission.
    assert(f.sample(1340,0,0,-1)==Action::None);
    assert(f.sample(2839,0,0,-1)==Action::None);
    assert(f.sample(2840,0,0,-1)==Action::Sleep);
  }
  {
    // Constant 1g and low gyro: slow flipping needs no acceleration spike.
    Fixture f;f.enter();bool woke=false;
    for(unsigned degrees=1;degrees<=40;++degrees){
      const float a=degrees*.01745329252f;
      if(f.sample(1600+degrees*40,sinf(a),0,-cosf(a),1)==Action::Wake){woke=true;break;}
    }
    assert(woke&&!f.rest.active);
    assert(f.sample(3500,0,0,1)==Action::None);
    assert(f.sample(3700,0,0,1)==Action::None&&!f.rest.active);
  }
  {
    Fixture f;f.enter();
    // Desk sensor noise and a single short bump must keep the screen dark.
    for(uint32_t t=1640;t<5000;t+=40)assert(f.sample(t,.005f,0,-.99f,2)==Action::None&&f.rest.active);
    assert(f.sample(5040,0,0,-1.3f,30)==Action::None&&f.rest.active);
    assert(f.sample(5080,0,0,-1)==Action::None&&f.rest.active);
    // A held exit is confirmed after 100ms even with little translational motion.
    assert(f.sample(5120,.6f,0,-.8f,1)==Action::None);
    assert(f.sample(5240,.6f,0,-.8f,1)==Action::Wake&&!f.rest.active);
  }
  {
    // Protected work cannot enter rest; finishing it grants a fresh quiet hold.
    Fixture f;
    for(uint32_t t=100;t<4000;t+=40)assert(f.sample(t,0,0,-1,0,true)==Action::None);
    assert(f.sample(4000,0,0,-1)==Action::None);
    assert(f.sample(5499,0,0,-1)==Action::None);
    assert(f.sample(5500,0,0,-1)==Action::Sleep);
    assert(f.sample(5540,0,0,-1,0,true)==Action::Wake&&!f.rest.active);
  }
  {
    // A button wakes on the desk and does not re-sleep until a confirmed lift.
    Fixture f;f.enter();
    assert(f.sample(1640,0,0,-1,0,false,true)==Action::Wake);
    for(uint32_t t=1680;t<6000;t+=40)assert(f.sample(t,0,0,-1)==Action::None&&!f.rest.active);
    f.sample(6100,0,0,1);f.sample(6260,0,0,1);assert(!f.rest.inhibited);
    f.sample(6300,0,0,-1);assert(f.sample(6340,0,0,-1)==Action::None);
    assert(f.sample(7840,0,0,-1)==Action::Sleep);
  }
  {
    // Mac manual start uses the same override, including after stopping voice.
    Fixture f;f.enter();f.rest.overrideWake();
    assert(!f.rest.active&&f.rest.inhibited);
    assert(f.sample(1640,0,0,-1,0,true)==Action::None);
    assert(f.sample(8000,0,0,-1)==Action::None&&!f.rest.active);
  }
  {
    // Invalid or stale sensor input grants no sleep and releases existing rest.
    Fixture f;f.enter();
    assert(f.sample(1640,0,0,0)==Action::Wake&&!f.rest.active);
    assert(f.rest.update(1700,Down,Down,false,false,false,false)==Action::None);
    assert(f.sample(1800,0,0,-1)==Action::None);
    assert(f.sample(3300,0,0,-1)==Action::Sleep);
    assert(f.rest.update(4301,Down,Down,f.motion.healthy(4301),false,false,false)==Action::Wake);
  }
  {
    // Continued handling while face down delays entry, and no calibration means
    // no face-down gesture. Healthy input alone never invents an orientation.
    Fixture f;
    for(uint32_t t=100;t<5000;t+=40)assert(f.sample(t,0,0,-1,20)==Action::None);
    assert(f.sample(5040,0,0,-1)==Action::None);
    assert(f.sample(6540,0,0,-1)==Action::Sleep);
    PoseDetector uncalibrated;FaceDownRestPolicy rest;
    for(uint32_t t=0;t<10000;t+=40){uncalibrated.update(0,0,-1,0,t);
      assert(rest.update(t,uncalibrated.stable,uncalibrated.candidate,true,false,false,false)==Action::None);}
  }
  {
    Fixture f;f.enter(0xffffff00u);
    assert(f.sample(1400,0,0,1)==Action::None);
    assert(f.sample(1560,0,0,1)==Action::Wake&&!f.rest.active);
  }
  puts("10 face-down hold, slow lift, vibration, protected task, override, sensor and clock-wrap scenarios passed");
}
