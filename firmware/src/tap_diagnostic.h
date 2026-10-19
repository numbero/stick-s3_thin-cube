#pragma once
#include <stdint.h>

// Diagnostic admission only. This class never creates key/transport events.
struct TapDiagnostic {
  enum class Reason { Accepted,Inactive,Button,Settling,Cooldown };
  uint32_t raw=0,accepted=0,blocked=0,unknown=0;
  uint32_t since=0,lastAccepted=0;
  uint32_t lastButton=0,eventAt=0,eventSequence=0;
  uint32_t inactive=0,buttons=0,settling=0,cooldown=0;
  uint16_t settleMs=500,buttonGuardMs=500,cooldownMs=600;
  Reason lastReason=Reason::Inactive;
  bool haveButton=false;
  bool context=false,button=false,haveAccepted=false;
  void update(uint32_t now,bool active,bool pressed){
    if(active!=context)since=now;
    if(pressed){lastButton=now;haveButton=true;}
    context=active;button=pressed;
  }
  Reason event(uint32_t now){
    ++raw;
    Reason reason=Reason::Accepted;
    if(!context)reason=Reason::Inactive;
    else if(button)reason=Reason::Button;
    else if((haveButton&&uint32_t(now-lastButton)<buttonGuardMs)||uint32_t(now-since)<settleMs)reason=Reason::Settling;
    else if(haveAccepted&&uint32_t(now-lastAccepted)<cooldownMs)reason=Reason::Cooldown;
    if(reason==Reason::Accepted){++accepted;lastAccepted=now;haveAccepted=true;}
    else ++blocked;
    if(reason==Reason::Inactive)++inactive;
    if(reason==Reason::Button)++buttons;
    if(reason==Reason::Settling)++settling;
    if(reason==Reason::Cooldown)++cooldown;
    eventAt=now;++eventSequence;lastReason=reason;
    return reason;
  }
  static const char* name(Reason reason){
    switch(reason){
      case Reason::Accepted:return "accepted";
      case Reason::Inactive:return "inactive";
      case Reason::Button:return "button";
      case Reason::Settling:return "settling";
      case Reason::Cooldown:return "cooldown";
    }
    return "unknown";
  }
};
