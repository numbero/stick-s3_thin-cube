#pragma once
#include "ui_timing.h"
#include <string.h>

// Protocol housekeeping does not represent a new user action. In particular,
// acknowledging deep sleep must not restart the very idle clock being checked.
inline bool idleControlActivity(const char* op){return strcmp(op,"heartbeat")&&strcmp(op,"sleep_ack");}

// Main-loop policy: all deadlines count from the last real activity.
// Control heartbeats and character animations are deliberately not activity.
struct IdlePowerPolicy {
  static constexpr uint32_t SleepMs=5*60*1000, DeepSleepMs=10*60*1000, OffMs=20*60*1000;
  enum class Action { None, Sleep, PowerOff };
  uint32_t lastActivity=0;
  void activity(uint32_t now){lastActivity=now;}
  uint32_t elapsed(uint32_t now)const{return petStateElapsed(now,lastActivity);}
  static bool deepSleepDue(uint32_t idle){return idle>=DeepSleepMs&&idle<OffMs;}
  Action update(uint32_t now,bool blocked,bool standby,bool batteryOnly){
    // Starting a task, or losing reliable motion input, grants a fresh window
    // when the task finishes. Long calls must never cause immediate shutdown.
    if(blocked){activity(now);return Action::None;}
    const auto idle=elapsed(now);
    if(standby&&batteryOnly&&idle>=OffMs)return Action::PowerOff;
    if(!standby&&idle>=SleepMs)return Action::Sleep;
    return Action::None;
  }
};

// Failed/stale reads never imply battery power. Both PM1 source bits and the
// actual USB input voltage must agree before automatic power-off is allowed.
inline bool idleBatteryOnly(bool valid,uint8_t source,uint16_t vinMv){
  return valid&&(source&7)==4&&vinMv<1000;
}

// Charger termination can toggle CHG_STAT at full battery while USB remains
// connected. Classify the actual input separately from the charging indicator.
inline int idlePowerSource(bool valid,uint8_t source,uint16_t vinMv){
  if(!valid)return -1;
  if(vinMv>=4000)return 1;
  return idleBatteryOnly(valid,source,vinMv)?0:-1;
}
