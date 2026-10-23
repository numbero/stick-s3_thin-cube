#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// A boot chord is latched at the first sample. A+B never becomes either
// maintenance or factory reset when one of the two buttons is released first.
struct MaintenanceBootGesture {
  static constexpr uint32_t HoldMs=2000;
  static constexpr uint32_t MaxHoldMs=10000;
  bool candidate=false,ready=false,finished=false;
  uint32_t since=0;
  enum class Action { None, Enter, Cancel };
  MaintenanceBootGesture(bool a,bool b,uint32_t now):candidate(a&&!b),since(now){}
  Action update(uint32_t now,bool a,bool b){
    if(finished)return Action::None;
    if(!candidate||b){finished=true;return Action::Cancel;}
    if(uint32_t(now-since)>=HoldMs)ready=true;
    if(a){
      if(uint32_t(now-since)>=MaxHoldMs){finished=true;return Action::Cancel;}
      return Action::None;
    }
    finished=true;return ready?Action::Enter:Action::Cancel;
  }
};

struct MaintenanceIdlePolicy {
  static constexpr uint32_t TimeoutMs=300000;
  static bool expired(uint32_t now,uint32_t lastActivity,bool protectedOperation){
    // An HTTP worker can publish activity a few milliseconds newer than the
    // main loop's captured timestamp. Treat that as fresh activity.
    return !protectedOperation&&int32_t(now-lastActivity)>=int32_t(TimeoutMs);
  }
};

struct MaintenanceSessionAttempts {
  static constexpr unsigned MaxFailures=5;
  static constexpr uint32_t CooldownMs=30000;
  unsigned failures=0;
  bool cooling=false;
  uint32_t since=0;
  bool allowed(uint32_t now){
    if(cooling&&uint32_t(now-since)>=CooldownMs){cooling=false;failures=0;}
    return !cooling;
  }
  void rejected(uint32_t now){if(++failures>=MaxFailures){cooling=true;since=now;}}
  void accepted(){failures=0;cooling=false;}
};

struct MaintenanceRequestRecord {
  uint32_t magic=0x54434D41;
  char request[37]{};
  bool valid() const {
    if(magic!=0x54434D41||!request[0]||request[36])return false;
    for(const char* p=request;*p;++p)if(*p<33||*p>126)return false;
    return true;
  }
};

// Consuming the durable intent precedes every AP start. If clearing it fails,
// the caller must continue the normal boot rather than create a restart loop.
enum class MaintenanceConsume { None, Requested, Invalid, StorageFailed };
template<class Store> MaintenanceConsume maintenanceConsumeRequest(Store& store){
  if(!store.exists())return MaintenanceConsume::None;
  MaintenanceRequestRecord request{};
  const bool valid=store.read(request)&&request.valid();
  if(!store.clear())return MaintenanceConsume::StorageFailed;
  return valid?MaintenanceConsume::Requested:MaintenanceConsume::Invalid;
}
