#pragma once
#include "ui_timing.h"
#include <algorithm>

// Separate from shutdown deadlines: protected work/power uncertainty grants
// a fresh dimming window without changing the existing 5/10-minute policy.
struct IdleDimPolicy {
  static constexpr uint32_t DelayMs=10000,FadeMs=1500;
  uint32_t blockedAt=0;
  bool dimmed=false;
  uint8_t update(uint32_t now,uint32_t activityAge,uint8_t saved,bool batteryOnly,bool eligible){
    if(!batteryOnly||!eligible){blockedAt=now;dimmed=false;return saved;}
    const uint32_t age=std::min(activityAge,petStateElapsed(now,blockedAt));
    if(age<=DelayMs){dimmed=false;return saved;}
    const uint8_t floor=std::min<unsigned>(saved,std::max<unsigned>(12,unsigned(saved)*35/100));
    const uint32_t fade=std::min(uint32_t(FadeMs),age-DelayMs);
    const uint8_t value=saved-(unsigned(saved-floor)*fade/FadeMs);
    dimmed=value<saved;
    return value;
  }
};

struct SleepFrameState {
  int battery;
  uint32_t flags;
  uint8_t rotation;
  SleepFrameState(int level=-1,uint32_t status=0,uint8_t direction=0):battery(level),flags(status),rotation(direction){}
  bool operator==(const SleepFrameState& other)const{
    return battery==other.battery&&flags==other.flags&&rotation==other.rotation;
  }
};
struct SleepFrameGate {
  static constexpr uint32_t SettleMs=1500;
  bool cached=false;
  SleepFrameState previous;
  bool draw(uint32_t now,uint32_t enteredAt,bool eligible,const SleepFrameState& state){
    if(!eligible||petStateElapsed(now,enteredAt)<SettleMs){cached=false;return true;}
    if(cached&&state==previous)return false;
    previous=state;cached=true;return true;
  }
};
