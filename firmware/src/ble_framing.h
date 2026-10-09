#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
// Client writes are ordered (.withResponse) and cannot interleave messages.
struct BLEControlAssembly {
  uint16_t id=0,total=0,offset=0;
  uint32_t started=0;
  char data[769]={};
  void reset(){total=offset=0;}
  bool accept(const uint8_t* bytes,size_t size,uint32_t now){
    if(total&&uint32_t(now-started)>2000)reset();
    if(size<=8||size>512||bytes[0]!='T'||bytes[1]!='C')return false;
    auto be=[](const uint8_t* b){return uint16_t(uint16_t(b[0])<<8|b[1]);};
    const uint16_t message=be(bytes+2),at=be(bytes+4),length=be(bytes+6);
    const size_t count=size-8;
    if(!length||length>768||size_t(at)+count>length){reset();return false;}
    if(at==0){id=message;total=length;offset=0;started=now;}
    if(id!=message||total!=length||offset!=at){reset();return false;}
    memcpy(data+at,bytes+8,count);offset+=count;
    if(offset!=total)return false;
    data[total]=0;return true;
  }
};
