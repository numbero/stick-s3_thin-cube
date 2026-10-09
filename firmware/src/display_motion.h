#pragma once
#include <math.h>
#include <stdint.h>

// Visual orientation follows gravity sooner than microphone session policy.
// A flat device preserves the last direction, including the exploration deck.
struct DisplayOrientation {
  static constexpr uint32_t ConfirmMs = 90;
  uint8_t rotation = 0, candidate = 0;
  bool known = false, pending = false;
  int reference = 0;
  uint32_t since = 0, confirmedAfterMs = 0;
  void update(float x,float y,float z,float gyro,int uprightSign,uint32_t now,bool locked=false) {
    if(reference != uprightSign){reference=uprightSign;known=false;pending=false;}
    if(locked){pending=false;return;}
    const float n=sqrtf(x*x+y*y+z*z);
    if(!reference || !(n>.75f && n<1.25f && gyro>=0 && gyro<160 && fabsf(z)/n<.5f)) {pending=false;return;}
    uint8_t next;
    if(fabsf(y)/n>.78f && fabsf(x)/n<.6f)next=y*reference<0?2:0;
    else if(fabsf(x)/n>.78f && fabsf(y)/n<.6f)next=x*reference>0?1:3;
    else {pending=false;return;}
    if(known && next==rotation){pending=false;return;}
    if(!pending || next!=candidate){pending=true;candidate=next;since=now;}
    if(uint32_t(now-since)>=ConfirmMs){rotation=next;known=true;pending=false;confirmedAfterMs=now-since;}
  }
};

// Pure time-based animation: retargeting uses the current angle instead of
// jumping back to an endpoint. No sleep, allocation, or display I/O here.
struct RotationTransition {
  static constexpr uint32_t DurationMs = 180;
  uint8_t target = 0;
  bool active = false;
  float from = 0, to = 0;
  uint32_t started = 0, duration = DurationMs;
  float angle(uint32_t now) const {
    if(!active)return to;
    const float t=fminf(1,float(uint32_t(now-started))/duration);
    const float ease=t*t*(3-2*t);
    return from+(to-from)*ease;
  }
  bool request(uint8_t next,uint32_t now) {
    next&=3;if(next==target)return false;
    from=angle(now);target=next;
    float delta=fmodf(float(next)*90-from,360);
    if(delta>180)delta-=360;if(delta< -180)delta+=360;
    to=from+delta;started=now;
    duration=uint32_t(fmaxf(120,DurationMs*fabsf(delta)/180));
    active=fabsf(to-from)>.01f;
    return true;
  }
  float step(uint32_t now) {
    const float value=angle(now);
    if(active && uint32_t(now-started)>=duration)active=false;
    return value;
  }
  static float fit(float angle,float width,float height) {
    return fit(angle,width,height,width,height);
  }
  static float fit(float angle,float width,float height,float dstW,float dstH) {
    const float radians=angle*.01745329252f;
    const float c=fabsf(cosf(radians)),s=fabsf(sinf(radians));
    return fminf(1,fminf(dstW/(width*c+height*s),dstH/(width*s+height*c)));
  }
};
