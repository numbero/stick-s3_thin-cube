#pragma once
#include "landscape_input.h"

// One local exploration owns the orientation until discovery or cancellation.
// Sensor/voice policy remains independent of the decorative floating pixels.
struct Exploration {
  enum class Event { None, Locked, Found };
  static constexpr uint32_t LockMs=250, ScanMs=1100;
  static constexpr float CenterX=182, SpanX=36, CenterY=70, SpanY=18;
  static constexpr float SnapPixels=8, ReleasePixels=10;
  float offset=0, baseline=0, targetOffset=0;
  float offsetY=0, baselineY=0, targetOffsetY=0;
  uint32_t started=0,lastTick=0,nearSince=0;
  unsigned discoveries=0;
  bool active=false,scanning=false,revealed=false,near=false;

  bool begin(float degrees,float vertical,uint32_t now) {
    if(active||!isfinite(degrees)||!isfinite(vertical))return false;
    static constexpr float targets[][2]={{-.75f,-.75f},{.75f,.75f},{-.35f,.5f},{.4f,-.5f}};
    active=true;scanning=revealed=near=false;offset=0;baseline=degrees;
    offsetY=0;baselineY=vertical;
    targetOffset=targets[discoveries%4][0];targetOffsetY=targets[discoveries%4][1];lastTick=now;
    return true;
  }
  void cancel() {active=scanning=near=false;}
  float aimX()const{return CenterX+offset*SpanX;}
  float aimY()const{return CenterY+offsetY*SpanY;}
  float targetX()const{return CenterX+targetOffset*SpanX;}
  float targetY()const{return CenterY+targetOffsetY*SpanY;}
  float probeX()const{
    if(scanning)return targetX();
    return near?aimX()+(targetX()-aimX())*.45f:aimX();
  }
  float probeY()const{
    if(scanning)return targetY();
    return near?aimY()+(targetY()-aimY())*.45f:aimY();
  }
  Event update(float degrees,float vertical,bool awake,uint32_t now) {
    const uint32_t elapsed=now-lastTick;lastTick=now;
    if(!awake){cancel();return Event::None;}
    if(!active)return Event::None;
    if(scanning){
      if(uint32_t(now-started)<ScanMs)return Event::None;
      active=scanning=near=false;revealed=true;offset=targetOffset;offsetY=targetOffsetY;discoveries++;
      return Event::Found;
    }
    if(!isfinite(degrees)||!isfinite(vertical)){near=false;return Event::None;}
    const float smooth=fminf(1,float(elapsed)/LandscapeTilt::SmoothMs);
    offset+=(LandscapeTilt::mapped(LandscapeTilt::delta(degrees,baseline))-offset)*smooth;
    offsetY+=(LandscapeTilt::mapped(LandscapeTilt::delta(vertical,baselineY))-offsetY)*smooth;
    const float distance=hypotf(aimX()-targetX(),aimY()-targetY());
    if(distance>(near?ReleasePixels:SnapPixels)){near=false;return Event::None;}
    if(!near){near=true;nearSince=now;}
    if(uint32_t(now-nearSince)<LockMs)return Event::None;
    scanning=true;started=now;
    return Event::Locked;
  }
  float progress(uint32_t now)const{return scanning?fminf(1,float(uint32_t(now-started))/ScanMs):0;}
  unsigned artifact()const{return revealed&&discoveries?(discoveries-1)%4:discoveries%4;}
};
