#include "../src/motion_wake_registers.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct Bus {
  uint8_t pm[256]{},bmi[256]{},features[8][16]{};
  unsigned operations=0,failAt=0;
  bool lowLine=false,badStatus=false,shutdown=false,blockAPSRead=false;
  Bus(){
    pm[6]=0x17;pm[7]=0;pm[0x12]=0x10;pm[0x15]=2;pm[0x17]=3;
    bmi[0]=0x24;bmi[0x21]=1;bmi[0x2f]=4;bmi[0x40]=0xA8;bmi[0x53]=2;
    bmi[0x55]=0;bmi[0x56]=0;bmi[0x57]=0;bmi[0x58]=0xff;bmi[0x7c]=0;bmi[0x7d]=0x0e;
    features[1][6]=5;features[1][7]=0xE0;features[1][8]=0xAA;features[1][9]=0x38;features[1][12]=0x5a;features[5][0]=0x42;
  }
  bool allowed(){return ++operations!=failAt;}
  bool pmRead(uint8_t r,uint8_t* p,size_t n){if(!allowed())return false;memcpy(p,pm+r,n);if(r==0x12&&lowLine)*p=0;return true;}
  bool pmWrite(uint8_t r,uint8_t v){pm[r]=v;if(r==0x0c)shutdown=true;return allowed();}
  bool bmiRead(uint8_t r,uint8_t* p,size_t n){
    if(r==0x7c&&blockAPSRead)return false;
    if(!allowed())return false;
    if(r>=0x30&&r+n<=0x40)memcpy(p,features[bmi[0x2f]&7]+r-0x30,n);else memcpy(p,bmi+r,n);
    if(r==0x21&&badStatus)*p=0x61;return true;
  }
  bool bmiWrite(uint8_t r,const uint8_t* p,size_t n){
    if(r>=0x30&&r+n<=0x40){assert(!(bmi[0x7c]&1)&&r==0x36&&n==4);memcpy(features[bmi[0x2f]&7]+r-0x30,p,n);}
    else memcpy(bmi+r,p,n);return allowed();
  }
  void pause(unsigned){}
};
int main(){
  unsigned total=0;
  {
    Bus b;const Bus original=b;MotionWakeRegisters<Bus,true> t;assert(t.prepare(b));total=b.operations;
    assert(b.features[1][9]==0xb8&&b.features[1][12]==0x5a&&b.features[5][0]==0x42&&b.bmi[0x7d]==4&&b.bmi[0x58]==0);
    assert((b.pm[0x18]&0x18)==0x10&&(b.pm[7]&0x30)==0x30);
    assert(b.pm[6]==original.pm[6]&&b.pm[8]==original.pm[8]&&!b.shutdown);
    assert(t.restore(b));assert(memcmp(b.pm,original.pm,sizeof(b.pm))==0);
    assert(memcmp(b.bmi,original.bmi,sizeof(b.bmi))==0&&memcmp(b.features,original.features,sizeof(b.features))==0);
  }
  // Every transient read/write failure (including writes that applied before
  // reporting failure) must leave no motion wake/HOLD and restore the normal IMU.
  for(unsigned fault=1;fault<=total;++fault){
    Bus b;const Bus original=b;b.failAt=fault;MotionWakeRegisters<Bus,true> t;
    assert(!t.prepare(b));b.failAt=0;assert(t.restore(b));
    assert(!b.shutdown&&!(b.pm[0x18]&0x10)&&!(b.pm[7]&0x30));
    assert(memcmp(b.pm,original.pm,sizeof(b.pm))==0&&memcmp(b.bmi,original.bmi,sizeof(b.bmi))==0);
    assert(memcmp(b.features,original.features,sizeof(b.features))==0);
  }
  {
    Bus b;b.lowLine=true;MotionWakeRegisters<Bus,true> t;assert(!t.prepare(b));assert(t.restore(b));
    b.lowLine=false;b.badStatus=true;assert(!t.prepare(b));assert(t.restore(b));assert(!b.shutdown);
  }
  {
    Bus b;MotionWakeRegisters<Bus,true> t;assert(t.prepare(b));b.blockAPSRead=true;
    assert(!t.restore(b));assert(!(b.pm[0x18]&0x10)&&!(b.pm[7]&0x30));
    assert(b.bmi[0x58]==0xff&&b.bmi[0x7d]==0x0e&&!b.shutdown);
  }
  {
    Bus b;b.pm[7]=0xc4;b.pm[6]=0x1f;const Bus original=b;MotionWakeRegisters<Bus,true> t;
    assert(t.prepare(b));assert(b.pm[7]==0x30&&!(b.pm[6]&8)&&(b.pm[6]&1));
    assert(t.restore(b));assert(memcmp(b.pm,original.pm,sizeof(b.pm))==0);
  }
  printf("Legacy motion wake layout, neighbour preservation, %u transient fault rollback points, asserted IRQ and chip-status rejection passed\n",total);
}
