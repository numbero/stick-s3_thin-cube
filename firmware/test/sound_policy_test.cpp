#include "../src/sound_policy.h"
#include <assert.h>
#include <stdio.h>
int main(){
  {SoundPolicy p;p.request(SoundCue::Poke,100);assert(p.take(100,true)==SoundCue::None);assert(p.take(200,false)==SoundCue::None);}
  {SoundPolicy p;p.request(SoundCue::Wake,100);assert(p.take(100,false)==SoundCue::Wake);assert(p.mustStop(true));p.ended(130);assert(!p.active);assert(p.suppressListening(629));assert(!p.suppressListening(630));}
  {SoundPolicy p;p.request(SoundCue::Scan,100);p.muted=true;assert(p.take(100,false)==SoundCue::None);p.muted=false;assert(p.take(110,false)==SoundCue::None);}
  {SoundPolicy p;p.volume=0;p.request(SoundCue::Preview,100);assert(p.take(100,false)==SoundCue::None);p.volume=100;assert(p.speakerVolume()==191);}
  {SoundPolicy p;p.request(SoundCue::Poke,100);assert(p.take(451,false)==SoundCue::None);}
  {SoundPolicy p;p.request(SoundCue::Poke,100);p.take(100,false);p.muted=true;assert(p.mustStop(false));}
  {SoundPolicy p;p.request(SoundCue::Scan,100);p.take(100,false);p.ended(200);p.request(SoundCue::Discovery,210);assert(p.take(319,false)==SoundCue::None);assert(p.take(320,false)==SoundCue::Discovery);}
  {SoundPolicy p;p.request(SoundCue::Poke,0xfffffff0);assert(p.take(20,false)==SoundCue::Poke);p.ended(30);assert(p.suppressListening(529));assert(!p.suppressListening(530));}
  {SoundPolicy p;p.request(SoundCue::Poke,0xffffff00);assert(p.take(100,false)==SoundCue::None);}
  {SoundPolicy p;p.request(SoundCue::Poke,100);p.request(SoundCue::Discovery,110);assert(p.take(120,false)==SoundCue::Discovery);}
  {SoundPolicy p;p.request(SoundCue::Wake,101);assert(p.take(100,false)==SoundCue::Wake);} // Caller clock may precede an event within the same loop.
  puts("11 sound arbitration, mute, freshness, cooldown and wrap scenarios passed");
}
