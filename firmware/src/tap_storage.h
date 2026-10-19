#pragma once
#include "tap_parameters.h"
#include <stddef.h>
#include <string.h>

// Stable, versioned wire format; never persist compiler struct padding.
constexpr size_t TapRecordSize=40;
enum class TapStoredRead { Missing, Found, Failed };
enum class TapLoadState { Defaults, Saved, Invalid, Unavailable };
inline uint32_t tapRecordCRC(const uint8_t* bytes,size_t size){
  uint32_t crc=0xffffffff;
  for(size_t i=0;i<size;++i){crc^=bytes[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}
  return ~crc;
}
inline bool tapEncodeRecord(const TapParameters& p,uint8_t (&bytes)[TapRecordSize]){
  if(!p.valid())return false;
  memcpy(bytes,"TCTP",4);bytes[4]=1;
  const bool flags[]={p.enabled,p.single,p.doubleTap,p.triple,p.filtered,p.wait,p.manual,p.software};
  bytes[5]=0;for(unsigned i=0;i<8;++i)if(flags[i])bytes[5]|=1u<<i;
  const uint16_t values[]={p.axis,p.sensitivity,p.windowMs,p.quietMs,p.settleMs,p.buttonGuardMs,p.cooldownMs,
    p.softThresholdMg,p.softReboundMs,p.softWindowMs,p.softGyroDps,p.softMotionGuardMs,p.softBaselineMs,p.softGyroFilterMs,p.softRatioPercent};
  for(unsigned i=0;i<15;++i){bytes[6+i*2]=uint8_t(values[i]);bytes[7+i*2]=uint8_t(values[i]>>8);}
  const uint32_t crc=tapRecordCRC(bytes,TapRecordSize-4);
  for(unsigned i=0;i<4;++i)bytes[TapRecordSize-4+i]=uint8_t(crc>>(8*i));
  return true;
}
inline bool tapDecodeRecord(const uint8_t* bytes,size_t size,TapParameters& out){
  if(size!=TapRecordSize||memcmp(bytes,"TCTP",4)||bytes[4]!=1)return false;
  uint32_t expected=0;for(unsigned i=0;i<4;++i)expected|=uint32_t(bytes[TapRecordSize-4+i])<<(8*i);
  if(tapRecordCRC(bytes,TapRecordSize-4)!=expected)return false;
  TapParameters p;
  bool* flags[]={&p.enabled,&p.single,&p.doubleTap,&p.triple,&p.filtered,&p.wait,&p.manual,&p.software};
  for(unsigned i=0;i<8;++i)*flags[i]=(bytes[5]&(1u<<i))!=0;
  uint16_t* values[]={&p.axis,&p.sensitivity,&p.windowMs,&p.quietMs,&p.settleMs,&p.buttonGuardMs,&p.cooldownMs,
    &p.softThresholdMg,&p.softReboundMs,&p.softWindowMs,&p.softGyroDps,&p.softMotionGuardMs,&p.softBaselineMs,&p.softGyroFilterMs,&p.softRatioPercent};
  for(unsigned i=0;i<15;++i)*values[i]=uint16_t(bytes[6+i*2])|(uint16_t(bytes[7+i*2])<<8);
  if(!p.valid())return false;
  out=p;return true;
}
template<class Store> TapLoadState tapLoadSettings(Store& store,TapParameters& out){
  uint8_t bytes[TapRecordSize]{};size_t size=0;
  const auto read=store.read(bytes,sizeof(bytes),size);
  if(read==TapStoredRead::Missing)return TapLoadState::Defaults;
  if(read==TapStoredRead::Failed)return TapLoadState::Unavailable;
  return tapDecodeRecord(bytes,size,out)?TapLoadState::Saved:TapLoadState::Invalid;
}
template<class Store> bool tapSaveSettings(Store& store,const TapParameters& p){
  uint8_t wanted[TapRecordSize]{},actual[TapRecordSize]{};size_t size=0;
  if(!tapEncodeRecord(p,wanted))return false;
  const auto read=store.read(actual,sizeof(actual),size);
  if(read==TapStoredRead::Failed)return false;
  // Avoid flash writes when the complete committed record already matches.
  if(read==TapStoredRead::Found&&size==sizeof(wanted)&&!memcmp(wanted,actual,sizeof(wanted)))return true;
  if(!store.write(wanted,sizeof(wanted)))return false;
  size=0;
  return store.read(actual,sizeof(actual),size)==TapStoredRead::Found&&size==sizeof(wanted)&&!memcmp(wanted,actual,sizeof(wanted));
}
