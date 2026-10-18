#pragma once
#include <stdint.h>

// Diagnostic admission only. This class never creates key/transport events.
struct TapDiagnostic {
  enum class Reason { Accepted,Inactive,Button,Settling,Cooldown };
  uint32_t raw=0,accepted=0,blocked=0,unknown=0;
  uint32_t since=0,lastAccepted=0;
  bool context=false,button=false,haveAccepted=false;
  void update(uint32_t now,bool active,bool pressed){
    if(active!=context||pressed)since=now;
    context=active;button=pressed;
  }
  Reason event(uint32_t now){
    ++raw;
    Reason reason=Reason::Accepted;
    if(!context)reason=Reason::Inactive;
    else if(button)reason=Reason::Button;
    else if(uint32_t(now-since)<500)reason=Reason::Settling;
    else if(haveAccepted&&uint32_t(now-lastAccepted)<600)reason=Reason::Cooldown;
    if(reason==Reason::Accepted){++accepted;lastAccepted=now;haveAccepted=true;}
    else ++blocked;
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
