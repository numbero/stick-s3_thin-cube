#include "../src/walkie_keys.h"
#include "../src/button_gestures.h"
#include <cassert>
#include <cstdio>

int main(){
  WalkieKeys k;
  k.update(true,true,true);assert(!k.heldA&&!k.heldB);
  k.update(true,false,false);k.update(true,true,true);assert(k.heldA&&k.heldB);
  k.update(true,false,true);assert(!k.heldA&&k.heldB);
  k.update(false,false,true);assert(!k.heldB);
  k.update(true,false,true);assert(!k.heldB);
  k.update(true,false,false);k.update(true,false,true);assert(k.heldB);
  k.update(true,true,false);assert(k.heldA&&!k.heldB);
  k.update(false,true,false);k.update(true,true,false);assert(!k.heldA);
  // UI filtering is not evidence of a physical release during negotiation.
  k={};k.update(true,true,true,false);k.update(true,true,true,true);
  assert(!k.heldA&&!k.heldB);
  k.update(true,false,false);k.update(true,true,true);assert(k.heldA&&k.heldB);
  k.update(true,true,true,false);k.update(true,true,true,true);assert(!k.heldA&&k.heldB);
  k.update(true,false,true);k.update(true,true,true);assert(k.heldA&&k.heldB);

  using A=ButtonGestures::Action;ButtonGestures g;
  g.update(0,false,false,false,true,true,true);
  assert(g.update(1,true,true,false,true,false,true)==A::None);assert(!g.suppressA);
  assert(g.update(4000,true,true,false,true,false,true)==A::None);
  assert(!g.switchReady&&!g.comboReady);
  assert(g.update(4001,false,false,true,true,true,false)==A::None);
  g.update(4002,false,true,false,true,true);
  assert(g.update(4100,false,false,true,true,true)==A::Mute);
  // Exit while B is held, then hold beyond the old system threshold.
  g.update(5000,false,true,false,true,false,true);
  assert(g.update(9000,false,true,false,true,true,false)==A::None);
  assert(g.update(10000,false,false,false,true,true,false)==A::None);
  g.update(11000,false,true,false,true,true);
  assert(g.update(14000,false,false,false,true,true)==A::Switch);
  // Starting outside then entering cancels the old long-press intent.
  g={};g.update(0,false,true,false,true,true);
  g.update(3000,false,true,false,true,false,true);
  assert(g.update(3100,false,false,false,true,false,true)==A::None);
  g.update(3200,false,true,false,false,false);
  assert(g.update(3300,false,false,true,true,true)==A::None);

  WalkieKeyQueue q;
  assert(q.push(true,'a'));assert(q.push(true,'b'));assert(q.push(false,'a'));
  const auto first=q.front()->seq;q.acknowledge(first+1);assert(q.count==3);
  q.acknowledge(first);assert(q.front()->key=='b');
  q.acknowledge(first);assert(q.count==2);
  q.clear();for(unsigned i=0;i<32;++i)assert(q.push(i%2,'b'));
  assert(!q.push(true,'a'));assert(q.count==32);
  q.clear();q.sequence=UINT32_MAX;assert(q.push(true,'b'));assert(q.front()->seq==0);
  q.acknowledge(0);assert(!q.front());
  puts("walkie A/B ownership, context routing and reliable edges passed");
}
