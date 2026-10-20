#pragma once
#include "idle_power.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// PM1 keeps counting after the ESP rail is cut. Timer POWERON permits an early
// boot to remove IMU/GPIO4 HOLD and motion wake before the final full shutdown;
// timer POWEROFF alone would preserve those settings. RTC RAM is unused by the
// rest of the application; the versioned cookie never writes paired-device NVS.
template<class Bus> struct DeepSleepTimer {
  static constexpr uint8_t Counter=0x38,Config=0x3c,Key=0x3d,RTC=0xa0;
  static constexpr uint8_t Cookie[]={0x54,0x43,0x53,0x4c,0x02,0xfd,0xa5,0x5a};
  struct Boot { bool expired=false,clean=false;uint8_t wake=0; };

  static uint32_t remainingSeconds(uint32_t idleMs){
    return idleMs>=IdlePowerPolicy::OffMs?0:(IdlePowerPolicy::OffMs-idleMs+999)/1000;
  }
  static bool writeChecked(Bus& bus,uint8_t reg,uint8_t value){
    uint8_t read=0;
    return bus.pmWrite(reg,value)&&bus.pmRead(reg,&read,1)&&read==value;
  }
  static bool cancel(Bus& bus){
    // Attempt all cleanup, including after a write that applied but failed ACK.
    bool ok=writeChecked(bus,Config,0);
    ok=bus.pmWrite(Key,0xa5)&&ok;
    for(size_t n=0;n<sizeof(Cookie);++n)ok=writeChecked(bus,RTC+n,0)&&ok;
    return ok;
  }
  static bool arm(Bus& bus,uint32_t idleMs){
    const uint32_t seconds=remainingSeconds(idleMs);
    bool ok=IdlePowerPolicy::deepSleepDue(idleMs)&&cancel(bus);
    for(unsigned n=0;n<4&&ok;++n)ok=writeChecked(bus,Counter+n,uint8_t(seconds>>(8*n)));
    for(size_t n=0;n<sizeof(Cookie)&&ok;++n)ok=writeChecked(bus,RTC+n,Cookie[n]);
    // Official timerSet sequence: count, ARM|POWERON, then reload key.
    ok=ok&&writeChecked(bus,Config,0x0b)&&bus.pmWrite(Key,0xa5);
    if(!ok)cancel(bus);
    return ok;
  }
  static Boot consumeBoot(Bus& bus){
    Boot result;uint8_t cookie[sizeof(Cookie)]={};
    bool read=bus.pmRead(0x05,&result.wake,1);
    for(size_t n=0;n<sizeof(Cookie);++n)read=bus.pmRead(RTC+n,&cookie[n],1)&&read;
    result.expired=read&&result.wake==1&&!memcmp(cookie,Cookie,sizeof(Cookie));
    result.clean=cancel(bus);
    return result;
  }
  static bool mayPowerOff(Bus& bus){
    // User input or USB at the deadline takes precedence over a timer wake.
    // Failed/unknown reads always continue a normal, usable boot.
    uint8_t wake=0,source=0,vin[2]={},button=0,gpio=0;
    return bus.pmRead(0x05,&wake,1)&&wake==1&&
      bus.pmRead(0x04,&source,1)&&bus.pmRead(0x24,vin,2)&&
      idleBatteryOnly(true,source,uint16_t(vin[0])|(uint16_t(vin[1])<<8))&&
      bus.pmRead(0x48,&button,1)&&!(button&1)&&
      bus.pmRead(0x12,&gpio,1)&&(gpio&0x10);
  }
};
// Arduino's pinned compiler uses C++11, where address-used constants need this.
template<class Bus> constexpr uint8_t DeepSleepTimer<Bus>::Cookie[];
