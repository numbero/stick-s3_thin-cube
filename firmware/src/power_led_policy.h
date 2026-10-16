#pragma once
#include <stdint.h>

// Local startup readiness is independent of pairing, AP reachability and OTA
// validation. Once ready, normal runtime sleep/disconnects cannot relight it.
struct PowerLedPolicy {
  struct Health {
    bool display,motion,audio,control,radio;
    constexpr Health(bool d=false,bool m=false,bool a=false,bool c=false,bool r=false)
      :display(d),motion(m),audio(a),control(c),radio(r){}
    bool complete()const{return display&&motion&&audio&&control&&radio;}
  };
  static constexpr uint32_t StableMs=2000,MaxLoopGapMs=250;
  bool ready=false;

  void update(uint32_t now,const Health& health){
    if(ready)return;
    if(!health.complete()){observing=false;return;}
    if(!observing||uint32_t(now-lastTick)>MaxLoopGapMs){
      observing=true;since=now;
    }
    lastTick=now;
    if(uint32_t(now-since)>=StableMs)ready=true;
  }
  bool light(bool debug)const{return !ready||debug;}
 private:
  bool observing=false;
  uint32_t since=0,lastTick=0;
};
