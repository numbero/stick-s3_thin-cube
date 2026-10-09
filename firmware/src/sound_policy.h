#pragma once
#include <stdint.h>

enum class SoundCue : uint8_t { None, Wake, Poke, Scan, Discovery, Dizzy, Sleep, Curious, Preview };

// A single fresh cue, never a backlog of chirps after a call or OTA ends.
struct SoundPolicy {
  SoundCue pending = SoundCue::None;
  uint32_t requestedAt = 0, endedAt = 0, startedAt = 0;
  bool active = false, hasEnded = false;
  uint8_t volume = 35;
  bool muted = false;
  bool audible() const { return !muted && volume > 0; }
  void request(SoundCue cue, uint32_t now) { pending = cue; requestedAt = now; }
  bool mustStop(bool reserved) const { return active && (reserved || !audible()); }
  void ended(uint32_t now) { active = false; hasEnded = true; endedAt = now; }
  bool suppressListening(uint32_t now) const {
    return active || (hasEnded && uint32_t(now-endedAt) < 500);
  }
  SoundCue take(uint32_t now, bool reserved) {
    if(reserved || !audible() || int32_t(now-requestedAt)>350) { pending=SoundCue::None; return SoundCue::None; }
    if(active || (hasEnded && uint32_t(now-endedAt)<120)) return SoundCue::None;
    const auto result=pending; pending=SoundCue::None;
    if(result!=SoundCue::None){active=true;startedAt=now;}
    return result;
  }
  uint8_t speakerVolume() const { return unsigned(volume)*191/100; }
};
