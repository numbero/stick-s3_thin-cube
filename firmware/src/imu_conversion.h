#pragma once
#include <stdint.h>

// M5Unified raw 16.16 offsets are in sensor coordinates. Use a wide
// subtraction (no negative left shift / signed overflow), then map board axes.
inline void imuConvert(const int16_t raw[3],const int32_t offset[3],float scale,float out[3]){
  float sensor[3];
  for(unsigned i=0;i<3;++i)sensor[i]=float(int64_t(raw[i])*65536-int64_t(offset[i]))*(scale/65536.0f);
  out[0]=sensor[1];out[1]=-sensor[0];out[2]=sensor[2];
}
