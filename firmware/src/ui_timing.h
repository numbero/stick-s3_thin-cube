#pragma once
#include <stdint.h>

inline uint32_t petStateElapsed(uint32_t now,uint32_t enteredAt){
  // UI actions can sample millis() after loop() sampled `now`. Treat that
  // small backwards step as zero, while retaining normal millis wraparound.
  // All UI deadlines are bounded to seconds, well below the signed half-range.
  const int32_t elapsed=int32_t(now-enteredAt);
  return elapsed>0?uint32_t(elapsed):0;
}
inline bool petIdleSleepDue(uint32_t now,uint32_t lastInteraction,float stillSeconds,
                            bool streaming,bool inverted,bool pairing){
  return !streaming&&!inverted&&!pairing&&stillSeconds>30.f&&petStateElapsed(now,lastInteraction)>30000;
}
