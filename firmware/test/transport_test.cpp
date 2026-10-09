#include "../src/transport_policy.h"
#include "../src/button_gestures.h"
#include "../src/ble_framing.h"
#include <cassert>
#include <cstdio>
int main(){
  TransportRecord r;assert(r.valid());assert(r.begin(TransportMode::BLE,"one","mac"));
  assert(!r.begin(TransportMode::WiFi,"two","button"));assert(r.boot()==TransportMode::BLE);
  assert(!r.commit("bad"));assert(!r.timedOut(60099,100));assert(r.timedOut(60100,100));
  auto interrupted=r;assert(interrupted.boot()==TransportMode::WiFi);assert(interrupted.phase==SwitchPhase::RolledBack);
  assert(r.commit("one"));assert(r.commit("one"));assert(r.boot()==TransportMode::BLE);
  assert(r.begin(TransportMode::WiFi,"two","button"));assert(r.boot()==TransportMode::WiFi);
  r.rollback();assert(r.mode==TransportMode::BLE);assert(!r.commit("two"));
  r.phase=SwitchPhase::Trial;assert(r.timedOut(60000-16,0xfffffff0));
  using A=ButtonGestures::Action;ButtonGestures g;
  assert(g.update(100,false,true,false,true,true)==A::None);
  assert(g.update(3100,false,true,false,true,true)==A::None);assert(g.switchReady);
  assert(g.update(3101,false,false,false,true,true)==A::Switch);
  assert(g.update(3102,false,false,true,true,true)==A::None);
  g={};g.update(1,false,true,false,true,true);assert(g.update(200,false,false,true,true,true)==A::Mute);
  g={};g.update(0,false,true,false,true,true);g.update(3000,false,true,false,true,true);
  g.update(3100,true,true,false,true,true);assert(!g.switchReady);
  g.update(6099,true,true,false,true,true);g.update(6100,true,true,false,true,true);
  assert(g.update(6101,false,true,false,true,true)==A::None);
  assert(g.update(6200,false,false,false,true,true)==A::Pair);
  g={};g.update(0,true,true,false,true,true);g.update(2999,false,true,false,true,true);
  g.update(4000,false,true,false,true,true);assert(g.update(5000,false,false,false,true,true)==A::None);
  g={};g.update(0,true,true,false,true,false);g.update(3000,true,true,false,true,true);
  assert(g.update(3001,false,false,false,true,true)==A::None);
  g={};g.update(0,false,true,false,false,true);assert(g.update(4000,false,false,false,true,true)==A::None);
  g={};g.update(0xfffffff0,false,true,false,true,true);g.update(2984,false,true,false,true,true);
  assert(g.update(2985,false,false,false,true,true)==A::Switch);
  g={};g.update(0,false,true,false,true,true);assert(g.update(3000,false,false,false,true,true)==A::Switch);
  g={};g.update(0,true,true,false,true,true);assert(g.update(3000,false,false,false,true,true)==A::Pair);
  g={};g.update(0,true,true,false,true,true);g.update(2000,true,false,false,true,true);
  g.update(3000,true,true,false,true,true);assert(g.update(4000,false,false,false,true,true)==A::None);
  BLEControlAssembly assembly;
  const uint8_t first[]={'T','C',0,1,0,0,0,6,'a','b','c'};
  const uint8_t last[]={'T','C',0,1,0,3,0,6,'d','e','f'};
  assert(!assembly.accept(last,sizeof(last),0));
  assert(!assembly.accept(first,sizeof(first),0));assert(assembly.accept(last,sizeof(last),2000));assert(!strcmp(assembly.data,"abcdef"));
  assembly.reset();assert(!assembly.accept(first,sizeof(first),0));assert(!assembly.accept(last,sizeof(last),2001));
  uint8_t malformed[sizeof(first)];memcpy(malformed,first,sizeof(first));malformed[6]=4;
  assert(!assembly.accept(malformed,sizeof(malformed),3000));assert(assembly.total==0);
  assert(!assembly.accept(first,8,4000));
  puts("transport transactions and button gestures passed");
}
