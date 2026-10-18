#include "../src/tap_diagnostic.h"
#include "../src/imu_conversion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(){
  TapDiagnostic t;
  assert(t.event(1)==TapDiagnostic::Reason::Inactive);
  t.update(100,true,false);
  assert(t.event(599)==TapDiagnostic::Reason::Settling);
  assert(t.event(600)==TapDiagnostic::Reason::Accepted);
  assert(t.event(601)==TapDiagnostic::Reason::Cooldown);
  // Two separate events with the same double-tap output are both accepted.
  assert(t.event(1200)==TapDiagnostic::Reason::Accepted);
  t.update(1300,true,true);assert(t.event(1300)==TapDiagnostic::Reason::Button);
  t.update(1400,true,false);assert(t.event(1799)==TapDiagnostic::Reason::Settling);
  t.update(1800,false,false);assert(t.event(2000)==TapDiagnostic::Reason::Inactive);
  t.update(2100,true,false);assert(t.event(2600)==TapDiagnostic::Reason::Accepted);
  assert(t.raw==9&&t.accepted==3&&t.blocked==6);
  t={};t.update(UINT32_MAX-200,true,false);
  assert(t.event(298)==TapDiagnostic::Reason::Settling);
  assert(t.event(299)==TapDiagnostic::Reason::Accepted);
  int16_t raw[3]={4096,8192,-4096};int32_t bias[3]={65536*4096,65536*4096,0};float out[3];
  imuConvert(raw,bias,8.0f/32768,out);
  assert(fabsf(out[0]-1)<0.00001f&&out[1]==0&&out[2]==-1);
  raw[0]=INT16_MIN;bias[0]=INT32_MAX;
  imuConvert(raw,bias,8.0f/32768,out);assert(out[1]>15.99f&&out[1]<=16);
  puts("tap admission, repeated gestures, button/transition suppression, wraparound and M5 offset/axis compatibility passed");
}
