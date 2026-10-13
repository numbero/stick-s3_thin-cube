#include "sound_effects.h"
#include "audio_input.h"
#include "power_save.h"
#include <M5Unified.h>
#include <math.h>

namespace {
SoundPolicy policy;
uint32_t played=0;
constexpr unsigned Rate=16000, Capacity=4800;
int16_t pcm[Capacity]; // Remains immutable until Speaker.end joins playback.
struct Note { float from, to; unsigned ms; };
unsigned synth(SoundCue cue) {
  Note notes[4]{}; unsigned count=0;
  switch(cue){
    case SoundCue::Wake: case SoundCue::Preview: notes[0]={660,880,70};notes[1]={990,1320,110};count=2;break;
    case SoundCue::Poke: notes[0]={1100,1600,55};notes[1]={880,660,65};count=2;break;
    case SoundCue::Scan: notes[0]={420,1250,160};count=1;break;
    case SoundCue::Discovery: notes[0]={784,784,65};notes[1]={988,988,65};notes[2]={1568,1760,100};count=3;break;
    case SoundCue::Dizzy: notes[0]={900,500,70};notes[1]={650,350,90};count=2;break;
    case SoundCue::Sleep: notes[0]={660,440,70};notes[1]={440,220,90};count=2;break;
    case SoundCue::Curious: notes[0]={740,1080,90};count=1;break;
    default:return 0;
  }
  unsigned used=0;float phase=0;
  for(unsigned n=0;n<count;n++){
    const unsigned length=notes[n].ms*Rate/1000;
    for(unsigned i=0;i<length && used<Capacity;i++){
      const float t=float(i)/length;
      phase+=6.2831853f*(notes[n].from+(notes[n].to-notes[n].from)*t)/Rate;
      if(phase>6.2831853f)phase-=6.2831853f;
      const float edge=fminf(1,fminf(float(i)/160,float(length-1-i)/240));
      pcm[used++]=int16_t(14000*edge*(.85f*sinf(phase)+.15f*sinf(phase*2)));
    }
    for(unsigned i=0;i<160&&used<Capacity;i++)pcm[used++]=0;
  }
  return used;
}
}
void soundRequest(SoundCue cue){policy.request(cue,millis());}
void soundConfigure(uint8_t volume,bool muted){policy.volume=volume>100?100:volume;policy.muted=muted;if(policy.active)M5.Speaker.setVolume(policy.speakerVolume());}
void soundCancel(){
  policy.pending=SoundCue::None;
  if(policy.active){M5.Speaker.end();policy.ended(millis());Serial.println("[sound] stopped");}
}
void soundTick(uint32_t ms,bool reserved){
  if(policy.mustStop(reserved)){soundCancel();ms=millis();}
  if(policy.active && (!M5.Speaker.isPlaying() || uint32_t(ms-policy.startedAt)>500)){
    M5.Speaker.end();policy.ended(ms);Serial.println("[sound] done");
  }
  const auto cue=policy.take(ms,reserved);
  if(cue==SoundCue::None)return;
  powerSavePerformance();
  audioTick(false); // Stop/join the Mic callback before touching the shared codec.
  const unsigned length=synth(cue);
  M5.Speaker.setVolume(policy.speakerVolume());
  if(length && M5.Speaker.begin() && M5.Speaker.playRaw(pcm,length,Rate,false,1,0,true)){
    played++;Serial.printf("[sound] cue %u volume %u count %u\n",unsigned(cue),policy.volume,unsigned(played));
  }else{M5.Speaker.end();policy.ended(ms);Serial.println("[sound] playback failed");}
}
bool soundPlaying(){return policy.active;}
bool soundSuppressListening(uint32_t ms){return policy.suppressListening(ms);}
uint8_t soundVolume(){return policy.volume;}
bool soundMuted(){return policy.muted;}
uint32_t soundCount(){return played;}
