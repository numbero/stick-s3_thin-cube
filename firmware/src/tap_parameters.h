#pragma once
#include <stdint.h>

// Only fields actually implemented by the pinned Bosch Legacy API. The SDK's
// tap_shock_dur/min_const_thres members have no getter/setter implementation.
struct TapParameters {
  bool enabled=true,single=false,doubleTap=true,triple=false;
  bool filtered=true,wait=true,manual=false;
  bool software=true;
  uint16_t axis=2,sensitivity=9,windowMs=400,quietMs=400;
  uint16_t settleMs=500,buttonGuardMs=500,cooldownMs=600;
  uint16_t softThresholdMg=550,softReboundMs=180,softWindowMs=450;
  uint16_t softGyroDps=200,softMotionGuardMs=300,softBaselineMs=200;
  uint16_t softGyroFilterMs=100;
  uint16_t softRatioPercent=65;
  uint8_t mask()const{return unsigned(single)|(unsigned(doubleTap)<<1)|(unsigned(triple)<<2);}
  bool valid()const{
    return (!enabled||mask())&&axis<=2&&sensitivity<=15&&
      windowMs>=250&&windowMs<=1000&&windowMs%5==0&&
      quietMs>=250&&quietMs<=500&&quietMs%5==0&&
      settleMs<=5000&&buttonGuardMs<=5000&&cooldownMs<=10000&&
      softThresholdMg>=100&&softThresholdMg<=2000&&softReboundMs>=80&&softReboundMs<=300&&
      softWindowMs>=250&&softWindowMs<=1000&&softWindowMs>softReboundMs&&
      softGyroDps>=30&&softGyroDps<=600&&softMotionGuardMs>=100&&softMotionGuardMs<=2000&&
      softBaselineMs>=50&&softBaselineMs<=1000&&softRatioPercent>=40&&softRatioPercent<=95&&softGyroFilterMs<=1000;
  }
};
// Exactly one backend may enqueue a shortcut. Hand-held acquisition is silent.
inline bool tapShortcutSource(const TapParameters& p,bool software){return p.enabled&&!p.manual&&p.software==software;}
