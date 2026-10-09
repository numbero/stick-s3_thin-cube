#include "pet.h"
#include "theme.h"
#include "device.h"
#include <math.h>
#include <algorithm>

namespace {
float scale=1.35f,visible=0,lift=0;
PetState state=PET_IDLE;
uint32_t entered=0,blinkAt=0,lastDraw=0;
float clamp(float x){return std::max(0.f,std::min(1.f,x));}
float rise(float t,float a,float b){float p=clamp((t-a)/(b-a));return p*p*(3-2*p);}
float window(float t,float a,float b,float c,float d){return rise(t,a,b)*(1-rise(t,c,d));}
float bump(float t,float a,float b){return t<=a||t>=b?0:sinf((t-a)/(b-a)*M_PI);}
struct Context{float x,y,s;LGFX_Sprite* g;};
void rect(const Context& c,float x,float y,float w,float h,uint16_t color){c.g->fillRect(lroundf(c.x+x*c.s),lroundf(c.y+y*c.s),ceilf(w*c.s),ceilf(h*c.s),color);}
}
void petSetScale(float s){scale=s;}
float petHeight(){return 52*scale;}
void petSetState(PetState s){if(state!=s){state=s;entered=millis();}}
PetState petState(){return state;}
void petPoke(){petSetState(PET_EXCITED);entered=millis();}
void petBlinkNow(){blinkAt=millis();}
void petDraw(LGFX_Sprite* g,int y,uint32_t ms,int originX,float attention){
  float t=ms-entered,dx=0,dy=0,eyeX=0,leftEye=6.5f,rightEye=6.5f,perkL=0,perkR=0,leg=0,step=0,brow=0;
  bool asleep=state==PET_SLEEP,live=deviceStreaming(),held=deviceHeld()&&live;
  const float dt=std::min(80u,ms-lastDraw);lastDraw=ms;
  visible+=(float(live)-visible)*std::min(1.f,dt/130);
  lift+=(float(held)-lift)*std::min(1.f,dt/85);
  if(visible<.01)visible=0;
  if(state==PET_IDLE){float cycle=fmodf(t,5200);dy=sinf(cycle/2600*M_PI*2)*.7;eyeX=1.3*window(cycle,900,1200,2000,2300)-1.3*window(cycle,3000,3300,4200,4600);}
  if(state==PET_CURIOUS){eyeX=2*window(t,100,320,2200,2750);dx=2*window(t,350,850,2400,3100);dy=-window(t,350,850,2400,3100);perkR=3*window(t,160,420,2200,2800);perkL=2.5*window(t,420,800,2400,3100);}
  if(state==PET_EXCITED){eyeX=1.5*window(t,40,150,280,450);leg=-1.5*bump(t,240,450)-.7*bump(t,1000,1130);dy=-4*bump(t,450,1000)-1.2*bump(t,1130,1430);step=.8*bump(t,450,1000)*sinf(t/120);perkL=perkR=3*window(t,180,450,1430,1900);}
  if(state==PET_UPSET){float v=window(t,100,280,1950,2700);eyeX=1.4*window(t,40,180,420,700);dy=-.6*v;brow=v;leftEye-=1.1*v;perkL=v;perkR=1.5*v;}
  if(state==PET_DIZZY){float decay=1-rise(t,300,1900);dx=sinf(t/140)*3*decay;dy=sinf(t/230)*.7*decay+.8*bump(t,1900,2250);leg=-.8*bump(t,1900,2250);perkL=sinf(t/210)*2*decay;perkR=-perkL;step=sinf(t/160)*decay;eyeX=1.3*window(t,2300,2500,2900,3200);if(t>2050&&t<2180)leftEye=rightEye=1.6;}
  if(state==PET_CHARGING){float c=fmodf(t,4400);dy=-.5*(bump(c,1300,1600)+bump(c,2150,2450)+bump(c,3000,3300));eyeX=1.3*window(c,180,500,3250,3800);perkL=perkR=window(c,180,500,3250,3800);}
  if(asleep){float v=rise(t,100,1100);dy=4*v;leg=-3*v;perkL=perkR=-4*v;leftEye=rightEye=6.5-4.9*rise(t,400,900);}
  if(state==PET_WAKE){float v=rise(t,350,1000);dx=window(t,1900,2200,2600,3100);dy=4*(1-v)-.8*bump(t,850,1500);leg=-3*(1-v);perkL=perkR=-4*(1-v)+2*window(t,700,1000,2500,3000);leftEye=1.6+4.9*rise(t,100,380);rightEye=1.6+4.9*rise(t,300,650);eyeX=-1.3*bump(t,1050,1550)+1.3*window(t,1650,1900,2500,2900);}
  if(live){perkL=perkR=3;eyeX=1.2;dx=-2*lift;dy=-lift*(.6+deviceLevel()*1.5);}
  if(!asleep&&ms-blinkAt<140)leftEye=rightEye=1.6;
  const float k=scale+(std::min(scale,1.24f)-scale)*visible;
  if(!asleep&&!live)eyeX+=std::max(-1.f,std::min(1.f,attention))*2;
  const float center=originX>=0?originX:(g->width()-66*k)/2;
  const float liveX=originX>=0?originX:16;
  Context c{center+(liveX-center)*visible+dx,float(y)+dy,k,g};
  const uint16_t skin=asleep?C::bodyDim:C::body;
  if(state==PET_CHARGING&&!live){float tick=fmodf(t,4400);for(int i=0;i<3;i++){
    float u=rise(tick,850+i*850,1450+i*850);if(tick<80+i*100||tick>1540+i*850)continue;
    rect(c,75+(58-75)*u,(28+i*7)+(24-28-i*7)*u-sinf(u*M_PI)*3,4,4,C::lite);
  }}
  const int legs[]={6,18,42,54};for(int i=0;i<4;i++)rect(c,legs[i],39,6,13+leg+(i%2?step:-step),skin);
  rect(c,0,13-perkL/2,6,13+perkL,skin);rect(c,60,13-perkR/2,6,13+perkR,skin);rect(c,6,0,54,39,skin);
  rect(c,12+eyeX,19.5-leftEye,6,leftEye,C::eye);rect(c,48+eyeX,19.5-rightEye,6,rightEye,C::eye);
  if(brow>.1){rect(c,11,9,4,1.6,C::brow);rect(c,15,7,4,1.6,C::brow);}
  if(state==PET_DIZZY&&t<2000){for(int i=0;i<2;i++){float a=t/430+i*M_PI;rect(c,31+cosf(a)*19,-10+sinf(a)*3,2,2,C::lite);}}
  if(state==PET_EXCITED&&t>450&&t<1700){rect(c,-5,5,1.5,5,C::lite);rect(c,-6.5,6.5,4.5,1.5,C::lite);rect(c,71,22,1.5,5,C::lite);rect(c,69.5,23.5,4.5,1.5,C::lite);}
  if(visible>.05){
    float f=held?sinf(ms/280.f)*1.6f:0,rx=74-lift*2,ry=25-lift*22+(1-visible)*22+f;
    rect(c,rx-3,ry+11+f,1.5,1.5,C::lite);rect(c,rx-5,ry+23-f,1.5,1.5,C::lite);
    rect(c,rx+11,ry-13,3,14,rgb565(181,165,140));rect(c,rx+11,ry-13,3,3,rgb565(232,210,174));
    rect(c,rx+2,ry-2,12,4,rgb565(113,99,79));rect(c,rx,ry+1,17,28,rgb565(135,117,90));
    rect(c,rx+2,ry+3,13,23,rgb565(48,42,35));rect(c,rx+4,ry+6,9,5,rgb565(166,133,67));rect(c,rx+5,ry+7,held?7:3,2,C::ink);
    for(int y:{14,17,20})rect(c,rx+4,ry+y,8,1.5,rgb565(161,138,102));
    rect(c,rx+14,ry+12,3,8,held?rgb565(61,220,151):rgb565(255,77,79));
    if(held){int marks=ceilf(deviceLevel()*3);for(int i=0;i<marks;i++)rect(c,rx+3+i*4,ry-18-i*3,2,3+i*2,C::ink);}
  }
}
