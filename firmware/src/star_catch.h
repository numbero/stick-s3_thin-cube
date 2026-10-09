#pragma once
#include "landscape_input.h"

// Two fixed slots bound both memory use and the number of falling objects.
struct StarCatch {
  static constexpr uint32_t RoundMs=20000;
  static constexpr float CenterX=157,SpanX=62,TrayWidth=24,CatchY=100,StartY=39;
  static constexpr unsigned Capacity=2;
  enum class Feedback { None, Caught, Missed };
  struct Events { unsigned caught=0,missed=0;bool finished=false; };
  struct Star {
    float x,y;bool active;
    Star(float sx=0,float sy=0,bool enabled=false):x(sx),y(sy),active(enabled){}
  };
  Star stars[Capacity];
  float baseline=0,offset=0,hitX=0;
  uint32_t started=0,lastTick=0,nextSpawn=0,feedbackAt=0;
  unsigned score=0,combo=0,bestCombo=0,spawnCount=0,rounds=0;
  bool active=false,finished=false;
  Feedback feedback=Feedback::None;

  bool begin(float degrees,uint32_t now) {
    if(active||!isfinite(degrees))return false;
    active=true;finished=false;baseline=degrees;offset=0;
    started=lastTick=now;nextSpawn=1700;spawnCount=score=combo=bestCombo=0;
    feedback=Feedback::None;rounds++;
    for(auto& star:stars)star=Star{};
    stars[0]={CenterX,StartY,true};
    return true;
  }
  void cancel(){active=finished=false;feedback=Feedback::None;for(auto& star:stars)star.active=false;}
  float trayX()const{return CenterX+offset*SpanX;}
  uint32_t leftMs(uint32_t now)const{
    if(finished)return 0;
    if(!active)return RoundMs;
    const uint32_t elapsed=now-started;return elapsed>=RoundMs?0:RoundMs-elapsed;
  }
  Events update(float degrees,bool awake,uint32_t now) {
    Events events;
    if(!awake){cancel();return events;}
    if(!active)return events;
    const uint32_t elapsed=now-started,stepMs=now-lastTick;lastTick=now;
    if(elapsed>=RoundMs){active=false;finished=true;for(auto& star:stars)star.active=false;events.finished=true;return events;}
    if(isfinite(degrees))offset+=(LandscapeTilt::mapped(LandscapeTilt::delta(degrees,baseline))-offset)*fminf(1,float(stepMs)/LandscapeTilt::SmoothMs);
    const float distance=float(stepMs)*.001f*(28+fminf(8,float(elapsed)*.0004f));
    // Resolve older stars first even after a slot is recycled or a delayed tick.
    const unsigned first=stars[1].active&&(!stars[0].active||stars[1].y>stars[0].y)?1:0;
    for(unsigned n=0;n<Capacity;n++){
      auto& star=stars[(first+n)%Capacity];if(!star.active)continue;
      star.y+=distance;
      if(star.y<CatchY)continue;
      star.active=false;feedbackAt=now;hitX=star.x;
      if(fabsf(star.x-trayX())<=TrayWidth/2+4){
        score++;combo++;if(combo>bestCombo)bestCombo=combo;
        events.caught++;feedback=Feedback::Caught;
      }else{combo=0;events.missed++;feedback=Feedback::Missed;}
    }
    if(elapsed>=nextSpawn){
      static constexpr float positions[]={112,202,146,214,101,174,130,191};
      for(auto& star:stars)if(!star.active){
        star={positions[(spawnCount+++(rounds-1))%8],StartY,true};break;
      }
      // A delayed loop never queues a burst of overdue spawns.
      const uint32_t interval=1700-elapsed/40;
      nextSpawn=elapsed+(interval<1100?1100:interval);
    }
    return events;
  }
};
