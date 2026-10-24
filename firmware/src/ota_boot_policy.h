#pragma once
#include <stdint.h>

// Observe actual local health continuously. A missed main-loop observation is
// not evidence of stability, even if the uptime clock advanced meanwhile.
class OtaBootPolicy {
 public:
  static constexpr uint32_t StableMs=15000,MaxLoopGapMs=250;
  static constexpr uint32_t RollbackMs=90000,CompleteMs=5000;
  void reset(){observing=false;elapsed=0;}
  bool update(uint32_t now,bool healthy){
    if(!healthy){reset();return false;}
    if(!observing||uint32_t(now-last)>MaxLoopGapMs){observing=true;since=now;}
    last=now;elapsed=now-since;
    return elapsed>=StableMs;
  }
  uint32_t stableMs()const{return elapsed<StableMs?elapsed:StableMs;}
 private:
  bool observing=false;
  uint32_t since=0,last=0,elapsed=0;
};
