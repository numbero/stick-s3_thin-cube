#pragma once
#include <stdint.h>

// Temporary accel-only, filtered, headerless FIFO at the existing 200Hz ODR.
// Does not change raw accel/gyro, tap parameters, IRQ routing or power mode.
template<class Bus> class ImuFifoCapture {
 public:
  bool begin(Bus& bus){
    captured_=false;
    if(!bus.read(0x45,&down_,1)||!bus.read(0x48,config_,2))return false;
    if(config_[1]&0xe0)return false; // Never take over another FIFO consumer.
    captured_=true;
    const uint8_t off[2]={0,0},filtered=(down_&0x0f)|0x80,on[2]={0,0x40};
    return set(bus,0x48,off,2)&&set(bus,0x45,&filtered,1)&&flush(bus)&&set(bus,0x48,on,2);
  }
  bool restore(Bus& bus){
    if(!captured_)return true;
    const uint8_t off[2]={0,0};
    bool ok=set(bus,0x48,off,2);
    ok=flush(bus)&&ok;
    ok=set(bus,0x45,&down_,1)&&ok;
    ok=set(bus,0x48,config_,2)&&ok;
    if(ok)captured_=false;
    return ok;
  }
 private:
  bool captured_=false;
  uint8_t down_=0,config_[2]{};
  static bool flush(Bus& bus){const uint8_t command=0xb0;const bool ok=bus.write(0x7e,&command,1);bus.pause(2);return ok;}
  static bool set(Bus& bus,uint8_t reg,const uint8_t* data,unsigned n){
    if(!bus.write(reg,data,n))return false;
    bus.pause(1);uint8_t back[2]{};
    if(!bus.read(reg,back,n))return false;
    for(unsigned i=0;i<n;++i)if(data[i]!=back[i])return false;
    return true;
  }
};
