#pragma once
#include <stdint.h>

struct PairingPolicy {
  static constexpr uint32_t WindowMs=120000;
  bool visible=false,timed=false,attempted=false;
  uint32_t opened=0,lastAttempt=0;
  unsigned attempts=0;
  void offer(uint32_t now,bool startTimer) {
    visible=true;timed=startTimer;opened=now;attempts=0;attempted=false;
  }
  bool active(uint32_t now) const {
    // A same-loop UI action or HTTP request may open/refresh the window after
    // the caller sampled now. A small backwards step is still inside the
    // window; signed subtraction also preserves normal millis rollover.
    return visible&&timed&&int32_t(now-opened)<int32_t(WindowMs);
  }
  bool begin(uint32_t now) {
    if(!visible)return false;
    if(active(now))return false;
    offer(now,true);return true;
  }
  bool attempt(uint32_t now) {
    if(!active(now)||attempts>=10||(attempted&&uint32_t(now-lastAttempt)<1000))return false;
    attempted=true;lastAttempt=now;attempts++;return true;
  }
  bool expire(uint32_t now,bool paired) {
    if(!visible||!timed||active(now))return false;
    offer(now,false);if(paired)visible=false;return true;
  }
  void close() { visible=false;timed=false;attempts=0; }
};
