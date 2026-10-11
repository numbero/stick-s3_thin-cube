#pragma once
#include <math.h>
#include <stdint.h>

// Detect handling, including slow tilts and yaw whose acceleration magnitude
// remains 1g. Anchor comparison accumulates slow changes instead of comparing
// only adjacent samples; thresholds keep stationary sensor noise from waking.
struct MotionActivity {
  static constexpr uint32_t FreshMs=1000;
  bool initialized=false,valid=false;
  uint32_t sampledAt=0;
  float anchor[3]={0,0,0},reference=1;
  bool update(float x,float y,float z,float gyro,uint32_t now){
    sampledAt=now;
    const float mag=sqrtf(x*x+y*y+z*z);
    valid=isfinite(mag)&&isfinite(gyro)&&mag>.05f&&gyro>=0;
    if(!valid)return false;
    if(!initialized){reference=mag>5?9.81f:1.f;initialized=true;anchor[0]=x/mag;anchor[1]=y/mag;anchor[2]=z/mag;return false;}
    const float unit[3]={x/mag,y/mag,z/mag};
    float change=0;
    for(unsigned i=0;i<3;i++){const float d=unit[i]-anchor[i];change+=d*d;}
    const bool moved=fabsf(mag/reference-1)>.10f||gyro>12.f||change>.0049f; // about 4 degrees
    if(moved)for(unsigned i=0;i<3;i++)anchor[i]=unit[i];
    return moved;
  }
  bool healthy(uint32_t now)const{return initialized&&valid&&uint32_t(now-sampledAt)<=FreshMs;}
};
