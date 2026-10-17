#include "../src/button_gestures.h"
#include "../src/pairing_policy.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

int main(){
  using Action=ButtonGestures::Action;
  for(unsigned order=0;order<3;order++){
    for(uint32_t delay: {1u,5u,100u}){
      ButtonGestures buttons;
      PairingPolicy pair;
      assert(buttons.update(1000,true,true,false,true,true)==Action::None);
      assert(buttons.update(4000,true,true,false,true,true)==Action::None);
      assert(buttons.comboReady);
      if(order)assert(buttons.update(4001,order==1,order==2,false,true,true)==Action::None);
      // uiTick opens pairing after loop() sampled its clock. deviceTick must
      // not expire it immediately with the older sample from that same loop.
      const uint32_t loopAt=4010,openedAt=loopAt+delay;
      assert(buttons.update(loopAt,false,false,true,true,true)==Action::Pair);
      assert(buttons.suppressA);
      pair.offer(openedAt,true);
      assert(!pair.expire(loopAt,true));
      assert(pair.visible&&pair.active(loopAt));
      assert(buttons.update(loopAt+1,false,false,true,true,true)==Action::None);
      assert(pair.active(openedAt+60000));
      assert(!pair.expire(openedAt+PairingPolicy::WindowMs-1,true));
      assert(pair.expire(openedAt+PairingPolicy::WindowMs,true));
      assert(!pair.visible);
    }
  }
  {
    // A refresh or a network pairing request can also record a newer time
    // while the main loop still holds its earlier timestamp.
    PairingPolicy pair;pair.offer(1000,true);
    pair.offer(2001,true);
    assert(!pair.expire(2000,true));
    assert(pair.active(2000)&&!pair.begin(2000));
    assert(pair.attempt(2002));
    assert(!pair.attempt(2500));
    assert(pair.attempt(3002));
    pair.close();assert(!pair.visible&&!pair.active(3003));
  }
  {
    // Preserve expiry across millis rollover, including a stale sample just
    // before rollover for a window opened just after it.
    PairingPolicy pair;pair.offer(2,true);
    assert(!pair.expire(0xfffffffeu,true));
    assert(pair.active(0xfffffffeu));
    assert(pair.expire(2+PairingPolicy::WindowMs,true));
    const uint32_t openedAt=0xfffffff0u;
    pair.offer(openedAt,true);
    assert(pair.active(20));
    assert(!pair.expire(openedAt+PairingPolicy::WindowMs-1,true));
    assert(pair.expire(openedAt+PairingPolicy::WindowMs,true));
  }
  {
    PairingPolicy pair;pair.offer(100,false);
    assert(!pair.expire(1000000,false)&&pair.visible&&!pair.timed);
    assert(pair.begin(1000000));
    assert(pair.expire(1000000+PairingPolicy::WindowMs,false));
    assert(pair.visible&&!pair.timed);
  }
  puts("re-pair release orders, stale loop clocks, refresh, expiry and rollover passed");
}
