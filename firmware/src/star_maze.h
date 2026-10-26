#pragma once
#include <math.h>
#include <stdint.h>

// Fixed geometry and bounded 180 Hz steps keep the ball out of thin walls.
struct StarMaze {
  struct Wall {float x,y,w,h;};
  static constexpr float Left=76,Top=34,Right=230,Bottom=112,Radius=3.2f;
  static constexpr float ContactGap=.001f;
  Wall walls[4]{};
  float x=88,y=99,vx=0,vy=0,goalX=218,goalY=46;
  float accumulator=0;
  uint8_t level=0;
  uint32_t lastTick=0,elapsedMs=0;
  bool active=false,finished=false;
  static float clamp(float v,float lo,float hi){return fmaxf(lo,fminf(hi,v));}
  void preview(unsigned number){
    level=number%3;x=88;y=99;goalX=218;goalY=46;vx=vy=accumulator=0;elapsedMs=0;
    const Wall layout[]={{104,34,7,55},{132,60,7,52},{161,34,7,55},{190,60,7,52}};
    for(unsigned i=0;i<4;i++)walls[i]=layout[i];
    if(level==1){x=306-x;goalX=306-goalX;for(auto& w:walls)w.x=306-w.x-w.w;}
    if(level==2){y=146-y;goalY=146-goalY;for(auto& w:walls)w.y=146-w.y-w.h;}
    active=finished=false;
  }
  void begin(unsigned number,uint32_t now){preview(number);active=true;lastTick=now;}
  void cancel(){active=finished=false;vx=vy=0;}
  void move(bool horizontal,float dt){
    float& p=horizontal?x:y;float& speed=horizontal?vx:vy;
    p+=speed*dt;
    const float lo=(horizontal?Left:Top)+Radius,hi=(horizontal?Right:Bottom)-Radius;
    if(p<lo||p>hi){p=clamp(p,lo,hi);speed=0;}
    for(const auto& w:walls){
      const float cross=horizontal?y:x;
      const float edge=clamp(cross,horizontal?w.y:w.x,horizontal?w.y+w.h:w.x+w.w);
      const float delta=cross-edge;
      // The other axis may have just put us tangent to this face. Rounded
      // coordinates must not turn that contact into an end-of-wall collision.
      if(fabsf(delta)>=Radius-ContactGap)continue;
      const float gap=sqrtf(Radius*Radius-delta*delta),near=horizontal?w.x:w.y,far=horizontal?w.x+w.w:w.y+w.h;
      if(p<=near-gap||p>=far+gap)continue;
      p=speed>0?near-gap-ContactGap:speed<0?far+gap+ContactGap:p<(near+far)*.5f?near-gap-ContactGap:far+gap+ContactGap;
      speed=0;
    }
    p=clamp(p,lo,hi);
  }
  bool update(float tx,float ty,uint32_t now){
    if(!active)return false;
    const uint32_t elapsed=now-lastTick;lastTick=now;
    elapsedMs+=elapsed;
    accumulator+=float(elapsed>100?100:elapsed)*.001f;
    tx=isfinite(tx)?clamp(tx,-1,1):0;ty=isfinite(ty)?clamp(ty,-1,1):0;
    constexpr float dt=1.f/180;
    while(accumulator>=dt&&active){
      accumulator-=dt;
      const float drag=expf(-dt*2.8f);
      vx=clamp((vx+tx*175*dt)*drag,-58,58);vy=clamp((vy+ty*175*dt)*drag,-58,58);
      move(true,dt);move(false,dt);
      if(hypotf(x-goalX,y-goalY)<=6.5f-Radius*.35f){active=false;finished=true;vx=vy=0;return true;}
    }
    return false;
  }
};
