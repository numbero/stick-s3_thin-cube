#include "../src/pose_voice.h"
#include "../src/display_motion.h"
#include "../src/face_down_rest.h"
#include "../src/motion_activity.h"
#include <assert.h>
#include <initializer_list>
#include <stdio.h>

using Action=FaceDownRestPolicy::Action;

struct Fixture {
  PoseDetector pose;
  PoseVoiceController controller;
  VoiceState voice;
  DisplayOrientation display;
  FaceDownRestPolicy rest;
  MotionActivity motion;
  int upright,face;
  Fixture(int uprightReference=1,int faceReference=1,bool micReady=true):upright(uprightReference),face(faceReference){
    assert(pose.calibrateInverted(0,float(-upright),0));
    assert(pose.calibrateFaceUp(0,0,float(face)));
    controller.calibrated(voice,pose.stable);
    voice.allowed=voice.peer=true;voice.captureReady=micReady;
  }
  Action sample(uint32_t now,float x,float y,float z,float gyro=0,bool button=false){
    // Follow deviceMotion -> UI rest -> deviceTick, refreshing the control lease.
    voice.leaseAt=now;
    const bool guarded=voice.poseExitGuarded();
    pose.update(x,y,z,gyro,now,guarded);
    display.update(x,y,z,gyro,pose.uprightSign,now,guarded);
    const bool moved=motion.update(x,y,z,gyro,now);
    const auto action=rest.update(now,pose.stable,pose.candidate,motion.healthy(now),voice.streaming,moved,button);
    voice.sleeping=rest.active;
    controller.update(voice,pose.stable,button,false,pose.candidate==pose.stable);
    voice.button(button&&!(display.rotation&1));
    voice.tick(now);
    return action;
  }
  void enter(uint32_t start=100){
    sample(start,0,float(-upright),0);
    sample(start+90,0,float(-upright),0);assert(display.rotation==2);
    sample(start+219,0,float(-upright),0);assert(!voice.streaming&&!voice.pendingGesture);
    sample(start+220,0,float(-upright),0);
    assert(voice.inverted&&voice.poseExitGuarded());
    assert(voice.captureReady?voice.streaming:voice.pendingGesture);
  }
};

int main(){
  {
    // All four natural tilt directions keep audio and the held A shortcut.
    // The session's guard survives the detector settling to moving/Other.
    for(int upright:{-1,1})for(int face:{-1,1})for(int direction=0;direction<4;++direction){
      Fixture f(upright,face);f.enter();
      const float x=direction<2?(direction==0?.8660254f:-.8660254f):0;
      const float z=direction>=2?(direction==2?.8660254f:-.8660254f):0;
      for(uint32_t now=400;now<=2400;now+=40){
        f.sample(now,x,-.5f*upright,z,0,true);
        assert(f.voice.streaming&&f.voice.inverted&&f.voice.held&&f.voice.poseExitGuarded());
        assert(f.display.rotation==2&&!f.display.pending);
      }
      assert(f.pose.stable==DevicePose::Other);
    }
  }
  {
    // The old 20-degree exit now stays ambiguous, as does 15.5 degrees at the
    // new boundary. At 14.5 degrees, a full 450ms still exits in both references.
    for(int upright:{-1,1})for(int face:{-1,1})for(int target=0;target<3;++target){
      Fixture f(upright,face);f.enter();
      uint32_t start=400;
      for(float degrees:{20.f,15.5f}){
        const float a=degrees*.01745329252f,c=cosf(a),s=sinf(a);
        const float x=target==1?c:0;
        const float y=target==0?c*upright:-s*upright;
        const float z=target==0?s:target==2?c*face:0;
        f.sample(start,x,y,z);f.sample(start+100,x,y,z);f.sample(start+1000,x,y,z);
        assert(f.pose.stable==DevicePose::Other&&f.voice.streaming);
        start+=1100;
      }
      const float a=14.5f*.01745329252f,c=cosf(a),s=sinf(a);
      const float closerX=target==1?c:0;
      const float closerY=target==0?c*upright:-s*upright;
      const float closerZ=target==0?s:target==2?c*face:0;
      f.sample(start,closerX,closerY,closerZ);
      f.sample(start+449,closerX,closerY,closerZ);assert(f.voice.streaming);
      f.sample(start+450,closerX,closerY,closerZ);assert(!f.voice.streaming&&!f.voice.inverted);
    }
  }
  {
    // Each ordinary mode stays reachable, including either landscape side.
    for(int target=0;target<4;++target){
      Fixture f;f.enter();
      const float x=target==1?1.f:target==2?-1.f:0;
      const float y=target==0?1.f:0,z=target==3?1.f:0;
      f.sample(400,x,y,z,0,true);assert(f.voice.held);
      f.sample(849,x,y,z,0,true);assert(f.voice.streaming&&f.voice.held&&f.display.rotation==2);
      f.sample(850,x,y,z,0,true);assert(!f.voice.streaming&&!f.voice.held&&!f.voice.inverted);
      f.sample(860,x,y,z);f.sample(950,x,y,z);
      assert(f.display.rotation==(target==0?0:target==1?1:target==2?3:2));
      f.sample(1000,0,-1,0,0,true);f.sample(1220,0,-1,0,0,true);
      assert(f.voice.streaming&&!f.voice.held); // A held before entry must be released first.
      f.sample(1230,0,-1,0);f.sample(1240,0,-1,0,0,true);assert(f.voice.held);
    }
  }
  {
    // A brief visit, a change of target, or a return to moving resets the hold.
    Fixture f;f.enter();
    f.sample(400,1,0,0);f.sample(700,1,0,0);assert(f.voice.streaming);
    f.sample(800,.7071068f,-.7071068f,0);
    f.sample(850,1,0,0);f.sample(1299,1,0,0);assert(f.voice.streaming);
    f.sample(1300,0,0,1);f.sample(1749,0,0,1);assert(f.voice.streaming);
    f.sample(1750,0,0,1);assert(!f.voice.streaming);
  }
  {
    // Invalid acceleration and fast rotation cannot accumulate an exit hold.
    for(int invalid=0;invalid<2;++invalid){
      Fixture f;f.enter();f.sample(400,0,0,1);
      f.sample(800,0,0,invalid?1.f:0,invalid?150.f:0);
      f.sample(820,0,0,1);f.sample(1269,0,0,1);assert(f.voice.streaming);
      f.sample(1270,0,0,1);assert(!f.voice.streaming);
    }
  }
  {
    // Face-down keeps its original 1.5s recognition, then follows the existing
    // quiet-rest path. Lifting wakes without starting an unsolicited session.
    for(int face:{-1,1}){
      Fixture f(1,face);f.enter();
      f.sample(400,0,0,float(-face));f.sample(1899,0,0,float(-face));assert(f.voice.streaming);
      f.sample(1900,0,0,float(-face));assert(f.pose.stable==DevicePose::FaceDown&&!f.voice.streaming&&!f.voice.held);
      f.sample(1940,0,0,float(-face));assert(f.sample(3439,0,0,float(-face))==Action::None);
      assert(f.sample(3440,0,0,float(-face))==Action::Sleep);
      assert(f.voice.sleeping&&!f.voice.captureWanted());
      f.sample(3500,.6f,0,-.8f*face);assert(f.sample(3600,.6f,0,-.8f*face)==Action::Wake);
      assert(!f.voice.sleeping&&!f.voice.streaming);
      f.sample(3700,0,-1,0);f.sample(3920,0,-1,0);assert(f.voice.streaming);
    }
  }
  {
    // A fresh gesture pending microphone startup gets the same exit tolerance.
    Fixture f(1,1,false);f.enter();
    f.sample(400,.8660254f,-.5f,0);f.sample(1000,.8660254f,-.5f,0);
    assert(f.voice.pendingGesture&&f.voice.poseExitGuarded());
    f.voice.captureReady=true;f.sample(1040,.8660254f,-.5f,0);assert(f.voice.streaming);
    f.sample(1100,1,0,0);f.sample(1550,1,0,0);assert(!f.voice.streaming);
  }
  {
    // Explicit stop/mute/disconnect always bypasses the guard. Holding the old
    // inverted pose cannot resume a stopped session.
    for(int reason=0;reason<3;++reason){
      Fixture f;f.enter();f.voice.button(true);assert(f.voice.held);
      if(reason==0)f.voice.stop();
      else if(reason==1){f.voice.muted=true;f.voice.tick(320);}
      else f.voice.setPeer(false,320);
      assert(!f.voice.poseExitGuarded()&&!f.voice.held);
      f.voice.muted=false;f.voice.setPeer(true,400);
      f.sample(400,0,-1,0);f.sample(1000,0,-1,0);assert(!f.voice.streaming);
    }
  }
  {
    Fixture f;f.enter();assert(f.voice.startManual(false));
    f.sample(400,1,0,0,0,true);f.sample(849,1,0,0,0,true);assert(f.voice.held);
    f.sample(850,1,0,0,0,true);assert(f.voice.streaming&&f.voice.manual&&!f.voice.inverted&&!f.voice.held);
  }
  {
    Fixture f;f.enter(0xffffff00u);
    f.sample(0xfffffff0u,1,0,0);f.sample(433,1,0,0);assert(f.voice.streaming);
    f.sample(434,1,0,0);assert(!f.voice.streaming);
  }
  {
    // Normal standby posture responsiveness is unchanged outside a session.
    PoseDetector p;assert(p.calibrateInverted(0,-1,0));assert(p.calibrateFaceUp(0,0,1));
    p.update(.8660254f,-.5f,0,0,100);
    assert(p.update(.8660254f,-.5f,0,0,260)==DevicePose::Landscape);
    p.update(0,0,1,0,300);assert(p.update(0,0,1,0,460)==DevicePose::FaceUp);
    p.update(0,1,0,0,500);assert(p.update(0,1,0,0,720)==DevicePose::Upright);
  }
  puts("11 walkie tilt tolerance, deliberate exits, rotation, rest, re-arm and clock-wrap scenarios passed");
}
