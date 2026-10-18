#include "../src/imu_fifo_capture.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct Bus {
  uint8_t regs[256]{};unsigned ops=0,fault=0;
  Bus(){regs[0x45]=0x9b;regs[0x48]=2;regs[0x49]=0x10;regs[0x40]=0xa9;regs[0x7d]=6;}
  bool ok(){return ++ops!=fault;}
  bool read(uint8_t r,uint8_t* v,unsigned n){if(!ok())return false;memcpy(v,regs+r,n);return true;}
  bool write(uint8_t r,const uint8_t* v,unsigned n){if(r!=0x7e)memcpy(regs+r,v,n);else assert(*v==0xb0);return ok();}
  void pause(unsigned){}
};
int main(){
  unsigned operations=0;
  {Bus b;const Bus original=b;ImuFifoCapture<Bus> c;assert(c.begin(b));operations=b.ops;
   assert(b.regs[0x45]==0x8b&&b.regs[0x48]==0&&b.regs[0x49]==0x40);
   assert(b.regs[0x40]==0xa9&&b.regs[0x7d]==6);assert(c.restore(b));
   assert(memcmp(b.regs,original.regs,sizeof(b.regs))==0);}
  for(unsigned i=1;i<=operations;++i){Bus b;const Bus original=b;b.fault=i;ImuFifoCapture<Bus> c;
   assert(!c.begin(b));b.fault=0;assert(c.restore(b));assert(memcmp(b.regs,original.regs,sizeof(b.regs))==0);}
  {Bus b;b.regs[0x49]=0x40;const Bus original=b;ImuFifoCapture<Bus> c;assert(!c.begin(b));assert(c.restore(b));
   assert(memcmp(b.regs,original.regs,sizeof(b.regs))==0);}
  printf("FIFO capture config, restoration, %u transient failure points and occupied FIFO rejection passed\n",operations);
}
