#include "../src/ble_advertising_policy.h"
#include <assert.h>
#include <stdio.h>
using Policy=BLEAdvertisingPolicy;
using Op=Policy::Operation;
void start(Policy& p,uint32_t now,bool fast){
  const auto op=p.next(now,true,false,false);assert(op==(fast?Op::StartFast:Op::StartSlow));
  p.issued(op,now,true);assert(p.next(now+1,true,false,false)==Op::None);
  p.completed(true,true,now+2);assert(p.state==Policy::State::Running&&p.fast==fast);
}
int main(){
  Policy p;p.observe(100,0);assert(p.next(100,false,false,false)==Op::None);
  start(p,100,true);
  assert(p.next(60099,true,false,false)==Op::None);
  assert(p.next(60100,true,false,false)==Op::Stop);
  p.issued(Op::Stop,60100,true);
  // A button press during stop must restart fast, rather than committing the
  // old slow target after its asynchronous acknowledgement.
  p.activity(60101);assert(p.next(60102,true,false,false)==Op::None);
  p.completed(false,true,60103);start(p,60104,true);
  assert(p.next(120101,true,false,false)==Op::Stop);p.issued(Op::Stop,120101,true);
  p.completed(false,true,120102);start(p,120103,false);
  for(unsigned i=0;i<100;++i)assert(p.next(121000+i,true,false,false)==Op::None);
  // Setup/pairing wins over the offline timeout and grants a fast window after
  // its exit, without restarting an already-fast advertisement every loop.
  assert(p.next(122000,true,false,true)==Op::Stop);p.issued(Op::Stop,122000,true);
  p.completed(false,true,122001);start(p,122002,true);
  assert(p.next(300000,true,false,true)==Op::None);
  assert(p.next(359999,true,false,false)==Op::None);
  // Connection auto-stops advertising. Delayed completion of a previous
  // start/stop cannot turn it back on or erase a new connection's fast window.
  p.observe(360000,1);assert(p.next(360000,true,true,false)==Op::None);
  p.observe(361000,2);start(p,361000,true);
  Policy late;late.observe(0,0);late.issued(Op::StartFast,0,true);
  late.observe(1,1);late.observe(2,2);late.completed(true,true,3);
  assert(late.state==Policy::State::Stopped);start(late,4,true);
  // Immediate API errors and asynchronous errors back off, then retry.
  Policy fail;fail.observe(0,0);fail.issued(Op::StartFast,0,false);
  assert(fail.next(999,true,false,false)==Op::None);
  assert(fail.next(1000,true,false,false)==Op::StartFast);
  fail.issued(Op::StartFast,1000,true);fail.completed(true,false,1001);
  assert(fail.next(2000,true,false,false)==Op::None);start(fail,2001,true);
  auto op=fail.next(60000,true,false,false);assert(op==Op::Stop);
  fail.issued(op,60000,true);fail.completed(false,false,60001);
  assert(fail.next(61000,true,false,false)==Op::None);
  assert(fail.next(61001,true,false,false)==Op::Stop);
  // uint32_t uptime wrap preserves the 60-second window.
  Policy wrap;wrap.observe(0xfffffff0u,0);start(wrap,0xfffffff0u,true);
  assert(wrap.next(59983,true,false,false)==Op::None);
  assert(wrap.next(59984,true,false,false)==Op::Stop);
  static_assert(Policy::FastMin*5/8==30&&Policy::FastMax*5/8==60);
  static_assert(Policy::SlowMin*5/8==500&&Policy::SlowMax*5/8==1000);
  puts("BLE fast/slow windows, activity, pairing, async races, retries and uptime wrap passed");
}
