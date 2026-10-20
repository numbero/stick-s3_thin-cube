#include "../src/idle_power.h"
#include "../src/motion_activity.h"
#include "../src/voice_state.h"
#include <assert.h>
#include <stdio.h>
#include <limits>
#include <initializer_list>

using Action=IdlePowerPolicy::Action;
int main(){
  {
    // One automatic policy: all three stages share one activity clock.
    IdlePowerPolicy p;p.activity(100);
    assert(p.update(300099,false,false,true)==Action::None);
    assert(p.update(300100,false,false,true)==Action::Sleep);
    assert(!p.deepSleepDue(p.elapsed(300100)));
    assert(!p.deepSleepDue(p.elapsed(600099)));
    assert(p.update(600100,false,true,true)==Action::None);
    assert(p.deepSleepDue(p.elapsed(600100)));
    assert(p.update(1200099,false,true,true)==Action::None);
    assert(p.deepSleepDue(p.elapsed(1200099)));
    assert(!p.deepSleepDue(p.elapsed(1200100)));
    assert(p.update(1200100,false,true,true)==Action::PowerOff);
    // Deep-sleep preparation unavailable/failed: online standby still shuts
    // down at 20m; there is no separate ten-minute power-off mode.
    for(uint32_t age=600000;age<1200000;age+=1000)
      assert(p.update(age+100,false,true,true)==Action::None);
    assert(p.update(1200100,false,true,false)==Action::None); // USB
    // Actual activity and protected work each grant a complete new window.
    p.activity(900000);
    assert(p.update(1200000,false,false,true)==Action::Sleep);
    assert(!p.deepSleepDue(p.elapsed(1200000)));
    assert(p.update(2100000,true,true,true)==Action::None);
    assert(p.update(2100001,false,false,true)==Action::None);
    assert(p.update(2400000,false,false,true)==Action::Sleep);
    assert(p.deepSleepDue(p.elapsed(2700000)));
    assert(p.update(3300000,false,true,true)==Action::PowerOff);
    // Millis wrap preserves the longer 20m deadline too.
    p.activity(0xfffffff0u);
    assert(p.deepSleepDue(p.elapsed(599984)));
    assert(p.update(1199983,false,true,true)==Action::None);
    assert(p.update(1199984,false,true,true)==Action::PowerOff);
  }
  {
    // Full-battery charge indication pulses do not change the actual input.
    for(uint8_t source:{uint8_t(4),uint8_t(5),uint8_t(6)})assert(idlePowerSource(true,source,5000)==1);
    assert(idlePowerSource(true,4,4000)==1);assert(idlePowerSource(true,4,300)==0);
    assert(idlePowerSource(true,4,3999)==-1);assert(idlePowerSource(true,5,0)==-1);
    assert(idlePowerSource(false,4,5000)==-1);assert(idlePowerSource(false,4,0)==-1);
  }
  {
    // Visual sleep comes first and must not restart either power deadline.
    IdlePowerPolicy p;p.activity(100);
    assert(petIdleSleepDue(30101,p.lastActivity,31,false,false,false));
    assert(p.update(30101,false,false,true)==Action::None);
    assert(p.update(300099,false,false,true)==Action::None);
    assert(p.update(300100,false,false,true)==Action::Sleep);
    assert(p.update(300101,false,true,true)==Action::None);
    assert(p.update(600099,false,true,true)==Action::None);
    assert(p.update(600100,false,true,true)==Action::None);
    assert(p.deepSleepDue(p.elapsed(600100)));
    assert(p.update(1200100,false,true,true)==Action::PowerOff);
  }
  {
    // Mac heartbeats and animations do not call activity(). USB may remain
    // in standby indefinitely and never reaches an automatic power cut.
    IdlePowerPolicy p;p.activity(10);
    for(uint32_t t=110;t<300010;t+=100)assert(p.update(t,false,false,false)==Action::None);
    assert(p.update(300010,false,false,false)==Action::Sleep);
    for(uint32_t t=300110;t<3600000;t+=100)assert(p.update(t,false,true,false)==Action::None);
  }
  {
    // Voice, games, pairing, setup, OTA and unavailable IMU all block the same
    // policy. A completed long task grants a fresh 5/10/20-minute window.
    IdlePowerPolicy p;p.activity(0);
    for(uint32_t t=10;t<=599990;t+=10)assert(p.update(t,true,false,true)==Action::None);
    assert(p.update(599999,true,true,true)==Action::None);
    assert(p.update(600000,false,false,true)==Action::None);
    assert(p.update(899999,false,false,true)==Action::Sleep);
    assert(p.update(1199999,false,true,true)==Action::None);
    assert(p.deepSleepDue(p.elapsed(1199999)));
    assert(p.update(1799999,false,true,true)==Action::PowerOff);
  }
  {
    // A/B holds, a fresh physical tilt, and explicit Mac start reset both
    // timers even when the device was already asleep close to power-off.
    IdlePowerPolicy p;p.activity(0);p.activity(599999);
    assert(p.update(600000,false,false,true)==Action::None);
    assert(p.update(899998,false,false,true)==Action::None);
    assert(p.update(899999,false,false,true)==Action::Sleep);
    assert(p.update(1199999,false,true,true)==Action::None);
    assert(p.deepSleepDue(p.elapsed(1199999)));
    assert(p.update(1799999,false,true,true)==Action::PowerOff);
    VoiceState v;v.allowed=true;v.peer=true;v.sleeping=true;
    assert(!v.captureWanted());v.sleeping=false;v.captureReady=true;
    assert(v.startManual(false));assert(v.streaming&&v.captureWanted());
    assert(p.update(1800000,v.streaming,false,true)==Action::None);
  }
  {
    // Clock wrap and same-loop later activity preserve bounded deadlines.
    IdlePowerPolicy p;p.activity(0xfffffff0u);
    assert(p.elapsed(24)==40);
    assert(p.update(299984,false,false,true)==Action::Sleep);
    assert(p.update(599984,false,true,true)==Action::None);
    assert(p.deepSleepDue(p.elapsed(599984)));
    assert(p.update(1199984,false,true,true)==Action::PowerOff);
    p.activity(100001);assert(p.update(100000,false,false,true)==Action::None);
  }
  {
    assert(idleBatteryOnly(true,4,0));assert(idleBatteryOnly(true,4,300));
    for(uint8_t source=0;source<8;source++)if(source!=4)assert(!idleBatteryOnly(true,source,0));
    assert(!idleBatteryOnly(false,4,0));assert(!idleBatteryOnly(true,4,1000));
    assert(!idleBatteryOnly(true,4,5000));assert(!idleBatteryOnly(true,5,5000));
  }
  {
    MotionActivity m;assert(!m.healthy(0));
    assert(!m.update(0,0,1,0,0));assert(m.healthy(1000));assert(!m.healthy(1001));
    // Thousands of stationary noisy samples must not extend the deadline.
    for(unsigned n=1;n<5000;n++)assert(!m.update((n%2?.005f:-.005f),0,.99f,2,n*5));
    // A gradual 5-degree tilt has ~1g magnitude and no large gyro spike.
    bool tilted=false;
    for(unsigned degrees=0;degrees<=5;degrees++){
      const float radians=degrees*3.14159265f/180;
      tilted=m.update(sinf(radians),0,cosf(radians),1,25000+degrees*20)||tilted;
    }
    assert(tilted);
    assert(m.update(0,0,1,30,26000)); // yaw / rotation around gravity
    assert(m.update(0,0,1.2f,0,27000)); // pickup acceleration
    assert(!m.update(0,0,0,0,28000));assert(!m.healthy(28000));
    assert(!m.update(std::numeric_limits<float>::quiet_NaN(),0,1,0,29000));assert(!m.healthy(29000));
    assert(!m.update(0,0,1,std::numeric_limits<float>::infinity(),30000));assert(!m.healthy(30000));
  }
  {
    MotionActivity m;assert(!m.update(0,0,9.81f,0,0xfffffff0u));
    assert(m.healthy(24));assert(m.update(2,0,9.6f,0,40));
  }
  puts("unified 5/10/20m stages, busy/wake/USB guards, clock wrap and motion scenarios passed");
}
