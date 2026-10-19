#pragma once
#include <stdint.h>
#include <math.h>
#include "tap_parameters.h"

// Causal, single-axis impulse counting. The FIFO supplies every 5 ms sample;
// the refractory period excludes the measured ~100–135 ms housing rebound.
struct SoftwareTapDetector {
  float baseline=0,lastPeak=0;
  float maxGyro=0,gyroLevel=0;
  uint32_t last=0,warmAt=0,motionAt=0,first=0,lastImpact=0;
  uint32_t candidates=0,recognized=0,rejected=0;
  uint32_t rejectedMotion=0,rejectedSingle=0,rejectedMultiple=0;
  int polarity=0;
  unsigned impacts=0;
  bool initialized=false,pulseUsed=false,haveMotion=false;
  void reset(){initialized=false;pulseUsed=false;impacts=0;haveMotion=false;}
  bool update(uint32_t now,float acceleration,float gyro,const TapParameters& p){
    if(!isfinite(acceleration)||!isfinite(gyro)||fabsf(acceleration)>8.01f){reset();return false;}
    if(!initialized||uint32_t(now-last)>50){
      baseline=acceleration;gyroLevel=gyro;last=warmAt=now;initialized=true;pulseUsed=false;impacts=0;haveMotion=false;return false;
    }
    const uint32_t dt=now-last;last=now;
    if(!dt)return false;
    const float delta=acceleration-baseline;
    baseline+=delta*float(dt)/float(p.softBaselineMs+dt);
    // Tap vibration produces short gyro spikes. Gate on sustained rotation,
    // with an optional causal low pass, while preserving the raw diagnostic.
    gyroLevel=p.softGyroFilterMs?gyroLevel+(gyro-gyroLevel)*float(dt)/float(p.softGyroFilterMs+dt):gyro;
    const bool high=fabsf(delta)*1000>=p.softThresholdMg;
    if(!high)pulseUsed=false;
    const float amplitude=fabsf(delta);
    const int sign=delta<0?-1:1;
    if(high&&gyro>maxGyro)maxGyro=gyro;
    if(impacts&&sign==polarity&&uint32_t(now-lastImpact)<p.softReboundMs/2&&high&&pulseUsed&&amplitude>lastPeak)lastPeak=amplitude;
    if(gyroLevel>p.softGyroDps){
      if(impacts){++rejected;++rejectedMotion;}
      impacts=0;motionAt=now;haveMotion=true;return false;
    }
    if(uint32_t(now-warmAt)<p.softBaselineMs||
       (haveMotion&&uint32_t(now-motionAt)<p.softMotionGuardMs)){impacts=0;return false;}
    bool result=false;
    if(impacts&&uint32_t(now-first)>p.softWindowMs){
      result=impacts==2;
      if(result)++recognized;else{++rejected;if(impacts==1)++rejectedSingle;else ++rejectedMultiple;}
      impacts=0;
    }
    const uint32_t separation=uint32_t(now-lastImpact);
    // A strong early impulse is counted, so fast triple taps do not become
    // doubles. A weaker housing rebound cannot restart the refractory timer.
    const bool distinct=!impacts||separation>=p.softReboundMs||
      (separation>=p.softReboundMs/2&&amplitude*100>=lastPeak*p.softRatioPercent);
    if(high&&!pulseUsed&&distinct&&(!impacts||sign==polarity)){
      if(!impacts){first=now;polarity=sign;++candidates;}
      ++impacts;lastImpact=now;lastPeak=amplitude;pulseUsed=true;
      // Retain a count above two until the window ends, rejecting triple taps.
      if(impacts>3)impacts=3;
    }
    return result;
  }
};
