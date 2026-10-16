#include "../src/power_led_policy.h"
#include "../src/motion_activity.h"
#include "../src/voice_state.h"
#include <assert.h>
#include <stdio.h>

const PowerLedPolicy::Health Healthy{true,true,true,true,true};
void run(PowerLedPolicy& policy,uint32_t start,uint32_t duration,
         const PowerLedPolicy::Health& health=Healthy){
  for(uint32_t age=0;age<=duration;age+=50)policy.update(start+age,health);
}

int main(){
  {
    // The LED covers setup, rather than just a delay measured from power-on.
    PowerLedPolicy p;
    assert(p.light(false)&&p.light(true));
    run(p,0,10000,{});assert(!p.ready&&p.light(false));
    run(p,10050,1950);assert(!p.ready&&p.light(false));
    p.update(12050,Healthy);assert(p.ready&&!p.light(false));
  }
  {
    // Every local initialization failure must prevent an apparent good boot.
    for(unsigned failed=0;failed<5;failed++){
      PowerLedPolicy p;
      auto health=Healthy;
      switch(failed){
        case 0:health.display=false;break;
        case 1:health.motion=false;break;
        case 2:health.audio=false;break;
        case 3:health.control=false;break;
        case 4:health.radio=false;break;
      }
      run(p,0,10000,health);assert(!p.ready&&p.light(false));
      run(p,10050,2000);assert(p.ready&&!p.light(false));
    }
  }
  {
    // A transient failure resets the two-second observation window.
    PowerLedPolicy p;run(p,100,1950);assert(!p.ready);
    p.update(2075,{});run(p,2100,1950);assert(!p.ready);
    p.update(4100,Healthy);assert(p.ready);
  }
  {
    // A blocked main loop cannot pass just because enough time has elapsed.
    PowerLedPolicy p;run(p,100,1500);
    p.update(5000,Healthy);assert(!p.ready&&p.light(false));
    run(p,5050,1900);assert(!p.ready);
    p.update(7000,Healthy);assert(p.ready);
  }
  {
    // A stale/invalid IMU sample restarts observation; fresh data can recover.
    PowerLedPolicy p;MotionActivity motion;
    motion.update(0,0,1,0,0);
    for(uint32_t now=0;now<=3500;now+=50){
      auto health=Healthy;health.motion=motion.healthy(now);p.update(now,health);
    }
    assert(!p.ready&&p.light(false));
    for(uint32_t now=3550;now<=5050;now+=50){
      motion.update(0,0,1,0,now);
      auto health=Healthy;health.motion=motion.healthy(now);p.update(now,health);
    }
    assert(!p.ready);
    motion.update(0,0,0,0,5100);
    auto invalid=Healthy;invalid.motion=motion.healthy(5100);p.update(5100,invalid);
    assert(!invalid.motion&&!p.ready);
    for(uint32_t now=5150;now<=7150;now+=50){
      motion.update(0,0,1,0,now);
      auto health=Healthy;health.motion=motion.healthy(now);p.update(now,health);
    }
    assert(p.ready&&!p.light(false));
  }
  {
    // Muted/offline/provisioning use need no PCM or host handshake to be ready.
    VoiceState voice;voice.muted=true;voice.localSound=false;voice.busy=true;
    assert(!voice.captureWanted()&&!voice.peer);
    PowerLedPolicy p;run(p,0,2000);assert(p.ready&&!p.light(false));
    // Runtime sleep, network loss or a long idle gap do not re-enter boot mode.
    p.update(500000,{});assert(p.ready&&!p.light(false));
  }
  {
    // Disabling debug during boot never hides startup; afterwards it is instant.
    PowerLedPolicy p;assert(p.light(true)&&p.light(false));
    run(p,0,2000);assert(p.ready&&p.light(true)&&!p.light(false));
    PowerLedPolicy reboot;assert(!reboot.ready&&reboot.light(false));
  }
  {
    PowerLedPolicy p;run(p,0xfffffc00u,1950);assert(!p.ready);
    p.update(0xfffffc00u+2000u,Healthy);assert(p.ready&&!p.light(false));
  }
  puts("8 startup LED scenarios: local readiness, faults, recovery, loop stalls, sensor freshness, offline/mute, debug and wrap passed");
}
