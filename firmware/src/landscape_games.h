#pragma once
#include "exploration.h"
#include "star_catch.h"
#include "star_maze.h"
#include "telekinetic_launch.h"
#include <stddef.h>

struct LandscapeGames {
  // Stable identifiers are also used in the versioned preference record.
  enum class Scene : uint8_t { None=0, Explore=1, Catch=2, Maze=3, Launch=4 };
  enum class Phase { Select, Playing, Result };
  struct Events {
    Exploration::Event explore=Exploration::Event::None;
    StarCatch::Events catchStars;
    bool mazeFinished=false;
    TelekineticLaunch::Events launch;
  };
  Exploration exploration;
  StarCatch catchStars;
  StarMaze maze;
  TelekineticLaunch launch;
  Scene left=Scene::Explore,right=Scene::Launch,selected=Scene::None;
  Scene playing=Scene::None,paused=Scene::None;
  Phase phase=Phase::Select;
  uint8_t rotation=0,playingRotation=0,mazeLevel=0,launchLevel=0;
  uint32_t clock=0,lastWall=0,prepareUntil=0;
  float baselineX=0,baselineY=0,offsetX=0,offsetY=0,shotAngle=-23,shotPower=.65f;
  bool shotLatched=false,preferencesDirty=false;
  LandscapeGames(){maze.preview(0);launch.preview(0);}
  // Rotation 1 puts front A on the right; rotation 3 puts A on the left.
  static bool landscape(uint8_t r){return r==1||r==3;}
  static bool valid(Scene s){return s>=Scene::Explore&&s<=Scene::Launch;}
  static const char* name(Scene s){
    switch(s){case Scene::Explore:return "EXPLORE";case Scene::Catch:return "CATCH";case Scene::Maze:return "MAZE";case Scene::Launch:return "LAUNCH";default:return "NONE";}
  }
  Scene remembered(uint8_t r)const{return r==3?left:r==1?right:Scene::None;}
  Scene scene(uint8_t r)const{return !landscape(r)?Scene::None:r==rotation?selected:remembered(r);}
  bool live()const{
    return playing==Scene::Explore?exploration.active:playing==Scene::Catch?catchStars.active:
      playing==Scene::Maze?maze.active:playing==Scene::Launch?launch.active:false;
  }
  bool active()const{return phase==Phase::Playing&&live();}
  bool hasRound(Scene s)const{return playing==s||paused==s;}
  bool resumable()const{return paused!=Scene::None&&paused==selected;}
  bool result()const{return phase==Phase::Result&&playing==selected;}
  bool ready(uint32_t now)const{return int32_t(now-prepareUntil)>=0;}
  void pause(){
    if(!active()){cancel();return;}
    paused=playing;playing=Scene::None;phase=Phase::Select;shotLatched=false;
  }
  void cancel(){
    exploration.cancel();catchStars.cancel();maze.preview(mazeLevel);launch.preview(launchLevel);
    playing=paused=Scene::None;phase=Phase::Select;shotLatched=false;
  }
  void observe(uint8_t r){
    if(rotation==r)return;
    if(active())pause();
    // Paused data never owns rotation, voice or power policy.
    playing=Scene::None;phase=Phase::Select;rotation=r;selected=remembered(r);shotLatched=false;
  }
  void next(uint8_t r){
    if(!landscape(r))return;
    observe(r);if(active()||phase==Phase::Result)pause();
    const int index=int(selected)-1,step=r==3?1:-1;
    selected=Scene(1+(index+step+4)%4);shotLatched=false;
  }
  static float unmap(float v,float dead=LandscapeTilt::DeadDegrees,float full=LandscapeTilt::FullDegrees){return v==0?0:copysignf(dead+fabsf(v)*(full-dead),v);}
  float pitchDead()const{return playing==Scene::Maze?LandscapeTilt::MazePitchDeadDegrees:LandscapeTilt::DeadDegrees;}
  float pitchFull()const{return playing==Scene::Maze?LandscapeTilt::MazePitchFullDegrees:LandscapeTilt::FullDegrees;}
  void rebase(float degrees,float vertical){
    if(playing==Scene::Explore){exploration.baseline=degrees-unmap(exploration.offset);exploration.baselineY=vertical-unmap(exploration.offsetY);}
    else if(playing==Scene::Catch)catchStars.baseline=degrees-unmap(catchStars.offset);
    else {baselineX=degrees-unmap(offsetX);baselineY=vertical-unmap(offsetY,pitchDead(),pitchFull());}
  }
  bool begin(uint8_t r,float degrees,float vertical,uint32_t now){
    if(active()||!landscape(r)||!isfinite(degrees))return false;
    observe(r);
    if(selected!=Scene::Catch&&!isfinite(vertical))return false;
    if(!valid(selected))return false;
    if(resumable()){
      playing=paused;paused=Scene::None;phase=Phase::Playing;playingRotation=r;
      rebase(degrees,vertical);lastWall=now;prepareUntil=now+300;shotLatched=false;
    }else{
      if(result()&&selected==Scene::Maze)mazeLevel=(mazeLevel+1)%3;
      if(result()&&selected==Scene::Launch&&launch.won)launchLevel=(launchLevel+1)%3;
      cancel();clock=0;baselineX=degrees;baselineY=vertical;offsetX=offsetY=0;
      bool began=true;
      switch(selected){
        case Scene::Explore:began=exploration.begin(degrees,vertical,clock);break;
        case Scene::Catch:began=catchStars.begin(degrees,clock);break;
        case Scene::Maze:maze.begin(mazeLevel,clock);break;
        case Scene::Launch:launch.begin(launchLevel,clock);break;
        default:began=false;break;
      }
      if(!began)return false;
      playing=selected;phase=Phase::Playing;playingRotation=r;lastWall=now;prepareUntil=now+300;
    }
    Scene& memory=r==3?left:right;
    if(memory!=selected){memory=selected;preferencesDirty=true;}
    return true;
  }
  void latchAim(){
    shotLatched=active()&&playing==Scene::Launch&&launch.phase==TelekineticLaunch::Phase::Aim;
    if(shotLatched){shotAngle=launch.angle;shotPower=launch.power;}
  }
  bool fire(uint32_t now){
    if(!active()||playing!=Scene::Launch||!ready(now)||!shotLatched)return false;
    launch.aim(shotAngle,shotPower);shotLatched=false;return launch.fire();
  }
  Events update(uint8_t r,float degrees,float vertical,bool awake,uint32_t now){
    Events events;
    if(!active()){lastWall=now;return events;}
    if(!awake||r!=playingRotation){pause();observe(r);lastWall=now;return events;}
    const uint32_t previous=lastWall;lastWall=now;
    if(!ready(now))return events;
    const uint32_t elapsed=int32_t(previous-prepareUntil)<0?now-prepareUntil:now-previous;
    clock+=elapsed;
    if(playing==Scene::Explore)events.explore=exploration.update(degrees,vertical,true,clock);
    else if(playing==Scene::Catch)events.catchStars=catchStars.update(degrees,true,clock);
    else{
      const float smooth=fminf(1,float(elapsed)/LandscapeTilt::SmoothMs);
      const float tx=isfinite(degrees)?LandscapeTilt::mapped(LandscapeTilt::delta(degrees,baselineX)):0;
      const float ty=isfinite(vertical)?LandscapeTilt::mapped(LandscapeTilt::delta(vertical,baselineY),pitchDead(),pitchFull()):0;
      offsetX+=(tx-offsetX)*smooth;offsetY+=(ty-offsetY)*smooth;
      if(playing==Scene::Maze)events.mazeFinished=maze.update(offsetX,offsetY,clock);
      else{
        if(!shotLatched)launch.aim(-23+offsetX*56,.65f-offsetY*.4f);
        events.launch=launch.update(clock);
      }
    }
    if(!live())phase=Phase::Result;
    return events;
  }
  // One NVS blob commits both independent sides atomically. Unknown records
  // use defaults; a bad identifier only resets that side.
  void loadPreferences(const uint8_t* bytes,size_t size){
    left=Scene::Explore;right=Scene::Launch;
    if(size==4&&bytes&&bytes[0]==0x47&&bytes[1]==1){
      if(valid(Scene(bytes[2])))left=Scene(bytes[2]);
      if(valid(Scene(bytes[3])))right=Scene(bytes[3]);
    }
    selected=remembered(rotation);preferencesDirty=false;
  }
  void preferences(uint8_t (&bytes)[4])const{bytes[0]=0x47;bytes[1]=1;bytes[2]=uint8_t(left);bytes[3]=uint8_t(right);}
};
