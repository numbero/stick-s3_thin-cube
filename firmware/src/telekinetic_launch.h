#pragma once
#include <math.h>
#include <stdint.h>

struct TelekineticLaunch {
  enum class Phase { Aim,Flight,Won,Lost };
  struct Wall {float x,y,w,h;};
  struct Ring {float x,y;bool lit;Ring(float px=0,float py=0,bool on=false):x(px),y(py),lit(on){}};
  struct Point {float x,y;};
  struct Events {unsigned hits=0;bool finished=false;};
  static constexpr float Left=80,Top=38,Right=231,Bottom=112,Radius=2.2f;
  Wall walls[2]{};Ring rings[3]{};Point trail[24]{};
  float x=93,y=98,vx=0,vy=0,angle=-23,power=.65f,flightTime=0,accumulator=0;
  unsigned shots=0,shotHits=0,bestCombo=0,bounces=0,trailCount=0;
  uint8_t level=0;
  uint32_t lastTick=0;
  bool active=false,finished=false,won=false;
  Phase phase=Phase::Aim;
  static float clamp(float v,float lo,float hi){return fmaxf(lo,fminf(hi,v));}
  void preview(unsigned number){
    *this=TelekineticLaunch();level=number%3;
    const Wall layouts[][2]={{{139,43,5,25},{181,86,28,4}},{{146,69,5,30},{185,43,5,29}},{{143,42,5,27},{177,76,5,28}}};
    const Point targets[][3]={{{124,85},{182,61},{219,46}},{{119,68},{151,52},{205,78}},{{117,89},{158,75},{214,95}}};
    for(unsigned i=0;i<2;i++)walls[i]=layouts[level][i];
    for(unsigned i=0;i<3;i++)rings[i]={targets[level][i].x,targets[level][i].y,false};
  }
  void begin(unsigned number,uint32_t now){preview(number);active=true;lastTick=now;}
  void cancel(){active=finished=false;vx=vy=0;}
  unsigned litCount()const{unsigned n=0;for(const auto& r:rings)if(r.lit)n++;return n;}
  void aim(float degrees,float strength){
    if(phase!=Phase::Aim||!isfinite(degrees)||!isfinite(strength))return;
    angle=clamp(degrees,-78,35);power=clamp(strength,.25f,1);
  }
  bool fire(){
    if(!active||phase!=Phase::Aim||shots>=3)return false;
    const float radians=angle*.01745329252f,speed=78+power*115;
    x=93;y=98;vx=cosf(radians)*speed;vy=sinf(radians)*speed;
    phase=Phase::Flight;flightTime=0;shots++;shotHits=bounces=trailCount=0;
    return true;
  }
  void reflect(const Wall& w){
    float dx=x-clamp(x,w.x,w.x+w.w),dy=y-clamp(y,w.y,w.y+w.h),d=hypotf(dx,dy);
    if(d>=Radius)return;
    float nx=0,ny=0;
    if(d>1e-7f){nx=dx/d;ny=dy/d;x+=nx*(Radius-d+.005f);y+=ny*(Radius-d+.005f);}
    else{
      const float faces[]={x-w.x,w.x+w.w-x,y-w.y,w.y+w.h-y};unsigned face=0;
      for(unsigned i=1;i<4;i++)if(faces[i]<faces[face])face=i;
      if(face<2){nx=face==0?-1:1;x=face==0?w.x-Radius-.005f:w.x+w.w+Radius+.005f;}
      else{ny=face==2?-1:1;y=face==2?w.y-Radius-.005f:w.y+w.h+Radius+.005f;}
    }
    const float normal=vx*nx+vy*ny;
    if(normal<0){vx-=1.95f*normal*nx;vy-=1.95f*normal*ny;bounces++;}
  }
  void finishShot(){
    if(shotHits>bestCombo)bestCombo=shotHits;
    won=litCount()==3;
    if(won||shots>=3){active=false;finished=true;phase=won?Phase::Won:Phase::Lost;vx=vy=0;}
    else{phase=Phase::Aim;x=93;y=98;vx=vy=0;}
  }
  Events update(uint32_t now){
    Events events;if(!active)return events;
    const uint32_t elapsed=now-lastTick;lastTick=now;
    accumulator+=float(elapsed>80?80:elapsed)*.001f;
    constexpr float dt=1.f/180;
    while(accumulator>=dt&&active){
      accumulator-=dt;if(phase!=Phase::Flight)continue;
      flightTime+=dt;const float drag=expf(-.56f*dt);vx*=drag;vy*=drag;x+=vx*dt;y+=vy*dt;
      if(x<Left+Radius){x=Left+Radius;vx=fabsf(vx)*.96f;bounces++;}
      if(x>Right-Radius){x=Right-Radius;vx=-fabsf(vx)*.96f;bounces++;}
      if(y<Top+Radius){y=Top+Radius;vy=fabsf(vy)*.96f;bounces++;}
      if(y>Bottom-Radius){y=Bottom-Radius;vy=-fabsf(vy)*.96f;bounces++;}
      for(const auto& w:walls)reflect(w);
      for(auto& r:rings)if(!r.lit&&hypotf(x-r.x,y-r.y)<=6+Radius*.4f){r.lit=true;shotHits++;events.hits++;}
      if(shotHits>bestCombo)bestCombo=shotHits;
      if(litCount()==3||hypotf(vx,vy)<16||flightTime>=5)finishShot();
    }
    if(phase==Phase::Flight){
      if(trailCount==24){for(unsigned i=1;i<24;i++)trail[i-1]=trail[i];trailCount--;}
      trail[trailCount++]={x,y};
    }
    events.finished=finished;
    return events;
  }
};
