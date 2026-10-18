#pragma once
#include <stdint.h>
#include <math.h>

// Two repeated angular impulses followed by a stationary, restored grip.
// Work in vector space: a preparatory movement must not lock an axis.
class FlickDetector {
 public:
  enum class Stage { Inactive,Arming,Ready,FirstBurst,WaitSecond,SecondBurst,Cooldown };
  enum class Event { None,Started,FirstComplete,Recognized,Rejected };
  enum class Reason { None,Context,Button,Sensor,Gap,Timeout,Return,Direction,Weak,ExtraTurn };
  struct Result {Event event;Reason reason;Result(Event e=Event::None,Reason r=Reason::None):event(e),reason(r){}};
  Stage stage=Stage::Inactive;
  uint32_t starts=0,hits=0,rejected=0,duration=0;
  unsigned axis=0,strokes=0;
  float peakFirst=0,peakSecond=0,peakReverse=0,travel=0,peakAccelDev=0;
  static constexpr float StartRate=300,PeakRate=500,ReverseRate=150,QuietRate=45,DirectionCos=.65f;
  static constexpr float GripFilterMs=350;
  static constexpr uint32_t ArmMs=350,SettleMs=100,FirstBurstMs=220,MinSeparationMs=180;
  static constexpr uint32_t PairWindowMs=1600,CooldownMs=1200,MaxGapMs=80,MaxSeparationMs=900;

  Result cancel(Reason reason){
    const bool candidate=stage==Stage::FirstBurst||stage==Stage::WaitSecond||stage==Stage::SecondBurst;
    stage=Stage::Inactive;havePrevious_=quiet_=ramp_=false;
    if(candidate){++rejected;return {Event::Rejected,reason};}return {};
  }
  Result update(uint32_t now,float ax,float ay,float az,float gx,float gy,float gz,bool active,bool button){
    if(!active)return cancel(Reason::Context);
    if(button)return cancel(Reason::Button);
    if(!isfinite(ax)||!isfinite(ay)||!isfinite(az)||!isfinite(gx)||!isfinite(gy)||!isfinite(gz))return cancel(Reason::Sensor);
    const uint32_t dt=havePrevious_?uint32_t(now-previousAt_):0;
    previousAt_=now;havePrevious_=true;
    if(dt>MaxGapMs)return cancel(Reason::Gap);
    const float gyro[3]={gx,gy,gz};
    const float speed=sqrtf(gx*gx+gy*gy+gz*gz),mag=sqrtf(ax*ax+ay*ay+az*az);
    const bool still=speed<QuietRate&&fabsf(mag-1)<.2f;
    if(stage==Stage::Inactive){stage=Stage::Arming;quiet_=false;}
    if(stage==Stage::Arming||stage==Stage::Cooldown){
      if(!still)quiet_=false;else if(!quiet_){quiet_=true;quietAt_=now;}
      if(quiet_&&uint32_t(now-quietAt_)>=ArmMs&&
         (stage!=Stage::Cooldown||uint32_t(now-hitAt_)>=CooldownMs)){
        stage=Stage::Ready;base_[0]=ax/mag;base_[1]=ay/mag;base_[2]=az/mag;ramp_=false;
      }return {};
    }
    if(stage==Stage::Ready){
      if(speed>=StartRate){
        ++starts;strokes=0;duration=0;startedAt_=firstPeakAt_=now;stage=Stage::FirstBurst;
        peakFirst=peakSecond=peakReverse=travel=secondArea_=0;peakAccelDev=fabsf(mag-1);
        firstStrong_=secondStrong_=0;firstReverse_=secondReverse_=quiet_=false;
        trackFirst(gyro,speed,now);return {Event::Started};
      }
      if(speed>100){if(!ramp_){ramp_=true;rampAt_=now;}else if(uint32_t(now-rampAt_)>150){stage=Stage::Arming;quiet_=false;}}
      else ramp_=false;
      if(still){
        // A preparatory linear impulse can look like a new gravity direction
        // while its magnitude is still near 1g. Preserve the settled reference
        // through brief transients; track a genuinely new grip over time.
        const float weight=float(dt)/(GripFilterMs+float(dt));
        base_[0]+=(ax/mag-base_[0])*weight;
        base_[1]+=(ay/mag-base_[1])*weight;
        base_[2]+=(az/mag-base_[2])*weight;
      }return {};
    }
    duration=uint32_t(now-startedAt_);travel+=speed*(float(dt)/1000);
    if(fabsf(mag-1)>peakAccelDev)peakAccelDev=fabsf(mag-1);
    if(duration>PairWindowMs)return reject(Reason::Timeout);
    if(stage==Stage::FirstBurst){
      if(duration<FirstBurstMs){trackFirst(gyro,speed,now);return {};}
      if(peakFirst<PeakRate||firstStrong_<2||travel<15)return reject(Reason::Weak);
      for(unsigned i=0;i<3;++i)direction_[i]/=peakFirst;
      axis=0;for(unsigned i=1;i<3;++i)if(fabsf(direction_[i])>fabsf(direction_[axis]))axis=i;
      stage=Stage::WaitSecond;strokes=1;
      const float projection=gx*direction_[0]+gy*direction_[1]+gz*direction_[2];
      if(projection<-ReverseRate)firstReverse_=true;
      return {Event::FirstComplete};
    }
    const float projection=gx*direction_[0]+gy*direction_[1]+gz*direction_[2];
    if(-projection>peakReverse)peakReverse=-projection;
    const float similarity=speed>.01f?projection/speed:0;
    if(stage==Stage::WaitSecond){
      const uint32_t separation=uint32_t(now-firstPeakAt_);
      if(separation>MaxSeparationMs)return reject(Reason::Timeout);
      if(projection<-ReverseRate)firstReverse_=true;
      if(firstReverse_&&separation>=MinSeparationMs&&projection>=PeakRate&&similarity>=DirectionCos){
        stage=Stage::SecondBurst;secondPeakAt_=now;peakSecond=speed;secondStrong_=1;
        secondArea_=projection*float(dt)/1000;quiet_=false;
      }return {};
    }
    if(projection>0)secondArea_+=projection*float(dt)/1000;
    if(projection>=PeakRate&&similarity>=DirectionCos){
      if(secondReverse_&&uint32_t(now-secondPeakAt_)>=MinSeparationMs)return reject(Reason::ExtraTurn);
      ++secondStrong_;if(speed>peakSecond){peakSecond=speed;secondPeakAt_=now;}
    }
    if(projection<-ReverseRate)secondReverse_=true;
    if(!still){quiet_=false;return {};}
    if(!quiet_){quiet_=true;quietAt_=now;}
    if(uint32_t(now-quietAt_)<SettleMs)return {};
    const float baseMag=sqrtf(base_[0]*base_[0]+base_[1]*base_[1]+base_[2]*base_[2]);
    const float home=baseMag>.01f?(ax*base_[0]+ay*base_[1]+az*base_[2])/(mag*baseMag):-1;
    if(!secondReverse_||secondStrong_<2||secondArea_<8||home<.85f)return reject(Reason::Return);
    strokes=2;++hits;hitAt_=now;stage=Stage::Cooldown;quietAt_=now;quiet_=true;
    return {Event::Recognized};
  }
  static const char* name(Stage s){
    switch(s){case Stage::Inactive:return "inactive";case Stage::Arming:return "arming";case Stage::Ready:return "ready";case Stage::FirstBurst:return "first-burst";case Stage::WaitSecond:return "wait-second";case Stage::SecondBurst:return "second-burst";case Stage::Cooldown:return "cooldown";}return "unknown";
  }
  static const char* name(Reason r){
    switch(r){case Reason::None:return "none";case Reason::Context:return "context";case Reason::Button:return "button";case Reason::Sensor:return "sensor";case Reason::Gap:return "sample-gap";case Reason::Timeout:return "timeout";case Reason::Return:return "return";case Reason::Direction:return "direction";case Reason::Weak:return "weak";case Reason::ExtraTurn:return "extra-turn";}return "unknown";
  }
 private:
  bool havePrevious_=false,quiet_=false,ramp_=false,firstReverse_=false,secondReverse_=false;
  unsigned firstStrong_=0,secondStrong_=0;
  uint32_t previousAt_=0,quietAt_=0,startedAt_=0,hitAt_=0,rampAt_=0,firstPeakAt_=0,secondPeakAt_=0;
  float direction_[3]={},base_[3]={0,-1,0},secondArea_=0;
  void trackFirst(const float* gyro,float speed,uint32_t now){
    if(speed>=PeakRate)++firstStrong_;
    if(speed>peakFirst){peakFirst=speed;firstPeakAt_=now;firstReverse_=false;for(unsigned i=0;i<3;++i)direction_[i]=gyro[i];}
    else if(peakFirst>0&&(gyro[0]*direction_[0]+gyro[1]*direction_[1]+gyro[2]*direction_[2])/peakFirst<-ReverseRate)firstReverse_=true;
  }
  Result reject(Reason reason){++rejected;stage=Stage::Arming;quiet_=false;return {Event::Rejected,reason};}
};
