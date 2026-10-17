#pragma once
#include "clawd_words_policy.h"
#include <cmath>

namespace ClawdSound {
struct Gate {
  ClawdWords::Gate home;
  bool enabled=false,light=false,cue=false;
  bool captureWanted()const{return enabled&&home.allowed()&&!light&&!cue;}
};

// Display-only peak buckets: no PCM, beat recognition, replay or interaction.
// Time, rather than frame count, sets the scroll speed. Taking a peak keeps
// short sounds visible even when they fall between LCD refreshes.
class Trace {
 public:
  enum { WindowMs=3600,StepMs=180,Slots=WindowMs/StepMs+1,
         Left=71,Right=131,Height=18,Center=35 };
  struct Bar {int x,height;uint32_t age;};
  void sample(uint32_t now,float value){
    if(!received){received=true;bucketAt=now;}
    const uint32_t steps=petStateElapsed(now,bucketAt)/StepMs;
    if(steps){
      if(steps>=Slots){for(auto& p:peaks)p=0;head=0;}
      else for(uint32_t i=0;i<steps;i++){head=(head+1)%Slots;peaks[head]=0;}
      bucketAt+=steps*StepMs;
    }
    const auto peak=uint8_t(lroundf(std::max(0.f,std::min(1.f,value))*255));
    peaks[head]=std::max(peaks[head],peak);
  }
  bool ready()const{return received;}
  Bar bar(uint32_t now,unsigned index)const{
    const uint32_t age=petStateElapsed(now,bucketAt)+index*StepMs;
    return {Right-1-int(age*(Right-Left)/WindowMs),
      1+(int(peaks[(head+Slots-index)%Slots])*(Height-1)+127)/255,age};
  }
 private:
  uint8_t peaks[Slots]={},head=0;
  uint32_t bucketAt=0;
  bool received=false;
};

// Instantaneous envelope and reflex retain their existing behavior. Only the
// visual trace holds a short history of quantized loudness, never audio.
class Meter {
 public:
  static constexpr uint32_t WarmupMs=200,FreshMs=250,ReflexMs=420,ReflexGapMs=3500;
  void reset(){*this=Meter();}
  void sample(uint32_t now,float rms){
    if(!std::isfinite(rms)||rms<0||rms>1){reset();return;}
    if(!received||petStateElapsed(now,lastFrame)>FreshMs){reset();received=true;started=now;}
    lastFrame=now;
    if(petStateElapsed(now,started)<WarmupMs)return; // Ignore codec startup pop.
    const float input=std::max(0.f,std::min(1.f,(20*std::log10(std::max(.00001f,rms))+60)/36));
    trace.sample(now,input);
    envelope+=(input-envelope)*(input>envelope?.6f:.08f);
    if(input>.8f&&previous<.4f&&(!hasReflex||petStateElapsed(now,reflexAt)>=ReflexGapMs)){
      hasReflex=true;reflexAt=now;
    }
    previous=input;frames++;
  }
  float level(uint32_t now)const{return fresh(now)?envelope:0;}
  float attention(uint32_t now)const{return std::max(0.f,std::min(1.f,(level(now)-.08f)/.72f));}
  float flinch(uint32_t now)const{
    if(!fresh(now)||!hasReflex)return 0;
    const auto age=petStateElapsed(now,reflexAt);
    return age>=ReflexMs?0:std::sin(float(age)*3.14159265f/ReflexMs);
  }
  unsigned samples()const{return frames;}
  bool traceReady(uint32_t now)const{return fresh(now)&&trace.ready();}
  const Trace& history()const{return trace;}
 private:
  bool fresh(uint32_t now)const{return received&&petStateElapsed(now,lastFrame)<=FreshMs;}
  bool received=false,hasReflex=false;
  uint32_t started=0,lastFrame=0,reflexAt=0;
  unsigned frames=0;
  float envelope=0,previous=0;
  Trace trace;
};

inline uint16_t fade(uint16_t dim,uint16_t ink,unsigned amount){
  const unsigned r=(((dim>>11)&31)*(255-amount)+((ink>>11)&31)*amount)/255;
  const unsigned g=(((dim>>5)&63)*(255-amount)+((ink>>5)&63)*amount)/255;
  const unsigned b=((dim&31)*(255-amount)+(ink&31)*amount)/255;
  return uint16_t((r<<11)|(g<<5)|b);
}
template<class Canvas>void draw(Canvas& canvas,const Meter& meter,uint32_t now,bool active,
                               uint16_t ink,uint16_t glow,uint16_t dim){
  if(!active||!meter.traceReady(now)){
    for(int i=0;i<20;i++)canvas.fillRect(72+i*3,Trace::Center,2,1,dim);
    return;
  }
  for(unsigned i=0;i<Trace::Slots;i++){
    const auto bar=meter.history().bar(now,i);
    if(bar.age>=Trace::WindowMs)continue;
    const int left=std::max(int(Trace::Left),bar.x),right=std::min(int(Trace::Right),bar.x+2);
    if(left>=right)continue;
    const auto color=bar.age<Trace::StepMs?glow:
      fade(dim,ink,255-bar.age*255/Trace::WindowMs);
    canvas.fillRect(left,Trace::Center-bar.height/2,right-left,bar.height,color);
  }
}
}
