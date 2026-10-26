#pragma once
#include <stdint.h>

struct IdleWorkProfile {
  static constexpr unsigned FastMHz=240, IdleMHz=160, HomeMHz=160;
  static constexpr uint32_t IdleMotionMs=40, IdleDrawMs=200, IdlePublishMs=200;
};

enum class PowerWorkMode { Performance, Home, Standby };
inline PowerWorkMode powerWorkMode(bool standby,bool home){
  return standby?PowerWorkMode::Standby:home?PowerWorkMode::Home:PowerWorkMode::Performance;
}
inline unsigned powerWorkMHz(PowerWorkMode mode){
  return mode==PowerWorkMode::Performance?IdleWorkProfile::FastMHz:
    mode==PowerWorkMode::Home?IdleWorkProfile::HomeMHz:IdleWorkProfile::IdleMHz;
}

// Main-loop owned. The GAP callback supplies observations, never mutates policy.
// A rejected/slow update disables idle requests for this connection; audio must
// wait for a confirmed fast interval after an idle interval has been accepted.
struct BLEPowerPolicy {
  static constexpr uint16_t FastMin=12,FastMax=18,IdleMin=48,IdleMax=72;
  bool idle=false,pending=false,requestedIdle=false,idleDisabled=false,wasSlowed=false;
  uint16_t actual=0;
  uint32_t requestedAt=0,retryAt=0;
  void reset(){*this=BLEPowerPolicy{};}
  void connected(uint16_t interval){
    reset();
    // Bluetooth LE interval is 6..3200 units of 1.25 ms. Never infer readiness
    // from an unknown value, a prior link, or an unconfirmed fast request.
    if(interval>=6&&interval<=3200)observed(interval,true);
  }
  bool requestDue(uint32_t now,bool wantIdle){
    idle=wantIdle&&!idleDisabled;
    if(pending){
      if(uint32_t(now-requestedAt)<2000)return false;
      pending=false;idleDisabled=true;idle=false;retryAt=now;
      if(requestedIdle)return true; // restore fast after an uncertain idle request
    }
    if(actual && (idle ? actual>=IdleMin&&actual<=IdleMax : actual<=24))return false;
    if(retryAt&&uint32_t(now-retryAt)<1000)return false;
    return true;
  }
  void requested(uint32_t now,bool accepted){
    requestedIdle=idle;requestedAt=now;
    pending=accepted;
    if(!accepted){idleDisabled=true;idle=false;retryAt=now;}
  }
  void observed(uint16_t interval,bool success){
    if(success){actual=interval;wasSlowed=wasSlowed||interval>24;}
    if(pending&&(!success||(requestedIdle&&(interval<IdleMin||interval>IdleMax))))idleDisabled=true;
    pending=false;retryAt=0;
  }
  bool audioReady()const{
    if(idle||(pending&&requestedIdle))return false;
    return actual>0&&actual<=24&&!(pending&&requestedIdle);
  }
};

struct DeepSleepPolicy {
  enum class Decision { Wait, Commit, Cancel };
  static constexpr uint32_t GraceMs=350,AckTimeoutMs=2000,RetryMs=30000;
  bool pending=false,needAck=false,acknowledged=false,cooling=false;
  uint32_t id=0,since=0,cancelledAt=0;
  void begin(uint32_t now,uint32_t sequence,bool connected){
    pending=true;id=sequence?sequence:1;since=now;needAck=connected;acknowledged=false;cooling=false;
  }
  void cancel(){pending=false;needAck=acknowledged=cooling=false;}
  void retryLater(uint32_t now){cancel();cooling=true;cancelledAt=now;}
  bool ready(uint32_t now)const{return !cooling||uint32_t(now-cancelledAt)>=RetryMs;}
  bool ack(uint32_t sequence){if(!pending||sequence!=id)return false;acknowledged=true;return true;}
  Decision update(uint32_t now,bool safe,bool connected){
    if(!pending)return Decision::Wait;
    if(!safe||(needAck&&!connected)){retryLater(now);return Decision::Cancel;}
    if(!needAck&&connected){needAck=true;acknowledged=false;}
    const uint32_t age=uint32_t(now-since);
    if(age>=AckTimeoutMs&&needAck&&!acknowledged){retryLater(now);return Decision::Cancel;}
    if(age>=GraceMs&&(!needAck||acknowledged))return Decision::Commit;
    return Decision::Wait;
  }
};
