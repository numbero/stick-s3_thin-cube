#include "../src/deep_sleep_timer.h"
#include "../src/power_save_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct Bus {
  uint8_t pm[256]{};
  unsigned operations=0,failAt=0;
  bool readMismatch=false;
  Bus(){pm[4]=4;pm[0x12]=0x10;}
  bool allowed(){return ++operations!=failAt;}
  bool pmRead(uint8_t r,uint8_t* p,size_t n){
    if(!allowed())return false;
    memcpy(p,pm+r,n);if(readMismatch&&r==0x3c)*p^=1;return true;
  }
  bool pmWrite(uint8_t r,uint8_t v){pm[r]=v;return allowed();}
  uint32_t seconds()const{return uint32_t(pm[0x38])|(uint32_t(pm[0x39])<<8)|
    (uint32_t(pm[0x3a])<<16)|(uint32_t(pm[0x3b])<<24);}
};
using Timer=DeepSleepTimer<Bus>;

int main(){
  {
    // Run the actual control ACK -> sleep grace -> PM1 timer admission path.
    // Heartbeats and sleep ACKs must preserve the original user idle deadline.
    IdlePowerPolicy idle;idle.activity(1000);
    DeepSleepPolicy sleep;sleep.begin(601000,42,true);
    assert(!idleControlActivity("heartbeat")&&!idleControlActivity("sleep_ack"));
    assert(idleControlActivity("settings")&&idleControlActivity("start"));
    if(idleControlActivity("sleep_ack"))idle.activity(601100);
    assert(sleep.ack(42));
    assert(sleep.update(601350,true,true)==DeepSleepPolicy::Decision::Commit);
    Bus b;assert(Timer::arm(b,idle.elapsed(601350))&&b.seconds()==600);
    assert(idle.lastActivity==1000);
    // Missing ACK and failed attempts back off without granting fake activity.
    sleep.begin(601000,43,true);
    assert(sleep.update(603000,true,true)==DeepSleepPolicy::Decision::Cancel);
    assert(!sleep.ready(632999)&&sleep.ready(633000));
    assert(idle.lastActivity==1000&&Timer::remainingSeconds(idle.elapsed(633000))==568);
    sleep.retryLater(1200999);
    assert(!sleep.ready(1201000));
    assert(idle.update(1201000,false,true,true,true)==IdlePowerPolicy::Action::PowerOff);
    sleep.retryLater(0xfffffff0u);assert(!sleep.ready(24));assert(sleep.ready(29984));
  }
  assert(Timer::remainingSeconds(600000)==600);
  assert(Timer::remainingSeconds(601350)==599); // includes ACK/prepare time
  assert(Timer::remainingSeconds(900000)==300); // late entry still ends at 20m
  assert(Timer::remainingSeconds(1199999)==1);
  assert(Timer::remainingSeconds(1200000)==0);
  assert(Timer::remainingSeconds(UINT32_MAX)==0);
  unsigned armOperations=0,bootOperations=0;
  {
    Bus b;assert(Timer::arm(b,601350));armOperations=b.operations;
    assert(b.seconds()==599&&b.pm[0x3c]==0x0b&&b.pm[0x3d]==0xa5);
    b.pm[5]=1;b.operations=0;
    const auto boot=Timer::consumeBoot(b);bootOperations=b.operations;
    assert(boot.expired&&boot.clean&&Timer::mayPowerOff(b));
    assert(!b.pm[0x3c]&&memcmp(b.pm+Timer::RTC,Timer::Cookie,sizeof(Timer::Cookie)));
    assert(!Timer::consumeBoot(b).expired); // one-shot even if final cut failed
  }
  // Fail every read/write, including applied writes followed by failed ACKs.
  for(unsigned fault=1;fault<=armOperations;++fault){
    Bus b;b.failAt=fault;assert(!Timer::arm(b,600000));
    assert(!b.pm[Timer::Config]&&memcmp(b.pm+Timer::RTC,Timer::Cookie,sizeof(Timer::Cookie)));
  }
  for(unsigned fault=1;fault<=bootOperations;++fault){
    Bus b;assert(Timer::arm(b,600000));b.pm[5]=1;b.operations=0;b.failAt=fault;
    const auto boot=Timer::consumeBoot(b);assert(!(boot.expired&&boot.clean));
    b.failAt=0;assert(Timer::cancel(b));assert(!Timer::consumeBoot(b).expired);
  }
  // Motion, power key, USB and reset, including simultaneous timer/user wakes,
  // cancel the old PM1 deadline and continue a normal boot.
  for(unsigned wake=0;wake<128;++wake){
    Bus b;assert(Timer::arm(b,600000));b.pm[5]=wake;
    const auto boot=Timer::consumeBoot(b);assert(boot.clean&&boot.expired==(wake==1));
    assert(!b.pm[Timer::Config]);
  }
  for(unsigned byte=0;byte<sizeof(Timer::Cookie);++byte){
    Bus b;assert(Timer::arm(b,600000));b.pm[5]=1;b.pm[Timer::RTC+byte]^=1;
    assert(!Timer::consumeBoot(b).expired);
  }
  {
    Bus b;assert(!Timer::arm(b,300000));assert(!Timer::arm(b,599999)); // screen-off only
    assert(!Timer::arm(b,1200000));assert(!b.pm[Timer::Config]);
    b.readMismatch=true;assert(!Timer::arm(b,600000));assert(!b.pm[Timer::Config]);
  }
  {
    Bus original;original.pm[5]=1;assert(Timer::mayPowerOff(original));
    const unsigned guardOperations=original.operations;
    for(unsigned fault=1;fault<=guardOperations;++fault){
      Bus b;b.pm[5]=1;b.failAt=fault;assert(!Timer::mayPowerOff(b));
    }
    for(unsigned source=0;source<8;++source){
      Bus b;b.pm[5]=1;b.pm[4]=source;assert(Timer::mayPowerOff(b)==(source==4));
    }
    Bus b;b.pm[5]=1;b.pm[0x24]=0xe8;b.pm[0x25]=3;assert(!Timer::mayPowerOff(b));
    b.pm[0x24]=b.pm[0x25]=0;b.pm[0x48]=1;assert(!Timer::mayPowerOff(b));
    b.pm[0x48]=0;b.pm[0x12]=0;assert(!Timer::mayPowerOff(b));
    b.pm[0x12]=0x10;b.pm[5]|=0x20;assert(!Timer::mayPowerOff(b));
  }
  {
    // A pickup at 15m discards the old timer. Screen off happens 5m after the
    // new boot/activity, deep sleep at 10m, and the new PM1 timer lasts 10m.
    Bus b;assert(Timer::arm(b,600000));b.pm[5]=0x20;
    const auto boot=Timer::consumeBoot(b);assert(!boot.expired&&boot.clean);
    IdlePowerPolicy idle;idle.activity(1000);
    assert(idle.update(300999,false,false,true,true)==IdlePowerPolicy::Action::None);
    assert(idle.update(301000,false,false,true,true)==IdlePowerPolicy::Action::Sleep);
    assert(!idle.deepSleepDue(idle.elapsed(301000)));
    assert(Timer::arm(b,idle.elapsed(601350))&&b.seconds()==600);
  }
  printf("5/10/20m PM1 deadline, one-shot wake, USB/user precedence, %u arm and %u boot fault points passed\n",
    armOperations,bootOperations);
}
