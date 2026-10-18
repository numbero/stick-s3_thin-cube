#pragma once
#include <stdint.h>

// Discrete, acknowledged events. They never acquire a held-key state.
struct GestureEvent {uint32_t seq,at;};
struct GestureEvents {
  static constexpr uint32_t LifetimeMs=750;
  GestureEvent entries[4]{};
  uint32_t sequence=0;unsigned read=0,count=0;
  void clear(){read=count=0;}
  void expire(uint32_t now){while(count&&uint32_t(now-entries[read].at)>LifetimeMs){read=(read+1)%4;--count;}}
  const GestureEvent* front(uint32_t now){expire(now);return count?&entries[read]:nullptr;}
  bool push(uint32_t now){expire(now);if(count==4)return false;if(!++sequence)++sequence;entries[(read+count)%4]={sequence,now};++count;return true;}
  void acknowledge(uint32_t seq){if(count&&entries[read].seq==seq){read=(read+1)%4;--count;}}
};
