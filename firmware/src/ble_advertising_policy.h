#pragma once
#include <stdint.h>

// Main-loop owner. GAP callbacks only publish start/stop completions. A
// successful connection stops advertising in the controller automatically.
struct BLEAdvertisingPolicy {
  static constexpr uint32_t FastWindowMs=60000,RetryMs=1000;
  static constexpr uint16_t FastMin=0x30,FastMax=0x60,SlowMin=0x320,SlowMax=0x640;
  enum class Operation { None, StartFast, StartSlow, Stop };
  enum class State { Stopped, Starting, Running, Stopping };
  State state=State::Stopped;
  bool fast=true,desiredFast=true,initialized=false,retrying=false;
  uint32_t fastAt=0,retryAt=0,revision=0,pendingRevision=0;

  void activity(uint32_t now){fastAt=now;}
  void observe(uint32_t now,uint32_t linkRevision){
    if(!initialized||revision!=linkRevision){
      initialized=true;revision=linkRevision;activity(now);retrying=false;
      if(state==State::Running)state=State::Stopped;
    }
  }
  Operation next(uint32_t now,bool ready,bool linked,bool priority){
    if(priority)activity(now); // Pairing/setup retain a full fast window on exit.
    desiredFast=priority||uint32_t(now-fastAt)<FastWindowMs;
    if(!ready||linked||state==State::Starting||state==State::Stopping||
       (retrying&&uint32_t(now-retryAt)<RetryMs))return Operation::None;
    if(state==State::Running)return fast==desiredFast?Operation::None:Operation::Stop;
    return desiredFast?Operation::StartFast:Operation::StartSlow;
  }
  void issued(Operation op,uint32_t now,bool accepted){
    retryAt=now;retrying=!accepted;
    if(!accepted)return;
    pendingRevision=revision;
    if(op==Operation::Stop)state=State::Stopping;
    else {fast=op==Operation::StartFast;state=State::Starting;}
  }
  void completed(bool start,bool success,uint32_t now){
    if(state!=(start?State::Starting:State::Stopping))return;
    const bool obsolete=pendingRevision!=revision;
    state=start?(success&&!obsolete?State::Running:State::Stopped):
      (success||obsolete?State::Stopped:State::Running);
    retryAt=now;retrying=!success&&!obsolete;
  }
};
