#include "../src/telekinetic_launch.h"
#include <assert.h>
#include <stdio.h>

int main(){
  const float angles[]={-24,-54,-16},powers[]={.75f,.75f,1};
  for(unsigned level=0;level<3;level++){
    TelekineticLaunch g;g.begin(level,0);g.aim(angles[level],powers[level]);assert(g.fire());assert(!g.fire());
    uint32_t now=0;unsigned hits=0;
    while(g.active&&now<6000){now+=10;hits+=g.update(now).hits;}
    assert(g.finished&&g.won&&g.shots==1&&g.litCount()==3&&hits==3);
    assert(!g.fire());if(level==1)assert(g.bounces>=2);
  }
  {
    TelekineticLaunch g;g.begin(0,0);g.aim(35,.25f);
    unsigned retained=0;uint32_t now=0;
    for(unsigned shot=0;shot<3;shot++){
      assert(g.fire());const float vx=g.vx,vy=g.vy;
      g.aim(-78,1);assert(g.vx==vx&&g.vy==vy&&g.angle==35);
      while(g.phase==TelekineticLaunch::Phase::Flight){now+=10;g.update(now);}
      assert(g.litCount()>=retained);retained=g.litCount();
    }
    assert(g.finished&&!g.won&&g.shots==3&&g.phase==TelekineticLaunch::Phase::Lost);assert(!g.fire());
  }
  {
    TelekineticLaunch g;g.begin(0,0);g.aim(NAN,NAN);assert(isfinite(g.angle)&&isfinite(g.power));
    g.aim(-100,2);assert(g.angle==-78&&g.power==1);g.aim(100,0);assert(g.angle==35&&g.power==.25f);
    assert(g.fire());g.update(100000);assert(g.flightTime<.09f); // no large-frame tunnelling
    g.cancel();assert(!g.active&&!g.finished);
  }
  puts("Launch: all three levels clearable, reflections, three-shot limit, ring retention, frozen flight aim");
}
