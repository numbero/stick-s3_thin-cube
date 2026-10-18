#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>

struct FlickCapturePoint {
  uint32_t ms;
  float ax,ay,az,gx,gy,gz;
  uint8_t flags,stage,event,reason;
};

// Fixed storage, no overwrite: a completed trial stays intact until a new one
// is explicitly started. The caller owns storage and synchronization.
struct FlickCaptureBuffer {
  enum class Stop { Idle,Recording,Requested,Timeout,Full };
  static constexpr uint32_t LimitMs=60000;
  FlickCapturePoint* points=nullptr;
  size_t capacity=0,count=0;
  uint32_t started=0,maxGap=0;
  Stop stop=Stop::Idle;
  FlickCaptureBuffer(FlickCapturePoint* p=nullptr,size_t n=0):points(p),capacity(n){}
  bool active()const{return stop==Stop::Recording;}
  bool start(uint32_t now){
    if(active()||!points||!capacity)return false;
    count=0;started=now;maxGap=0;stop=Stop::Recording;return true;
  }
  void finish(){if(active())stop=Stop::Requested;}
  void tick(uint32_t now){if(active()&&uint32_t(now-started)>=LimitMs)stop=Stop::Timeout;}
  bool append(const FlickCapturePoint& p){
    tick(p.ms);if(!active())return false;
    if(!isfinite(p.ax)||!isfinite(p.ay)||!isfinite(p.az)||!isfinite(p.gx)||!isfinite(p.gy)||!isfinite(p.gz))return false;
    if(count){const uint32_t gap=p.ms-points[count-1].ms;if(gap>maxGap)maxGap=gap;}
    points[count++]=p;if(count==capacity)stop=Stop::Full;return true;
  }
  static const char* name(Stop s){
    switch(s){case Stop::Idle:return "idle";case Stop::Recording:return "recording";case Stop::Requested:return "stopped";case Stop::Timeout:return "timeout";case Stop::Full:return "full";}return "unknown";
  }
};
