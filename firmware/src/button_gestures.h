#pragma once
#include <stdint.h>

struct ButtonGestures {
  enum class Action { None, Mute, Switch, Pair };
  bool previousB=false, combo=false, comboReady=false, comboReleased=false, pairAllowed=false, blocked=false;
  bool suppressA=false, switchReady=false;
  uint32_t bSince=0, bothSince=0;
  Action update(uint32_t now,bool a,bool b,bool bClicked,bool enabled,bool canPair) {
    suppressA=b||combo||blocked||previousB;
    if(!enabled){blocked=a||b;combo=false;switchReady=false;previousB=b;return Action::None;}
    if(blocked){blocked=a||b;previousB=b;return Action::None;}
    if(b&&!previousB){bSince=now;switchReady=false;}
    if(a&&b&&!combo){combo=true;bothSince=now;pairAllowed=canPair;comboReady=false;comboReleased=false;switchReady=false;}
    if(combo){
      if(!comboReleased&&uint32_t(now-bothSince)>=3000)comboReady=true;
      // Once either button releases, the simultaneous hold cannot grow further.
      if(!a||!b)comboReleased=true;
      pairAllowed=pairAllowed&&canPair;
      if(!a&&!b){const bool fire=comboReady&&pairAllowed;combo=false;comboReady=false;previousB=false;return fire?Action::Pair:Action::None;}
      previousB=b;return Action::None;
    }
    if(b&&uint32_t(now-bSince)>=3000)switchReady=true;
    const bool released=previousB&&!b;previousB=b;
    if(released){const bool fire=switchReady||uint32_t(now-bSince)>=3000;switchReady=false;return fire?Action::Switch:(bClicked?Action::Mute:Action::None);}
    return Action::None;
  }
};
