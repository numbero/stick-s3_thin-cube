#include "../src/star_catch.h"
#include <assert.h>
#include <stdio.h>

unsigned visible(const StarCatch& game){unsigned n=0;for(const auto& star:game.stars)n+=star.active;return n;}
int main(){
  {
    StarCatch g;assert(!g.active&&g.leftMs(0)==20000);assert(g.begin(7,100));
    assert(!g.begin(0,200)&&g.baseline==7&&g.rounds==1&&visible(g)==1);
    assert(g.score==0&&g.combo==0&&g.stars[0].x==g.trayX());
    unsigned caught=0;for(uint32_t t=120;t<2600;t+=20)caught+=g.update(7,true,t).caught;
    assert(caught==1&&g.score==1&&g.combo==1&&g.bestCombo==1&&g.active);
  }
  {
    StarCatch g;g.begin(0,100);unsigned missed=0;
    for(uint32_t t=120;t<2600;t+=20)missed+=g.update(15,true,t).missed;
    assert(missed==1&&g.score==0&&g.combo==0&&g.active&&!g.finished);
  }
  {
    StarCatch g;g.begin(7,100);g.update(9.9f,true,190);assert(g.offset==0);
    g.update(22,true,280);assert(g.offset==1);g.update(-8,true,370);assert(g.offset==-1);
    assert(g.trayX()-g.TrayWidth/2>=82&&g.trayX()+g.TrayWidth/2<=232);
    g.update(-90,true,460);assert(g.offset==-1);
  }
  {
    StarCatch g;g.begin(0,100);assert(!g.update(0,true,20099).finished&&g.active);
    const auto score=g.score;assert(g.leftMs(20099)==1);
    assert(g.update(0,true,20100).finished&&g.finished&&!g.active&&visible(g)==0);
    assert(g.score==score&&g.leftMs(20100)==0);
    assert(!g.update(0,true,30000).finished&&g.score==score);
  }
  {
    StarCatch g;g.begin(0,100);uint32_t t=120;
    for(;g.score==0&&t<3000;t+=20)g.update(0,true,t);
    assert(g.score==1&&g.combo==1);
    for(;t<4600;t+=20)g.update(15,true,t);
    assert(g.active&&g.score==1&&g.combo==0&&g.bestCombo==1);
  }
  {
    // Contact tolerance has a real boundary and a miss never ends the round.
    StarCatch g;g.begin(0,100);g.update(-15,true,190);
    g.stars[0]={g.trayX()+16,99,true};g.stars[1].active=false;
    assert(g.update(-15,true,230).caught==1&&g.score==1);
    g.stars[0]={g.trayX()+16.1f,99,true};
    assert(g.update(-15,true,270).missed==1&&g.combo==0&&g.active);
  }
  {
    // A star crossing the tray in a delayed frame cannot tunnel through it.
    StarCatch g;g.begin(0,100);g.stars[0]={g.trayX(),90,true};
    assert(g.update(0,true,600).caught==1);
    assert(g.update(0,true,620).caught==0&&g.score==1);
  }
  {
    StarCatch g;assert(!g.begin(NAN,100));g.begin(0,200);g.update(15,true,290);
    const auto offset=g.offset;g.update(NAN,true,380);assert(g.offset==offset&&isfinite(g.trayX()));
    g.update(0,false,400);assert(!g.active&&!g.finished&&visible(g)==0);
    assert(!g.update(0,true,30000).finished&&g.score==0);
  }
  {
    // Complete rounds with all lane sequences using the actual tilt controller.
    StarCatch g;unsigned totalCaught=0;
    for(unsigned round=0;round<8;round++){
      const uint32_t start=round*21000;assert(g.begin(5,start));unsigned misses=0;
      for(uint32_t elapsed=20;elapsed<=g.RoundMs;elapsed+=20){
        const StarCatch::Star* oldest=nullptr;
        for(const auto& star:g.stars)if(star.active&&(!oldest||star.y>oldest->y))oldest=&star;
        const float desired=oldest?(oldest->x-g.CenterX)/g.SpanX:0;
        const float angle=desired==0?0:desired*12+(desired<0?-3:3);
        misses+=g.update(5+angle,true,start+elapsed).missed;
        assert(visible(g)<=g.Capacity);
        for(const auto& star:g.stars)if(star.active){
          assert(star.x>=g.CenterX-g.SpanX&&star.x<=g.CenterX+g.SpanX);
          assert(star.y>=g.StartY&&star.y<g.CatchY);
        }
      }
      assert(g.finished&&!g.active&&misses==0&&g.score>=10&&g.combo==g.score);
      totalCaught+=g.score;
    }
    assert(totalCaught>=80);
  }
  {
    StarCatch g;const uint32_t start=0xfffffff0u;g.begin(0,start);
    g.update(0,true,start+90);assert(g.active&&g.leftMs(start+90)==19910);
    assert(!g.update(0,true,start+19999).finished);
    assert(g.update(0,true,start+20000).finished&&!g.active);
  }
  {
    StarCatch g;g.begin(0,100);g.update(0,true,2500);assert(g.score==1);
    g.cancel();assert(!g.active&&!g.finished&&visible(g)==0&&g.score==1);
    assert(g.begin(8,3000)&&g.rounds==2&&g.score==0&&g.combo==0&&g.offset==0);
    assert(g.baseline==8&&visible(g)==1);
  }
  {
    StarCatch g;g.begin(0,100);g.update(0,true,1800);assert(visible(g)==2);
    const auto event=g.update(15,true,10000);assert(event.caught+event.missed==2);
    assert(visible(g)==1&&g.active); // No backlog burst after a long delayed tick.
  }
  puts("12 star catch contact, reachability, round timing, combo, capacity and wrap scenarios passed");
}
