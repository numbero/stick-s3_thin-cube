#include "../src/gesture_events.h"
#include <assert.h>
#include <stdio.h>
int main(){
  GestureEvents q;assert(q.push(100));assert(q.push(120));const auto first=q.front(130)->seq;
  q.acknowledge(first+1);assert(q.count==2);q.acknowledge(first);assert(q.count==1);
  q.acknowledge(first);assert(q.count==1);assert(q.front(870));assert(!q.front(871));
  q.clear();for(int i=0;i<4;++i)assert(q.push(1000));assert(!q.push(1000));assert(!q.front(1751));
  q.sequence=UINT32_MAX;assert(q.push(UINT32_MAX-100));assert(q.front(649)->seq==1);assert(!q.front(650));
  assert(q.push(1000));q.clear();assert(!q.front(1001));assert(q.push(1002));assert(q.front(1002)->seq==3);
  puts("gesture queue: exact acknowledgements, retransmission, expiry, capacity, context clear and wrap passed");
}
