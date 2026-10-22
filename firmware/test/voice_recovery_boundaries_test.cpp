#include "../src/pose_voice.h"
#include "../src/pairing_policy.h"
#include "../src/button_gestures.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

VoiceState ready(){VoiceState v;v.allowed=v.captureReady=true;v.setPeer(true,100);return v;}

int main(){
  {
    // Pairing is reachable while offline and held inverted. Cancellation,
    // expiration and successful completion all leave a temporary pause.
    for(int end=0;end<3;++end)for(bool stopped:{false,true}){
      auto v=ready();v.setPeer(false,100);PoseVoiceController c;
      c.update(v,DevicePose::Inverted,false);assert(!v.streaming&&!v.poseRearmRequired);
      if(stopped)v.stop();
      ButtonGestures buttons;
      buttons.update(100,true,true,false,true,true);
      buttons.update(3100,true,true,false,true,true);
      assert(buttons.update(3101,false,false,true,true,true)==ButtonGestures::Action::Pair);
      PairingPolicy pair;pair.offer(3200,true);
      // deviceOpenPairing releases audio/keys and the authenticated peer.
      v.pause();v.setPeer(false,3200);v.busy=true;
      c.update(v,DevicePose::Inverted,false);
      if(end==1)assert(pair.expire(123200,true));else pair.close();
      assert(!pair.visible);
      v.busy=false;v.setPeer(true,123220);v.tick(123220,true);
      c.update(v,DevicePose::Inverted,true);
      assert(v.streaming==!stopped&&v.poseRearmRequired==stopped&&!v.held);
      if(stopped){
        c.update(v,DevicePose::Upright,false);
        c.update(v,DevicePose::Inverted,false);assert(v.streaming);
      }
    }
  }
  {
    // A stale stable non-inverted pose cannot clear an explicit stop while
    // inversion is already forming. Exercise each ordinary starting pose.
    for(int sign:{-1,1})for(int start=0;start<3;++start){
      auto v=ready();PoseVoiceController c;PoseDetector p;
      p.uprightSign=sign;p.faceUpSign=1;
      const float x=start==2?1.f:0,y=start==0?float(sign):0,z=start==1?1.f:0;
      p.update(x,y,z,0,100);p.update(x,y,z,0,320);
      c.update(v,p.stable,false);assert(v.startManual(false));
      p.update(0,float(-sign),0,0,400);
      assert(p.candidate==DevicePose::Inverted&&p.stable!=DevicePose::Inverted);
      v.stop();c.update(v,p.stable,false,false,false);
      assert(v.poseRearmRequired&&!v.streaming);
      p.update(0,float(-sign),0,0,620);c.update(v,p.stable,false);
      assert(v.poseRearmRequired&&!v.streaming);
      // A newly confirmed real exit still re-arms the next inversion.
      p.update(x,y,z,0,700);p.update(x,y,z,0,920);c.update(v,p.stable,false);
      assert(!v.poseRearmRequired);
      p.update(0,float(-sign),0,0,1000);p.update(0,float(-sign),0,0,1220);
      v.setPeer(true,1220);c.update(v,p.stable,false);assert(v.streaming);
    }
  }
  {
    // Stale IMU cannot clear the stop even with matching old candidates.
    auto v=ready();PoseVoiceController c;c.update(v,DevicePose::FaceUp,false);
    v.stop();c.update(v,DevicePose::FaceUp,false,false,false);
    assert(v.poseRearmRequired);
    c.update(v,DevicePose::Inverted,false);assert(!v.streaming);
    c.update(v,DevicePose::Upright,false);c.update(v,DevicePose::Inverted,false);
    assert(v.streaming);
  }
  {
    // Cover both tick calls: readiness known before deviceTick, and readiness
    // becoming true in audioTick after the first policy advance.
    for(bool readyBefore:{false,true})for(int sign:{-1,1}){
      auto v=ready();v.captureReady=false;PoseVoiceController c;PoseDetector p;
      p.uprightSign=sign;p.faceUpSign=1;
      p.update(0,float(-sign),0,0,100);c.update(v,p.stable,false);
      p.update(0,float(-sign),0,0,320);c.update(v,p.stable,false);
      assert(v.pendingGesture&&!v.streaming);
      v.setPeer(true,1400);v.captureReady=readyBefore;
      v.tick(1400,false);v.captureReady=true;
      c.update(v,p.stable,false,false,false);v.tick(1400,false);
      assert(!v.streaming&&v.pendingGesture&&v.streamGeneration==0);
      for(uint32_t now=1420;now<1600;now+=20){v.setPeer(true,now);v.tick(now,false);assert(!v.streaming);}
      v.tick(1600,true);assert(v.streaming&&!v.pendingGesture&&v.streamGeneration==1);
      v.tick(1620,true);assert(v.streamGeneration==1);
    }
  }
  {
    // The health gate applies to starting posture capture. Existing speech
    // retains tilt tolerance, and an explicit manual start has no pose demand.
    auto v=ready();v.setPose(true,false);v.tick(200,false);assert(v.streaming);
    v.stop();v.tick(220,true);assert(!v.streaming&&v.poseRearmRequired);
    assert(v.requestManual(true,240));assert(v.streaming&&v.manual&&!v.held);
    v.stop();v.captureReady=false;assert(v.requestManual(false,260));
    v.captureReady=true;v.tick(280,false);assert(v.streaming&&v.manual);
  }
  {
    auto v=ready();v.captureReady=false;v.setPose(true,false);
    v.setPeer(true,0xfffffff0u);v.tick(20,false);v.captureReady=true;
    v.tick(30,false);assert(!v.streaming&&v.pendingGesture);
    v.tick(40,true);assert(v.streaming);
    v.stop();v.setPose(false,false,false);assert(v.poseRearmRequired);
    v.setPose(false,false,true);assert(!v.poseRearmRequired);
  }
  puts("6 pairing recovery, forming-pose stop, stale input/capture readiness and wrap scenario groups passed");
}
