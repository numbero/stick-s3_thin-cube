#pragma once
#include <stdint.h>
#include <stddef.h>

// Pinned M5Unified BMI270 blob matches Bosch base config (see research note).
// Only the base ANY_MOTION four-byte feature is modified. Bus implements checked
// read/write + 1ms waits; this transaction never sends a shutdown command.
template<class Bus> class MotionWakeRegisters {
 public:
  bool prepare(Bus& bus){
    captured_=featureCaptured_=false;
    uint8_t id=0,status=0;
    if(!bus.bmiRead(0x00,&id,1)||id!=0x24||!bus.bmiRead(0x21,&status,1)||!statusOK(status))return false;
    for(size_t i=0;i<PMCount;++i)if(!bus.pmRead(PM[i],&pm_[i],1))return false;
    for(size_t i=0;i<BMICount;++i)if(!bus.bmiRead(BMI[i],&bmi_[i],1))return false;
    captured_=true;
    if(!pmMask(bus,0x18,0x18,0)||!bmiMask(bus,0x7C,1,0)||!bmiMask(bus,0x2F,7,1))return false;
    if(!bus.bmiRead(0x3C,feature_,4))return false;
    featureCaptured_=true;
    const uint8_t motion[]={0x05,0xE0,0xAA,0xB8}; // xyz,100ms;83mg,out_conf7,enable
    if(!bmiWrite(bus,0x3C,motion,4)||!bmiMask(bus,0x40,0xff,0x17)||
       !bmiMask(bus,0x7D,0x0f,0x04)||!bmiMask(bus,0x53,0x1e,0x08)||
       !bmiMask(bus,0x55,1,0)||!bmiMask(bus,0x56,0xff,0x40)||
       !bmiMask(bus,0x57,0xff,0)||!bmiMask(bus,0x58,0xff,0))return false;
    if(!pmMask(bus,0x10,0x10,0)||!pmMask(bus,0x17,3,0)||!pmMask(bus,0x15,3,1)||
       !pmMask(bus,0x19,0x10,0)||!pmMask(bus,0x06,0x1c,0x14)||
       !pmMask(bus,0x07,0xff,0x30))return false;
    uint8_t clear[2]={};
    if(!bus.bmiRead(0x1C,clear,2)||!bus.pmWrite(0x05,0)||!bus.pmWrite(0x40,0))return false;
    if(!bmiMask(bus,0x2F,7,bmi_[0]&7)||!bmiMask(bus,0x7C,1,1))return false;
    // Require a quiet HIGH interrupt line for 60ms, and re-check internal state.
    for(unsigned n=0;n<3;++n){bus.pause(20);if(!lineHigh(bus))return false;}
    if(!bus.bmiRead(0x21,&status,1)||!statusOK(status))return false;
    return pmMask(bus,0x18,0x18,0x10)&&lineHigh(bus);
  }
  bool restore(Bus& bus){
    // Attempt every cleanup even after an error; never short-circuit rollback.
    bool ok=disarm(bus);
    if(!captured_)return ok;
    const uint8_t apsOff=bmi_[BMICount-1]&~1;
    const bool apsDisabled=bmiWrite(bus,0x7C,&apsOff,1);ok=apsDisabled&&ok;
    if(featureCaptured_&&apsDisabled){
      ok=bmiMask(bus,0x2F,7,1)&&ok;
      ok=bmiWrite(bus,0x3C,feature_,4)&&ok;
    }
    if(featureCaptured_&&!apsDisabled)ok=false;
    for(size_t i=1;i<BMICount;++i)if(BMI[i]!=0x7C)ok=bmiWrite(bus,BMI[i],&bmi_[i],1)&&ok;
    ok=bmiWrite(bus,0x2F,&bmi_[0],1)&&ok;
    uint8_t clear[2]={};ok=bus.bmiRead(0x1C,clear,2)&&ok;
    const uint8_t config=bmi_[BMICount-1];
    ok=bmiWrite(bus,0x7C,&config,1)&&ok;
    for(size_t i=0;i<PMCount;++i){
      uint8_t value=pm_[i];
      if(PM[i]==0x18)value&=~0x10; // normal operation must never retain motion wake
      if(PM[i]==0x07)value&=~0x30;
      ok=pmWrite(bus,PM[i],value)&&ok;
    }
    return ok;
  }
  static bool disarm(Bus& bus){
    bool ok=pmMask(bus,0x18,0x10,0);
    ok=pmMask(bus,0x07,0x30,0)&&ok;
    return ok;
  }
  static bool lineHigh(Bus& bus){uint8_t value=0;return bus.pmRead(0x12,&value,1)&&(value&0x10);}
 private:
  static constexpr size_t PMCount=7,BMICount=9;
  static constexpr uint8_t PM[PMCount]={0x06,0x07,0x10,0x15,0x17,0x18,0x19};
  static constexpr uint8_t BMI[BMICount]={0x2F,0x40,0x53,0x55,0x56,0x57,0x58,0x7D,0x7C};
  uint8_t pm_[PMCount]{},bmi_[BMICount]{},feature_[4]{};
  bool captured_=false,featureCaptured_=false;
  static bool statusOK(uint8_t status){return (status&15)==1&&!(status&0x60);}
  static bool pmWrite(Bus& bus,uint8_t reg,uint8_t value){
    if(!bus.pmWrite(reg,value))return false;
    uint8_t read=0;return bus.pmRead(reg,&read,1)&&read==value;
  }
  static bool pmMask(Bus& bus,uint8_t reg,uint8_t mask,uint8_t value){
    uint8_t current=0;if(!bus.pmRead(reg,&current,1))return false;
    return pmWrite(bus,reg,uint8_t((current&~mask)|(value&mask)));
  }
  static bool bmiWrite(Bus& bus,uint8_t reg,const uint8_t* values,size_t length){
    if(!bus.bmiWrite(reg,values,length))return false;
    bus.pause(1);uint8_t read[4]={};
    if(length>sizeof(read)||!bus.bmiRead(reg,read,length))return false;
    for(size_t i=0;i<length;++i)if(read[i]!=values[i])return false;
    return true;
  }
  static bool bmiMask(Bus& bus,uint8_t reg,uint8_t mask,uint8_t value){
    uint8_t current=0;if(!bus.bmiRead(reg,&current,1))return false;
    const uint8_t updated=(current&~mask)|(value&mask);return bmiWrite(bus,reg,&updated,1);
  }
};
template<class Bus> constexpr uint8_t MotionWakeRegisters<Bus>::PM[];
template<class Bus> constexpr uint8_t MotionWakeRegisters<Bus>::BMI[];
