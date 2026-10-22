#include "../src/pose_voice.h"
#include "../src/face_down_rest.h"
#include "../src/walkie_keys.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

// Exercise the real detector and deviceTick order, loading only the persisted
// calibration signs as deviceBegin does (not controller.calibrated()).
struct Device {
  PoseDetector pose;
  PoseVoiceController controller;
  VoiceState voice;
  WalkieKeys keys;
  FaceDownRestPolicy rest;
  int upright,face;
  uint32_t lastGeneration=0;
  Device(int up=1,int screen=1):upright(up),face(screen){
    pose.uprightSign=up;pose.faceUpSign=screen;
    voice.allowed=true;voice.localSound=false;
  }
  void sample(uint32_t now,float y,float z,bool a=false,bool b=false,bool healthy=true){
    pose.update(0,y,z,0,now,voice.poseExitGuarded());
    voice.leaseAt=now;
    voice.tick(now,healthy);
    // Audio demand is evaluated before the pose controller in deviceTick.
    const bool preparing=healthy&&controller.prepareCapture(voice,pose.candidate,false);
    if(preparing)assert(voice.captureWanted(true));
    controller.update(voice,pose.stable,a,false,healthy&&pose.candidate==pose.stable);
    if(voice.streamGeneration!=lastGeneration)keys.update(false,false,false);
    keys.update(voice.streaming&&voice.inverted&&voice.canStream(),a,b);
    voice.held=keys.heldA;
    voice.tick(now,healthy);
    lastGeneration=voice.streamGeneration;
  }
  void inverted(uint32_t now,bool a=false,bool b=false,bool healthy=true){sample(now,float(-upright),0,a,b,healthy);}
  void enter(uint32_t now=100,bool a=false,bool b=false){
    inverted(now,a,b);inverted(now+219,a,b);
    assert(!voice.streaming);
    inverted(now+220,a,b);
  }
};

int main(){
  {
    // Both calibration signs, either face reference, A/B already held at boot.
    for(int up:{-1,1})for(int face:{-1,1}){
      Device d(up,face);d.voice.setPeer(true,100);d.voice.captureReady=true;
      d.enter(100,true,true);
      assert(d.controller.initialized&&d.voice.inverted&&d.voice.streaming);
      assert(!d.voice.manual&&!d.voice.held&&!d.keys.heldB);
      const auto session=d.voice.streamGeneration;
      d.inverted(340,true,true);assert(d.voice.streamGeneration==session);
      d.inverted(360);d.inverted(380,true,true);
      assert(d.voice.held&&d.keys.heldB);
    }
  }
  {
    // A boot/wake can settle inverted well before the Mac audio peer is ready.
    Device d;d.enter();assert(d.voice.inverted&&!d.voice.pendingGesture);
    d.inverted(2000);assert(!d.voice.streaming);
    d.voice.setPeer(true,2020);d.inverted(2020);
    assert(d.voice.pendingGesture&&d.voice.captureWanted());
    d.voice.captureReady=true;d.inverted(2040);assert(d.voice.streaming);
  }
  {
    // The inverted edge may also occur after a known rest pose, while offline.
    Device d;d.sample(100,0,-1);d.sample(1600,0,-1);
    assert(d.pose.stable==DevicePose::FaceDown);
    d.enter(1700);assert(!d.voice.streaming);
    d.voice.setPeer(true,2000);d.voice.captureReady=true;d.inverted(2000);
    assert(d.voice.streaming);
  }
  {
    // A slow microphone/radio startup retains intent until it is actually ready.
    Device d;d.voice.setPeer(true,100);d.enter();assert(d.voice.pendingGesture);
    for(uint32_t t=340;t<2000;t+=20){d.inverted(t);assert(!d.voice.streaming&&d.voice.pendingGesture);}
    d.voice.captureReady=true;d.inverted(2000);assert(d.voice.streaming);
  }
  {
    // Online wake and temporary protected work cannot consume the only edge.
    for(bool sleep:{false,true}){
      Device d;d.voice.setPeer(true,100);d.voice.captureReady=true;
      d.voice.sleeping=sleep;d.voice.busy=!sleep;d.enter();assert(!d.voice.streaming);
      d.voice.sleeping=d.voice.busy=false;d.inverted(340,true,true);
      assert(d.voice.streaming&&!d.voice.held&&!d.keys.heldB);
      d.voice.sleeping=sleep;d.voice.busy=!sleep;d.inverted(360,true,true);
      assert(!d.voice.streaming&&!d.keys.active);
      d.voice.sleeping=d.voice.busy=false;d.inverted(380,true,true);
      assert(d.voice.streaming&&!d.voice.held&&!d.keys.heldB);
    }
  }
  {
    // Face-down online rest can wake directly into inversion, without a
    // separately observed face-up/upright frame between sleep and voice entry.
    Device d;d.voice.setPeer(true,100);d.voice.captureReady=true;
    d.sample(100,0,-1);d.sample(1600,0,-1);
    d.rest.update(1600,d.pose.stable,d.pose.candidate,true,false,false,false);
    assert(d.rest.update(3100,d.pose.stable,d.pose.candidate,true,false,false,false)==FaceDownRestPolicy::Action::Sleep);
    d.voice.sleeping=true;d.enter(3200);assert(!d.voice.streaming);
    assert(d.rest.update(3420,d.pose.stable,d.pose.candidate,true,false,false,false)==FaceDownRestPolicy::Action::Wake);
    d.voice.sleeping=false;d.inverted(3440);assert(d.voice.streaming);
    // PM1 L1 wake reconstructs these objects just like a fresh boot.
    Device reboot(d.upright,d.face);reboot.enter(100);
    reboot.voice.setPeer(true,400);reboot.voice.captureReady=true;reboot.inverted(400);
    assert(reboot.voice.streaming);
  }
  {
    // Lease loss, explicit disconnect and mic/radio loss all release shortcuts.
    for(int reason=0;reason<3;++reason){
      Device d;d.voice.setPeer(true,100);d.voice.captureReady=true;d.enter();
      d.inverted(340,true,true);assert(d.voice.held&&d.keys.heldB);
      const auto old=d.voice.streamGeneration;
      if(reason==0)d.voice.tick(1341,true);
      else if(reason==1)d.voice.setPeer(false,360);
      else {d.voice.captureReady=false;d.voice.tick(360,true);}
      assert(!d.voice.streaming&&!d.voice.held);
      d.voice.setPeer(true,1400);d.voice.captureReady=true;d.inverted(1400,true,true);
      assert(d.voice.streaming&&d.voice.streamGeneration==old+1);
      assert(!d.voice.held&&!d.keys.heldB);
    }
  }
  {
    // Even a pause/restart in one main-loop iteration creates a new nonce/key
    // context: the final streaming bool alone would miss this transition.
    Device d;d.voice.setPeer(true,100);d.voice.captureReady=true;d.enter();
    d.inverted(340,true,true);assert(d.voice.held&&d.keys.heldB);
    const auto old=d.voice.streamGeneration;
    d.voice.setPeer(false,360);d.voice.setPeer(true,360);d.inverted(360,true,true);
    assert(d.voice.streaming&&d.voice.streamGeneration==old+1);
    assert(!d.voice.held&&!d.keys.heldB);
  }
  {
    // A stop/mute/disable stays stopped through wake and reconnect. Manual start
    // or a fresh confirmed non-inverted pose is the explicit way to re-arm it.
    for(int reason=0;reason<3;++reason){
      Device d;d.voice.setPeer(true,100);d.voice.captureReady=true;d.enter();
      if(reason==0)d.voice.stop();
      else if(reason==1)d.voice.muted=true;
      else d.voice.allowed=false;
      d.inverted(340);assert(!d.voice.streaming&&d.voice.poseRearmRequired);
      d.voice.muted=false;d.voice.allowed=true;
      d.voice.setPeer(false,360);d.voice.setPeer(true,380);d.inverted(380);
      assert(!d.voice.streaming&&!d.controller.prepareCapture(d.voice,DevicePose::Inverted,false));
      d.voice.sleeping=true;d.inverted(400);d.voice.sleeping=false;d.inverted(420);
      assert(!d.voice.streaming);
      d.sample(440,1,0);d.sample(660,1,0);assert(!d.voice.poseRearmRequired);
      d.enter(680);assert(d.voice.streaming);
      d.voice.stop();assert(d.voice.requestManual(true,940));d.inverted(940,true,true);
      assert(d.voice.streaming&&d.voice.manual&&!d.voice.held);
    }
  }
  {
    // Never restart from an old stable inversion during movement or bad input.
    Device d;d.voice.setPeer(true,100);d.voice.captureReady=true;d.enter();
    d.voice.setPeer(false,340);d.sample(340,0,1);
    d.voice.setPeer(true,360);d.sample(360,0,1);assert(!d.voice.streaming);
    d.sample(500,0,1);assert(d.pose.stable==DevicePose::FaceUp&&!d.voice.streaming);
    d.enter(520);assert(d.voice.streaming);
    d.voice.pause();d.inverted(760,false,false,false);assert(!d.voice.streaming);
    d.inverted(780);assert(d.voice.streaming);
    d.voice.pause();d.pose.update(0,-1.3f,0,0,800);
    d.controller.update(d.voice,d.pose.stable,false,false,d.pose.candidate==d.pose.stable);
    assert(!d.voice.streaming);
  }
  {
    // Pending manual intent and an active manual meeting retain their meaning.
    Device d;d.voice.setPeer(true,100);d.enter();
    assert(d.voice.requestManual(true,340));d.inverted(340,true);
    assert(d.voice.pendingManual&&!d.voice.pendingGesture);
    d.voice.captureReady=true;d.inverted(360,true);assert(d.voice.manual&&!d.voice.held);
    const auto session=d.voice.streamGeneration;
    d.sample(380,1,0);d.sample(560,1,0);
    assert(d.voice.streaming&&d.voice.manual&&!d.voice.inverted);
    assert(d.voice.streamGeneration==session);
  }
  {
    Device d;d.voice.setPeer(true,0xffffff00u);d.voice.captureReady=true;
    d.enter(0xffffff00u);assert(d.voice.streaming);
    d.voice.pause();d.inverted(20,true,true);assert(d.voice.streaming&&!d.voice.held&&!d.keys.heldB);
  }
  std::puts("12 boot/wake/reconnect pose reconciliation, capture, stop, key/session and wrap scenarios passed");
}
