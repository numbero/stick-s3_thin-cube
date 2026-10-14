#include "../src/awake_power.h"
#include <assert.h>
#include <stdio.h>

int main(){
  {
    IdleDimPolicy p;
    assert(p.update(10000,10000,134,true,true)==134&&!p.dimmed);
    unsigned last=134;
    for(unsigned age=10001;age<=11500;age++){
      unsigned value=p.update(age,age,134,true,true);assert(value<=last&&value>=46);last=value;
    }
    assert(last==46&&p.dimmed);assert(p.update(20000,20000,134,true,true)==46);
    assert(p.update(20001,0,134,true,true)==134&&!p.dimmed); // Movement/button restores immediately.
    assert(p.update(21000,21000,134,false,true)==134&&!p.dimmed); // USB or unknown source stays bright.
    assert(p.update(22000,22000,134,true,true)==134); // Unplug grants a fresh window.
    assert(p.update(32500,32500,134,true,true)==46);
  }
  {
    IdleDimPolicy p;
    assert(p.update(20000,20000,134,true,true)==46);
    for(unsigned now=20001;now<25000;now+=100)assert(p.update(now,now,134,true,false)==134);
    assert(p.update(25000,25000,134,true,true)==134); // Voice/game/rotation/task/invalid IMU protection.
    assert(p.update(34000,34000,134,true,true)==134);
    assert(p.update(36500,36500,134,true,true)==46);
  }
  {
    for(unsigned saved=0;saved<256;saved++){
      IdleDimPolicy p;assert(p.update(20000,20000,saved,true,true)<=saved);
      if(saved<=12)assert(p.update(20000,20000,saved,true,true)==saved);
    }
    IdleDimPolicy p;p.blockedAt=0xfffffff0;
    assert(p.update(uint32_t(0xfffffff0+11500u),11500,100,true,true)==35);
    assert(p.update(20,0,100,true,true)==100);
  }
  {
    SleepFrameGate g;SleepFrameState s{70,0,0};
    assert(g.draw(1499,0,true,s));assert(g.draw(1500,0,true,s));
    for(unsigned now=1501;now<60000;now+=33)assert(!g.draw(now,0,true,s));
    s.battery=69;assert(g.draw(60000,0,true,s));assert(!g.draw(60001,0,true,s));
    for(unsigned bit=0;bit<14;bit++){
      s.flags^=1u<<bit;assert(g.draw(60002+bit,0,true,s));assert(!g.draw(60002+bit,0,true,s));
    }
    s.rotation=2;assert(g.draw(60100,0,true,s));assert(!g.draw(60101,0,true,s));
    assert(g.draw(60102,0,false,s)); // Wake, rotation, game, story or operation page must draw.
    assert(g.draw(60103,0,true,s));assert(!g.draw(60104,0,true,s)); // Invalidation cannot leave a stale page.
    assert(g.draw(70000,70000,true,s));assert(g.draw(71499,70000,true,s));assert(g.draw(71500,70000,true,s));
    assert(!g.draw(71501,70000,true,s));
    SleepFrameGate wrap;assert(wrap.draw(uint32_t(0xfffffff0+1500u),0xfffffff0,true,s));
    assert(!wrap.draw(uint32_t(0xfffffff0+1501u),0xfffffff0,true,s));
  }
  puts("battery dimming/fade, immediate activity restore, protected work and visible sleep-frame cache scenarios passed");
}
