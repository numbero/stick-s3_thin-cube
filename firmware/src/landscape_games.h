#pragma once
#include "exploration.h"
#include "star_catch.h"

struct LandscapeGames {
  enum class Scene { None, Explore, Catch };
  struct Events {Exploration::Event explore=Exploration::Event::None;StarCatch::Events catchStars;};
  Exploration exploration;
  StarCatch catchStars;
  Scene playing=Scene::None;
  // StickS3's front A sits below the portrait LCD. Rotation 1 puts A on the
  // right; rotation 3 puts A on the left (native panel offset_rotation = 0).
  static Scene scene(uint8_t rotation){return rotation==1?Scene::Explore:rotation==3?Scene::Catch:Scene::None;}
  bool active()const{return exploration.active||catchStars.active;}
  void cancel(){exploration.cancel();catchStars.cancel();playing=Scene::None;}
  bool begin(uint8_t rotation,float degrees,uint32_t now){
    if(active())return false;
    const auto selected=scene(rotation);
    const bool began=selected==Scene::Explore?exploration.begin(degrees,now):selected==Scene::Catch?catchStars.begin(degrees,now):false;
    if(began)playing=selected;
    return began;
  }
  Events update(uint8_t rotation,float degrees,bool awake,uint32_t now){
    Events events;
    if(!awake||(active()&&scene(rotation)!=playing)){cancel();return events;}
    if(playing==Scene::Explore)events.explore=exploration.update(degrees,awake,now);
    else if(playing==Scene::Catch)events.catchStars=catchStars.update(degrees,awake,now);
    if(!active())playing=Scene::None;
    return events;
  }
};
