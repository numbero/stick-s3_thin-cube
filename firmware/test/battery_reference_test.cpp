#include "../src/battery_gauge.h"
#include "../src/battery_curve_config.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void seed(BatteryGauge& gauge, int mv, int source=0, unsigned start=0){
  for(unsigned sec=0;sec<7;++sec)gauge.sample((start+sec)*1000,mv,source);
}

int main(){
  const auto& curve=ReferenceBattery::Curve;
  assert(curve.valid());
  assert(curve.level(2500)==0 && curve.level(4500)==100);
  for(int mv=2500;mv<4500;++mv)assert(curve.level(mv)<=curve.level(mv+1));
  // Plateau/tail and the full-charge boundary differ from the old straight line.
  assert(curve.level(3687)==10 && curve.level(3793)==40);
  assert(curve.level(3860)==56 && BatteryGauge::voltageLevel(3860)==70);
  assert(curve.level(4100)==92 && curve.level(4177)==100);
  // No display-model switch or five-second warmup on changes in UI/audio/radio.
  assert(!CalibratedBattery::Enabled);
  for(bool battery:{false,true})for(bool quiet:{false,true})for(unsigned brightness:{0u,12u,134u,255u}){
    const auto selected=selectBatteryCurve(0x6046e5b1e87cULL,battery,quiet,brightness);
    assert(selected.curve==&curve && strcmp(selected.model,"lipo-ref-an4189")==0);
  }
  BatteryGauge gauge;gauge.setCurve(&curve);seed(gauge,3860);
  assert(gauge.level(6000)==56 && gauge.otaLevel(6000)==56);
  gauge.setCurve(&curve);gauge.sample(7000,3860,0);assert(gauge.level(7000)==56);
  // Long-term smoothing still tracks a sustained drop; low voltage warns sooner.
  for(unsigned sec=8;sec<70;++sec){
    gauge.sample(sec*1000,3300,0);
    if(sec>=13)assert(gauge.low(sec*1000));
    if(sec>=40)assert(gauge.level(sec*1000)<=1);
  }
  assert(gauge.level(69000)==0);
  // The new, higher full-charge endpoint cannot loosen OTA admission.
  BatteryGauge full;full.setCurve(&curve);seed(full,4100,1);
  assert(full.level(6000)==92 && full.otaLevel(6000)==92);
  full.sample(7000,3400,1);assert(full.otaLevel(7000)<=BatteryGauge::voltageLevel(3400));
  // Charge-to-battery transition and read loss still produce unknown, never 0%.
  for(unsigned sec=8;sec<12;++sec){full.sample(sec*1000,3860,0);assert(full.level(sec*1000)==-1);}
  full.sample(12000,3860,0);assert(full.level(12000)==56);
  full.sample(13000,-1,0);assert(full.otaLevel(13000)==-1);
  assert(full.level(18000)==-1);
  puts("LiPo reference table, operating-mode continuity, low response, source reset and OTA safety passed");
}
