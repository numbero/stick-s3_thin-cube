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
  // Every operating mode uses the official display formula while retaining
  // the previous reference table solely as an OTA cap.
  assert(!CalibratedBattery::Enabled);
  for(bool battery:{false,true})for(bool quiet:{false,true})for(unsigned brightness:{0u,12u,134u,255u}){
    const auto selected=selectBatteryCurve(0x6046e5b1e87cULL,battery,quiet,brightness);
    assert(selected.curve==nullptr && selected.otaCurve==&curve);
    assert(strcmp(selected.model,"m5unified-linear")==0);
  }
  BatteryGauge gauge;gauge.setOtaCurve(&curve);seed(gauge,3860);
  assert(gauge.level(6000)==70 && gauge.otaLevel(6000)==56);
  gauge.setCurve(nullptr);gauge.setOtaCurve(&curve);
  gauge.sample(7000,3860,0);assert(gauge.level(7000)==70);
  // Long-term smoothing still tracks a sustained drop; low voltage warns sooner.
  for(unsigned sec=8;sec<70;++sec){
    gauge.sample(sec*1000,3300,0);
    if(sec>=13)assert(gauge.low(sec*1000));
    if(sec>=40)assert(gauge.level(sec*1000)<=2);
  }
  assert(gauge.level(69000)==0);
  // Displaying 100% at the official endpoint must not loosen OTA admission.
  BatteryGauge full;full.setOtaCurve(&curve);seed(full,4100,1);
  assert(full.level(6000)==100 && full.otaLevel(6000)==92);
  full.sample(7000,3400,1);assert(full.otaLevel(7000)<=BatteryGauge::voltageLevel(3400));
  // Charge-to-battery transition and read loss still produce unknown, never 0%.
  for(unsigned sec=8;sec<12;++sec){full.sample(sec*1000,3860,0);assert(full.level(sec*1000)==-1);}
  full.sample(12000,3860,0);assert(full.level(12000)==70);
  full.sample(13000,-1,0);assert(full.otaLevel(13000)==-1);
  assert(full.level(18000)==-1);
  // Compare the previous production policy with the new one throughout the
  // supported voltage range. Only display conversion may change; OTA must
  // remain identical, including around its low-battery admission boundary.
  for(int mv=2500;mv<=4500;++mv){
    BatteryGauge previous,current;
    previous.setCurve(&curve);current.setOtaCurve(&curve);
    seed(previous,mv);seed(current,mv);
    const int official=std::max(0,std::min(100,int((mv-3300)*100/800.0f)));
    assert(current.level(6000)==official);
    assert(current.otaLevel(6000)==previous.otaLevel(6000));
    assert(current.low(6000)==previous.low(6000));
  }
  // Realistic charging relaxation, supply changes, invalid reads and a low
  // transient must preserve the old safety decisions at every sample.
  BatteryGauge previous,current;
  previous.setCurve(&curve);current.setOtaCurve(&curve);
  const int readings[]={4192,4190,4190,4190,4192,4150,4132,4138,4124,
                        4108,4100,4090,4074,4054,4044,-1,4042,3400,
                        3380,3370,3360,3350,3340,3330,3320,3310,3300};
  unsigned tick=0;
  for(int mv:readings){
    const int source=tick<16?1:tick<18?-1:0;
    previous.sample(tick*1000,mv,source);current.sample(tick*1000,mv,source);
    assert(previous.otaLevel(tick*1000)==current.otaLevel(tick*1000));
    assert(previous.low(tick*1000)==current.low(tick*1000));
    ++tick;
  }
  assert(current.low((tick-1)*1000));
  assert(current.level((tick+5)*1000)==-1 && current.otaLevel((tick+5)*1000)==-1);
  puts("Official linear display, unchanged OTA/low safety, mode continuity, source reset and freshness passed");
}
