#include "../src/connection_timing.h"
#include <assert.h>
#include <stdio.h>
int main(){
  ConnectionTiming t;uint32_t elapsed=0,step=0;
  assert(!t.mark(ConnectionStage::Channel,10,elapsed,step));
  t.begin(100);assert(t.mark(ConnectionStage::Channel,125,elapsed,step));assert(elapsed==25&&step==25);
  assert(!t.mark(ConnectionStage::Channel,200,elapsed,step));
  assert(t.mark(ConnectionStage::Secured,225,elapsed,step));assert(elapsed==125&&step==100);
  assert(!t.mark(ConnectionStage::Count,230,elapsed,step));
  t.begin(0xfffffff0u);assert(t.mark(ConnectionStage::Channel,0x10,elapsed,step));assert(elapsed==32&&step==32);
  assert(t.mark(ConnectionStage::Channel,0x20,elapsed,step)==false);
  t.begin(300);assert(t.mark(ConnectionStage::Channel,301,elapsed,step));assert(elapsed==1);
  puts("connection milestones: duplicate suppression, new attempts and clock wrap passed");
}
