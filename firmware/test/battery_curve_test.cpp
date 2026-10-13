#include "../src/battery_gauge.h"
#include <assert.h>
#include <stdio.h>

int main(){
  const BatteryCurvePoint points[]={{3300,0},{3700,20},{3900,70},{4100,100}};
  const BatteryCurve curve{points,4};
  assert(curve.valid()&&curve.level(2500)==0&&curve.level(4500)==100);
  assert(curve.level(3700)==20&&curve.level(3800)==45&&curve.level(4000)==85);
  for(int mv=2500;mv<4500;++mv)assert(curve.level(mv)<=curve.level(mv+1));
  const BatteryCurvePoint descending[]={{3300,0},{3900,80},{3800,100}};
  const BatteryCurvePoint badPercent[]={{3300,0},{3900,90},{4100,80}};
  const BatteryCurvePoint duplicate[]={{3300,0},{3300,20},{4100,100}};
  assert(!(BatteryCurve{descending,3}).valid());
  assert(!(BatteryCurve{badPercent,3}).valid());
  assert(!(BatteryCurve{duplicate,3}).valid());
  assert((BatteryCurve{nullptr,0}).level(3900)==-1);
  BatteryGauge gauge;
  for(unsigned s=0;s<7;++s)gauge.sample(s*1000,3800,0);
  assert(gauge.level(6000)==62);
  gauge.setCurve(&curve);
  for(unsigned s=7;s<11;++s){gauge.sample(s*1000,3800,0);assert(gauge.level(s*1000)==-1);}
  gauge.sample(11000,3800,0);
  assert(gauge.level(11000)==45&&gauge.otaLevel(11000)==45);
  gauge.setCurve(&curve);gauge.sample(12000,3800,0);assert(gauge.level(12000)==45);
  const BatteryCurvePoint optimistic[]={{3300,0},{3500,100}};
  const BatteryCurve high{optimistic,2};gauge.setCurve(&high);
  for(unsigned s=13;s<20;++s)gauge.sample(s*1000,3500,0);
  assert(gauge.level(19000)==100&&gauge.otaLevel(19000)==25);
  gauge.setCurve(nullptr);
  for(unsigned s=20;s<27;++s)gauge.sample(s*1000,3800,0);
  assert(gauge.level(26000)==62);
  puts("battery curve interpolation, validation, model transition and conservative OTA passed");
}
