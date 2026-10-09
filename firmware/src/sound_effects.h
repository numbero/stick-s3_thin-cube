#pragma once
#include "sound_policy.h"
void soundRequest(SoundCue cue);
void soundConfigure(uint8_t volume, bool muted);
void soundCancel();
void soundTick(uint32_t ms, bool reserved);
bool soundPlaying();
bool soundSuppressListening(uint32_t ms);
uint8_t soundVolume();
bool soundMuted();
uint32_t soundCount();
