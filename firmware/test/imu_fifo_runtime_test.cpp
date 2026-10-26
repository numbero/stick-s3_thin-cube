#include "../src/imu_fifo_runtime.h"
#include "../src/software_tap.h"
#include "../src/tap_diagnostic.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <initializer_list>
struct Bus {
  uint8_t regs[256]{};unsigned ops=0,flushes=0,fault=0;bool failAll=false;
  Bus(){regs[0x45]=0x9b;regs[0x48]=2;regs[0x49]=0x10;regs[0x40]=0xa9;regs[0x42]=0xe8;regs[0x7d]=6;}
  bool ok(){return !failAll&&++ops!=fault;}
  bool read(uint8_t r,uint8_t* v,unsigned n){if(!ok())return false;memcpy(v,regs+r,n);return true;}
  bool write(uint8_t r,const uint8_t* v,unsigned n){
    if(!ok())return false;
    if(r==0x7e){assert(*v==0xb0);++flushes;}else memcpy(regs+r,v,n);return true;
  }
  void pause(unsigned){}
};
int main(){
  Bus b;const Bus original=b;ImuFifoRuntime<Bus> fifo;
  // An idle main loop must never touch the FIFO, including after startup and
  // a previous talk context. Pose ODR, power and interrupt paths are separate.
  for(unsigned i=0;i<100;++i)assert(fifo.setSoftware(b,false));
  assert(b.ops==0&&fifo.restored);
  assert(fifo.setSoftware(b,true));const unsigned beginOps=b.ops,flushes=b.flushes;
  for(unsigned i=0;i<100;++i)assert(fifo.setSoftware(b,true));
  assert(b.ops==beginOps&&b.flushes==flushes&&fifo.software);
  assert(b.regs[0x40]==original.regs[0x40]&&b.regs[0x42]==original.regs[0x42]&&b.regs[0x7d]==original.regs[0x7d]);
  // A recording attached to voice survives voice exit, then releases the last
  // owner. The reverse attachment likewise preserves the recorder's samples.
  assert(fifo.setRecording(b,true));assert(fifo.setSoftware(b,false));
  assert(b.ops==beginOps&&fifo.recording&&!fifo.software&&!fifo.restored);
  assert(fifo.setSoftware(b,true));assert(b.ops==beginOps);
  assert(fifo.setRecording(b,false));assert(b.ops==beginOps&&fifo.software);
  assert(fifo.setSoftware(b,false));assert(fifo.restored);
  assert(memcmp(b.regs,original.regs,sizeof(b.regs))==0);
  const unsigned stoppedOps=b.ops;
  for(unsigned i=0;i<100;++i)assert(fifo.setSoftware(b,false));
  assert(b.ops==stoppedOps);
  // Re-entry flushes old data instead of feeding a previous scene's history
  // into the freshly reset double-tap detector.
  assert(fifo.setSoftware(b,true));assert(b.flushes>flushes);
  assert(fifo.setSoftware(b,false));
  for(unsigned fault=1;fault<=beginOps;++fault){
    Bus f;const Bus saved=f;ImuFifoRuntime<Bus> c;f.fault=fault;
    assert(!c.setSoftware(f,true));assert(!c.software&&!c.recording);
    f.fault=0;assert(c.setSoftware(f,false));assert(c.restored);
    assert(memcmp(f.regs,saved.regs,sizeof(f.regs))==0);
  }
  assert(fifo.setSoftware(b,true));b.failAll=true;
  assert(!fifo.setSoftware(b,false));assert(!fifo.software&&!fifo.restored);
  assert(!fifo.setRecording(b,true));assert(!fifo.recording);
  b.failAll=false;assert(fifo.setRecording(b,true));assert(fifo.recording);
  assert(fifo.setRecording(b,false));assert(fifo.restored);
  assert(memcmp(b.regs,original.regs,sizeof(b.regs))==0);
  // The unchanged 200 Hz detector must recognize the first double after the
  // existing 500 ms settling window, even across repeated FIFO teardown and
  // a uint32_t clock wrap. One impact from an old context cannot pair with a
  // new context's single impact.
  SoftwareTapDetector detector;TapParameters parameters;TapDiagnostic guard;
  for(uint32_t entry:{1000u,0xfffffff0u,500000u}){
    assert(fifo.setSoftware(b,true));detector.reset();guard.update(entry,true,false);
    unsigned accepted=0;
    for(unsigned age=0;age<1500;age+=5){
      const float acceleration=(age>=600&&age<615)||(age>=860&&age<875)?-1.f:0.f;
      if(detector.update(entry+age,acceleration,0,parameters)&&
         guard.event(entry+age)==TapDiagnostic::Reason::Accepted)++accepted;
    }
    assert(accepted==1);guard.update(entry+1500,false,false);
    assert(fifo.setSoftware(b,false));detector.reset();
  }
  detector.reset();
  for(unsigned age=0;age<800;age+=5)detector.update(age,age>=600&&age<615?-1.f:0.f,0,parameters);
  detector.reset();unsigned stale=0;
  for(unsigned age=0;age<1500;age+=5)if(detector.update(10000+age,age>=600&&age<615?-1.f:0.f,0,parameters))++stale;
  assert(stale==0);
  puts("FIFO idle, context re-entry, shared recording, register restoration and failure recovery passed");
}
