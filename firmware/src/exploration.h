#pragma once
#include "landscape_input.h"

// One local exploration owns the orientation until discovery or cancellation.
// Sensor/voice policy remains independent of the decorative floating pixels.
struct Exploration {
  enum class Event { None, Locked, Found };
  static constexpr uint32_t LockMs=250, ScanMs=1100;
  static constexpr float CenterX=182, SpanX=36, AimY=70;
  static constexpr float SnapPixels=8, ReleasePixels=10;
  float offset=0, baseline=0, targetOffset=0;
  uint32_t started=0,lastTick=0,nearSince=0;
  unsigned discoveries=0;
  bool active=false,scanning=false,revealed=false,near=false;

  bool begin(float degrees,uint32_t now) {
    if(active||!isfinite(degrees))return false;
    static constexpr float targets[]={-.75f,.75f,-.35f,.4f};
    active=true;scanning=revealed=near=false;offset=0;baseline=degrees;
    targetOffset=targets[discoveries%4];lastTick=now;
    return true;
  }
  void cancel() {active=scanning=near=false;}
  float aimX()const{return CenterX+offset*SpanX;}
  float targetX()const{return CenterX+targetOffset*SpanX;}
  float probeX()const{
    if(scanning)return targetX();
    return near?aimX()+(targetX()-aimX())*.45f:aimX();
  }
  Event update(float degrees,bool awake,uint32_t now) {
    const uint32_t elapsed=now-lastTick;lastTick=now;
    if(!awake){cancel();return Event::None;}
    if(!active)return Event::None;
    if(scanning){
      if(uint32_t(now-started)<ScanMs)return Event::None;
      active=scanning=near=false;revealed=true;offset=targetOffset;discoveries++;
      return Event::Found;
    }
    if(!isfinite(degrees)){near=false;return Event::None;}
    const float delta=LandscapeTilt::delta(degrees,baseline);
    offset+=(LandscapeTilt::mapped(delta)-offset)*fminf(1,float(elapsed)/LandscapeTilt::SmoothMs);
    const float distance=fabsf(aimX()-targetX());
    if(distance>(near?ReleasePixels:SnapPixels)){near=false;return Event::None;}
    if(!near){near=true;nearSince=now;}
    if(uint32_t(now-nearSince)<LockMs)return Event::None;
    scanning=true;started=now;
    return Event::Locked;
  }
  float progress(uint32_t now)const{return scanning?fminf(1,float(uint32_t(now-started))/ScanMs):0;}
  unsigned artifact()const{return revealed&&discoveries?(discoveries-1)%4:discoveries%4;}
};
