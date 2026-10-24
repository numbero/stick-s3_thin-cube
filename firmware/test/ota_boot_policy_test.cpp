#include "../src/ota_boot_policy.h"
#include <cassert>
#include <cstdio>

void sample(OtaBootPolicy& policy,uint32_t start,uint32_t duration){
  for(uint32_t elapsed=0;elapsed<=duration;elapsed+=50)policy.update(start+elapsed,true);
}
int main(){
  OtaBootPolicy p;
  sample(p,1000,14950);assert(p.stableMs()==14950);
  assert(!p.update(15999,true));assert(p.update(16000,true));
  assert(p.stableMs()==15000);
  assert(!p.update(16050,false)&&p.stableMs()==0);
  sample(p,20000,5000);assert(!p.update(25251,true)&&p.stableMs()==0);
  sample(p,25251,14950);assert(!p.update(40250,true));assert(p.update(40251,true));
  p.reset();sample(p,0,14950);assert(p.update(15200,true)); // 250ms observation gap is allowed.
  assert(p.stableMs()==15000); // The stable duration reaches its cap.
  p.reset();sample(p,0xfffff000u,14950);assert(p.update(0xfffff000u+15000u,true));
  std::puts("OTA continuous health window: prerequisites, exact 15s boundary, failed health, observation gap, cap and uptime wrap passed");
}
