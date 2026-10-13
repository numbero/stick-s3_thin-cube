#include "../src/battery_calibration.h"
#include <assert.h>
#include <stdio.h>
using Cal=BatteryCalibration;

static uint32_t running(Cal& c,uint32_t start=0){
  c.arm(start,42);
  c.update(start,4100,4100,1,true,false,true);
  c.update(start+Cal::ChargeStableMs,4100,4100,1,true,false,true);
  assert(c.state()==Cal::Ready);
  c.update(start+Cal::ChargeStableMs+1000,4080,-1,0,true,false,true);
  assert(c.state()==Cal::Settling);
  const auto now=start+Cal::ChargeStableMs+1000+Cal::SettleMs;
  c.update(now,4070,4070,0,true,false,true);
  assert(c.state()==Cal::Running&&c.elapsed(now)==0);
  return now;
}
int main(){
  {
    Cal c;c.arm(0,0);assert(c.id()!=0&&c.ownsUI());
    c.update(1000,3900,3900,0,true,false,true);
    assert(c.state()==Cal::Aborted&&c.reason()==Cal::Uncharged&&!c.ownsUI());
  }
  {
    Cal c;c.arm(0,1);
    for(unsigned s=0;s<250;++s)c.update(s*1000,4100,4100,1,true,true,true);
    assert(c.state()==Cal::Charging);
    c.update(250000,4100,4100,1,true,false,true);
    c.update(400000,4100,4100,1,false,false,true);
    c.update(401000,4100,4100,1,true,false,true);
    assert(c.state()==Cal::Charging&&c.chargeStable(401000)==0);
    c.update(450000,4150,4150,1,true,false,true);
    assert(c.chargeStable(450000)==0); // Voltage still moving: not full.
  }
  {
    Cal c;const auto start=running(c);
    for(unsigned i=0;i<10;++i)c.update(start+i*1000,3290,3290,0,true,false,true);
    assert(c.state()==Cal::Running&&c.brightness(start+30000)==12);
    const auto low=start+Cal::PhaseMs+10000;
    assert(c.brightness(low)==134);
    for(unsigned i=0;i<4;++i){c.update(low+i*1000,3290,3290,0,true,false,true);assert(c.state()==Cal::Running);}
    c.update(low+4000,3290,3290,0,true,false,true);
    assert(c.state()==Cal::Complete&&c.reason()==Cal::LowVoltage&&c.elapsed(low+5000)==74000);
    assert(!c.shutdownDue(low+13999)&&c.shutdownDue(low+14000));
    c.shutdownAttempted();assert(!c.shutdownDue(low+14000));
    c.cancel();assert(c.state()==Cal::Complete);
  }
  {
    Cal c;const auto start=running(c);
    c.update(start+1000,4000,4000,1,true,false,true);
    assert(c.reason()==Cal::Plugged&&c.state()==Cal::Aborted);
  }
  {
    Cal c;const auto start=running(c);
    c.update(start+15000,4000,4000,0,true,false,false);
    assert(c.reason()==Cal::LinkLost&&c.state()==Cal::Aborted);
  }
  {
    Cal c;const auto start=running(c);
    c.update(start+10000,-1,4000,0,false,false,true);
    assert(c.reason()==Cal::ReadLost);
    running(c);c.update(start+10000,4000,4000,-1,true,false,true);
    assert(c.reason()==Cal::ReadLost);
  }
  {
    Cal c;running(c);c.cancel(Cal::Movement);assert(c.reason()==Cal::Movement&&!c.active());
    c.arm(0,42);c.update(Cal::MaxChargeMs,4100,4100,1,true,false,true);assert(c.reason()==Cal::Timeout);
  }
  {
    Cal c;const auto start=running(c,0xffff0000u);
    c.update(start+Cal::MaxRunMs,3800,3800,0,true,false,true);
    assert(c.reason()==Cal::Timeout);
  }
  puts("8 battery calibration charge, profile, endpoint, abort and clock-wrap scenarios passed");
}
