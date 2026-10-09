#pragma once
#include <stdint.h>
#include <string.h>

enum class TransportMode : uint8_t { WiFi, BLE };
enum class SwitchPhase : uint8_t { Stable, Pending, Trial, Committed, RolledBack };
inline const char* transportName(TransportMode m) { return m==TransportMode::BLE?"ble":"wifi"; }
inline const char* switchName(SwitchPhase p) {
  switch(p){case SwitchPhase::Pending:return "pending";case SwitchPhase::Trial:return "trial";
    case SwitchPhase::Committed:return "committed";case SwitchPhase::RolledBack:return "rolled_back";default:return "stable";}
}
// One NVS blob is the transaction boundary; never persist its fields separately.
struct TransportRecord {
  uint32_t magic=0x54434D31;
  TransportMode mode=TransportMode::WiFi, previous=TransportMode::WiFi;
  SwitchPhase phase=SwitchPhase::Stable;
  uint8_t reserved=0;
  char request[37]={}, source[9]={};
  bool valid() const {
    return magic==0x54434D31 && uint8_t(mode)<=1 && uint8_t(previous)<=1 && uint8_t(phase)<=4 &&
      request[36]==0 && source[8]==0 && (phase==SwitchPhase::Stable || request[0]);
  }
  bool busy() const {return phase==SwitchPhase::Pending||phase==SwitchPhase::Trial;}
  bool begin(TransportMode target,const char* id,const char* from) {
    if(!id||!id[0]||strlen(id)>36||!from||strlen(from)>8||busy()||mode==target)return false;
    previous=mode;mode=target;phase=SwitchPhase::Pending;
    strcpy(request,id);strcpy(source,from);return true;
  }
  TransportMode boot() {
    if(phase==SwitchPhase::Pending)phase=SwitchPhase::Trial;
    else if(phase==SwitchPhase::Trial)rollback();
    return mode;
  }
  bool commit(const char* id) {
    if(strcmp(id,request))return false;
    if(phase==SwitchPhase::Committed)return true;
    if(phase!=SwitchPhase::Trial)return false;
    phase=SwitchPhase::Committed;return true;
  }
  void rollback(){mode=previous;phase=SwitchPhase::RolledBack;}
  bool timedOut(uint32_t now,uint32_t started) const {return phase==SwitchPhase::Trial&&uint32_t(now-started)>=60000;}
};
