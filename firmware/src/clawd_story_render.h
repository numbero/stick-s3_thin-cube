#pragma once
#include "clawd_story_policy.h"
#include <algorithm>
#include <math.h>
#include <initializer_list>

namespace ClawdStoryRaster {
constexpr uint16_t color565(uint8_t r,uint8_t g,uint8_t b){return uint16_t(((r&248)<<8)|((g&252)<<3)|(b>>3));}
struct Palette {
  static constexpr uint16_t body=color565(217,119,87),eye=color565(20,14,10);
  static constexpr uint16_t lite=color565(232,161,132),screen=color565(13,13,13);
  static constexpr uint16_t glow=color565(255,201,77),ok=color565(61,220,151);
};
constexpr float Pi=3.14159265f;
constexpr uint16_t Paper=color565(234,217,181),Dim=color565(89,67,35);
constexpr uint16_t Blue=color565(121,205,232),Deep=color565(23,60,74);
constexpr uint16_t Violet=color565(167,151,213),Plum=color565(48,42,73),Metal=color565(146,151,143);
constexpr uint16_t Steel=color565(147,150,139),Dark=color565(48,54,57);
constexpr uint16_t Green=color565(114,188,133),Leaf=color565(49,103,71);
constexpr uint16_t Whale=color565(91,143,159),Belly=color565(134,179,189),Fin=color565(61,107,123);
inline float clamp(float v){return std::max(0.f,std::min(1.f,v));}
inline float mix(float a,float b,float v){return a+(b-a)*v;}
inline float rise(float t,float a,float b){const float v=clamp((t-a)/(b-a));return v*v*(3-2*v);}
inline float gate(float t,float a,float b,float c,float d){return rise(t,a,b)*(1-rise(t,c,d));}
inline float hop(float t,float a,float b){return t<a||t>b?0:sinf(Pi*(t-a)/(b-a));}
struct Point {float x,y;};
struct Pose {
  float cx,bottom,k,weight,look=0,eye=6.5f,eyeY=0,left=0,right=0,step=0,angle=0,squash=1,foot=0;
  float xscale=1,eyeR=-1,steps[4]={};
  bool gentle=false;
  int missing=-1;
  Pose(float x,float y,float scale,float w):cx(x),bottom(y),k(scale),weight(w){}
};
template<class Surface> class Painter {
 public:
  Painter(Surface& target,ClawdStory::Id scene,float seconds):g(target),id(scene),t(seconds){}
  void draw(){
    switch(id){
      case ClawdStory::Id::Repair:repair();break;
      case ClawdStory::Id::Camp:camp();break;
      case ClawdStory::Id::Gravity:gravity();break;
      case ClawdStory::Id::Sticker:sticker();break;
      case ClawdStory::Id::Hole:hole();break;
      case ClawdStory::Id::Space:space();break;
      case ClawdStory::Id::Water:water();break;
      case ClawdStory::Id::Photo:photo();break;
      case ClawdStory::Id::Dance:dance();break;
      case ClawdStory::Id::Garden:garden();break;
      case ClawdStory::Id::Plane:plane();break;
      case ClawdStory::Id::Whale:whale();break;
      case ClawdStory::Id::Portal:portal();break;
      default:break;
    }
  }
 private:
  Surface& g;ClawdStory::Id id;float t,alpha=1;
  uint16_t ink(uint16_t color)const{
    if(alpha>=.999f)return color;
    const unsigned r=(color>>11)*255/31,gg=((color>>5)&63)*255/63,b=(color&31)*255/31;
    return color565(lroundf(mix(13,r,alpha)),lroundf(mix(13,gg,alpha)),lroundf(mix(13,b,alpha)));
  }
  template<class F> void fade(float opacity,F fn){
    if(opacity<=0)return;const float old=alpha;alpha*=clamp(opacity);fn();alpha=old;
  }
  void rect(float x,float y,float w,float h,uint16_t c){
    if(w<=0||h<=0)return;
    g.fillRect(lroundf(x),60+lroundf(y),std::max(1,int(ceilf(w))),std::max(1,int(ceilf(h))),ink(c));
  }
  void line(float x,float y,float ex,float ey,uint16_t c){g.drawLine(lroundf(x),60+lroundf(y),lroundf(ex),60+lroundf(ey),ink(c));}
  void poly(std::initializer_list<Point> points,uint16_t c){
    const Point* p=points.begin();const unsigned n=points.size();if(n<3||n>6)return;
    float low=p[0].y,high=low;for(const auto& q:points){low=std::min(low,q.y);high=std::max(high,q.y);}
    // Clipped scanlines bound work even while a stamp/floor is off-screen.
    for(int y=std::max(0,int(ceilf(low)));y<=std::min(109,int(floorf(high)));y++){
      float xs[6];unsigned count=0;
      for(unsigned i=0;i<n;i++){
        const Point a=p[i],b=p[(i+1)%n];
        if((a.y<=y&&b.y>y)||(b.y<=y&&a.y>y))xs[count++]=a.x+(y-a.y)*(b.x-a.x)/(b.y-a.y);
      }
      std::sort(xs,xs+count);
      for(unsigned i=0;i+1<count;i+=2)rect(ceilf(xs[i]),y,floorf(xs[i+1])-ceilf(xs[i])+1,1,c);
    }
  }
  void ellipse(float x,float y,float rx,float ry,uint16_t c,bool outline=false){
    rx=std::max(.2f,rx);ry=std::max(.2f,ry);
    for(int yy=-int(ceilf(ry));yy<=int(ceilf(ry));yy++){
      const float n=1-yy*yy/(ry*ry);if(n<0)continue;const int xx=lroundf(rx*sqrtf(n));
      if(outline){rect(x-xx,y+yy,1,1,c);rect(x+xx,y+yy,1,1,c);}else rect(x-xx,y+yy,xx*2+1,1,c);
    }
  }
  void star(float x,float y,float s=1,uint16_t c=Palette::glow){rect(x-2*s,y,5*s,s,c);rect(x,y-2*s,s,5*s,c);}
  void sky(){
    static const uint8_t stars[][2]={{9,10},{28,21},{48,8},{75,16},{106,7},{124,31},{17,55},{117,66}};
    for(unsigned i=0;i<8;i++)rect(stars[i][0],stars[i][1],1,1,i%2?Dim:Paper);
  }
  void floor(){line(6,100,129,100,Dim);}
  Pose actor(float x,float bottom,float k){
    const float end=ClawdStory::duration(id)*.001f;
    const bool gentle=unsigned(id)>=unsigned(ClawdStory::Id::Space);
    const float weight=rise(t,0,gentle?.7f:.8f)*(1-rise(t,end-.8f,end));
    Pose result(mix(67.5f,x,weight),mix(97.6f,bottom,weight),mix(1.55f,k,weight),weight);result.gentle=gentle;return result;
  }
  void pet(const Pose& a){
    if(a.k<.11f){rect(a.cx-1,a.bottom-2,2,2,Palette::body);return;}
    const float k=a.k,squash=mix(1,a.squash,a.weight),angle=a.angle*a.weight,xscale=mix(1,a.xscale,a.weight);
    const float cy=a.bottom-26*k*squash,cs=cosf(angle),sn=sinf(angle);
    auto point=[&](float x,float y)->Point{const float xx=(x-33)*k*xscale,yy=(y-26)*k*squash;return {a.cx+xx*cs-yy*sn,cy+xx*sn+yy*cs};};
    auto R=[&](float x,float y,float w,float h,uint16_t c=Palette::body){
      if(fabsf(angle)<.0001f){const Point p=point(x,y);rect(p.x,p.y,w*k*xscale,h*k*squash,c);}
      else poly({point(x,y),point(x+w,y),point(x+w,y+h),point(x,y+h)},c);
    };
    if(k<.4f&&!a.gentle){
      R(6,0,54,39);R(0,13,66,13);
      for(int x:{6,18,42,54})R(x,39,6,13);
      R(12,13,6,6,Palette::eye);R(48,13,6,6,Palette::eye);return;
    }
    const int legs[]={6,18,42,54};
    for(int i=0;i<4;i++)if(a.weight<.98f||a.missing!=i)R(legs[i],39,6,std::max(2.f,13+(a.gentle?a.steps[i]:(i%2?a.step:-a.step))*a.weight));
    const float left=a.left*a.weight,right=a.right*a.weight;
    R(0,13-left/2,6,13+left);R(60,13-right/2,6,13+right);R(6,0,54,39);
    if(a.gentle&&k<.34f){R(12,13,6,6,Palette::eye);R(48,13,6,6,Palette::eye);return;}
    const float leftEye=a.gentle?mix(6.5f,a.eye,a.weight):a.eye;
    const float rightEye=a.gentle?mix(6.5f,a.eyeR<0?a.eye:a.eyeR,a.weight):a.eye;
    R(12+a.look*a.weight,19.5f-leftEye+a.eyeY*a.weight,6,leftEye,Palette::eye);
    R(48+a.look*a.weight,19.5f-rightEye+a.eyeY*a.weight,6,rightEye,Palette::eye);
    if(a.foot>0&&a.weight>0){
      const float amount=a.foot*a.weight;
      const uint16_t footColor=color565(lroundf(mix(217,232,amount)),lroundf(mix(119,161,amount)),lroundf(mix(87,132,amount)));
      R(54,39-amount/k,mix(6*k,19,amount)/k,mix(13*k,11,amount)/k,footColor);
    }
  }
  static Point turn(float x,float y,float cx,float cy,float angle){
    const float cs=cosf(angle),sn=sinf(angle),dx=x-cx,dy=y-cy;
    return {cx+dx*cs-dy*sn,cy+dx*sn+dy*cs};
  }
  void gentleFloor(){line(5,101,130,101,Dim);}
  void travelSky(float warp=0){
    static const uint8_t stars[][2]={{8,10},{27,26},{47,8},{74,17},{104,6},{124,34},{15,55},{118,78},{29,88},{90,103}};
    for(unsigned i=0;i<10;i++){
      const float x=fmodf(stars[i][0]+135-fmodf(t*warp*(9+i%3),135),135);
      rect(x,stars[i][1],1,1,i%3?Dim:Paper);if(warp)rect(x-3*warp,stars[i][1],3*warp,1,Dim);
    }
  }
  void rocket(float x,float y,float s,bool fire){
    auto R=[&](float xx,float yy,float w,float h,uint16_t c){rect(x+xx*s,y+yy*s,w*s,h*s,c);};
    poly({{x,y},{x+12*s,y+13*s},{x-12*s,y+13*s}},Paper);R(-12,12,24,34,Steel);R(-9,18,18,18,Deep);
    R(-12,38,24,4,Palette::body);R(-16,34,4,15,Paper);R(12,34,4,15,Paper);R(-7,46,14,3,Dark);
    if(fire){const float h=9+sinf(t*23)*3;poly({{x-6*s,y+49*s},{x+6*s,y+49*s},{x,y+(49+h)*s}},Palette::glow);R(-2,49,4,h*.6f,Palette::lite);}
  }
  void space(){
    const float launch=rise(t,1.1f,3),far=rise(t,2.6f,3.5f),land=rise(t,3.8f,4.7f),exit=rise(t,4.8f,5.4f),lift=hop(t,5.6f,6.5f);
    const float s=mix(mix(.95f,.36f,far),.72f,land),x=mix(mix(40,105,launch),100,land),y=mix(mix(40,10,launch),47,land);
    Pose a=actor(mix(x,48,exit),mix(y+35*s,96,exit)-12*lift,mix(.26f*s,.43f,exit));
    a.look=2;a.eyeY=-lift;a.left=3;a.right=2;a.squash=1+.12f*lift;a.angle=-.08f*lift;
    fade(a.weight,[&]{travelSky(gate(t,1.5f,2,3.8f,4.3f));fade(land,[&]{ellipse(79,105,50,14,Steel);ellipse(72,97,7,3,Dark);ellipse(107,99,5,2,Dark);});rocket(x,y,s,t<4.5f);});pet(a);
    fade(a.weight*rise(t,5.15f,5.55f),[&]{const float top=72-12*lift;line(65,top,65,96-12*lift,Paper);poly({{66,top},{82,top+sinf(t*5)*2},{80,top+8},{66,top+7}},Palette::body);rect(70,top+2,2,2,Palette::eye);rect(76,top+2,2,2,Palette::eye);});
  }
  void cup(){
    rect(87,24,29,3,Paper);rect(88,27,2,58,Paper);rect(113,27,2,58,Paper);rect(90,84,24,2,Paper);
    rect(116,37,9,2,Paper);rect(123,39,2,24,Paper);rect(116,63,9,2,Paper);
    const unsigned used=unsigned(t>1.4f)+unsigned(t>2)+unsigned(t>2.6f);
    rect(91,58+used*3,20,24-used*3,Blue);rect(91,58+used*3,20,1,Paper);
  }
  void water(){
    const float bubble=gate(t,2.8f,3.15f,4.75f,5.05f),land=hop(t,4.9f,5.3f),jump=hop(t,5.65f,6.25f),wobble=sinf(t*6)*bubble;
    Pose a=actor(38+3*wobble,97-17*bubble-9*jump,mix(.6f,.45f,bubble));
    a.look=1.5f;a.eye=land>.2f?1.6f:6.5f;a.eyeY=-bubble;a.angle=.09f*wobble;a.squash=1+.09f*wobble-.26f*land;a.xscale=1+.12f*land;a.left=2;a.right=3;a.steps[1]=a.steps[3]=jump*3;
    fade(a.weight,[&]{gentleFloor();cup();for(float at:{.95f,1.65f,2.3f}){
      if(t<at||t>at+.7f)continue;const float u=clamp((t-at)/.7f);ellipse(mix(99,a.cx,u),mix(54,a.bottom-25,u)-14*sinf(u*Pi),2,2.7f,Blue);
    }fade(bubble,[&]{const float cy=a.bottom-14;ellipse(a.cx,cy,21+2*wobble,22-2*wobble,Deep);ellipse(a.cx,cy,21+2*wobble,22-2*wobble,Blue,true);line(a.cx-13,cy-15,a.cx-6,cy-19,Paper);});
      fade(gate(t,5.2f,5.5f,6.25f,6.5f),[&]{ellipse(a.cx+7,100,8-3*jump,2+3*jump,Blue);});
      fade(hop(t,4.8f,5.35f),[&]{for(unsigned i=0;i<7;i++){const float r=23+8*rise(t,4.8f,5.35f),angle=i*Pi*2/7;rect(a.cx+cosf(angle)*r,a.bottom-14+sinf(angle)*r,1,2,Blue);}});
    });pet(a);
  }
  void digit(unsigned n,float x,float y){
    static const uint8_t rows[][5]={{2,6,2,2,7},{6,1,2,4,7},{6,1,2,1,6}};
    for(unsigned yy=0;yy<5;yy++)for(unsigned xx=0;xx<3;xx++)if(rows[n-1][yy]&(4>>xx))rect(x+xx*2,y+yy*2,2,2,Palette::glow);
  }
  void camera(){
    rect(84,38,7,3,Steel);rect(79,41,29,18,Steel);rect(82,44,23,11,Dark);ellipse(98,49,6,6,Paper);ellipse(98,49,4,4,Blue);rect(83,43,4,3,Palette::glow);
    line(93,59,93,77,Steel);line(93,73,82,97,Steel);line(93,73,105,97,Steel);
    if(t>.85f&&t<2.55f)digit(t<1.45f?3:t<2.05f?2:1,60,13);
    fade(hop(t,2.6f,2.85f),[&]{star(115,39,1,Paper);rect(114,48,3,1,Paper);});
  }
  void photoCard(float x,float y,float w,float h,float angle){
    const float cx=x+w/2,cy=y+h/2;auto P=[&](float xx,float yy){return turn(xx,yy,cx,cy,angle);};
    poly({P(x,y),P(x+w,y),P(x+w,y+h),P(x,y+h)},Paper);poly({P(x+4,y+4),P(x+w-4,y+4),P(x+w-4,y+h-19),P(x+4,y+h-19)},Deep);
    const Point a=P(x+13,y+h-9),b=P(x+w-13,y+h-9);line(a.x,a.y,b.x,b.y,Dim);
  }
  void photo(){
    const float enter=rise(t,2.95f,4.25f),leave=rise(t,5.05f,6.2f),jump=hop(t,5.15f,6.1f),angle=sinf(t*4)*.055f*gate(t,4.2f,4.4f,5.1f,5.5f);
    Pose a=actor(mix(mix(mix(38,67,enter),29,leave),56,rise(t,6.25f,6.7f)),mix(mix(96,77,enter),97,leave)-19*jump,mix(mix(.6f,.74f,enter),.65f,leave));
    a.look=-1;a.eyeR=hop(t,1.8f,2.1f)>.2f?1.6f:6.5f;a.eye=hop(t,4.65f,4.9f)>.2f?1.6f:6.5f;a.angle=angle;a.left=2;a.right=3;a.squash=1+.12f*jump-.18f*hop(t,6.1f,6.4f);
    fade(a.weight,[&]{gentleFloor();fade(1-rise(t,3,3.75f),[&]{camera();});fade(gate(t,2.85f,3.1f,6.65f,7.15f),[&]{
      const float shrink=rise(t,6.1f,6.6f),w=mix(mix(18,68,enter),20,shrink),h=mix(mix(24,89,enter),26,shrink),x=mix(mix(85,33,enter)+18*leave,93,shrink),y=mix(mix(67,7,enter),73,shrink)-5*hop(t,5.6f,6.3f);
      photoCard(x,y,w,h,angle);
    });});pet(a);
  }
  void record(float x,float y,float rx,float phase,uint16_t c=Palette::glow){
    ellipse(x,y,rx,std::max(2.f,rx*.35f),Palette::screen);rect(x+cosf(phase*9)*rx*.6f,y+sinf(phase*9)*rx*.2f,2,1,c);
  }
  void deck(){
    rect(46,75,45,3,Steel);rect(47,78,43,14,Dark);rect(51,92,3,8,Steel);rect(83,92,3,8,Steel);record(57,75,8,t);record(78,75,8,-t,Blue);
    for(unsigned i=0;i<5;i++){const float h=3+(1+sinf(t*9+i))*3;rect(58+i*4,89-h,2,h,i%2?Blue:Violet);}
  }
  void dance(){
    const float airborne=hop(t,2.8f,4.6f),land=hop(t,4.6f,4.95f),side=rise(t,4.2f,4.7f),sway=sinf(t*8),wrong=hop(t,4.9f,5.7f);
    Pose a=actor(mix(67,106,side)+sway*2,97-28*airborne,.57f);
    a.look=sway;a.eye=land>.2f?1.8f:6.5f;a.angle=.1f*sway;a.squash=1-.23f*land;a.left=3+sway;a.right=3-sway;
    for(unsigned i=0;i<4;i++)a.steps[i]=sinf(t*8+i*Pi/2)*2.2f;
    fade(a.weight,[&]{gentleFloor();for(int y:{85,95,108})line(4,y,131,y,Plum);deck();fade(gate(t,2.8f,3,4.15f,4.6f),[&]{record(29,48-9*airborne,6,t,Blue);record(105,28+8*sinf(t*4),6,t,Violet);});fade(wrong,[&]{record(59,75-37*wrong,8,t*1.5f);});});pet(a);
    if(t<2.8f)fade(a.weight,[&]{rect(47,78,43,14,Dark);});
  }
  Point flower(){
    const float grow=rise(t,2.3f,4.5f),lean=gate(t,4.7f,5,5.45f,5.85f),headX=96-13*lean,headY=mix(83,17,grow)+8*lean,jointY=mix(89,53,grow);
    rect(83,96,25,3,Paper);poly({{85,99},{106,99},{103,109},{88,109}},Palette::body);line(96,96,96,jointY,Green);line(96,jointY,headX,headY,Green);line(97,96,97,jointY,Green);
    for(unsigned i=0;i<3;i++){const float g=clamp(grow*1.5f-i*.22f);if(g>.05f){ellipse(87,86-i*20*grow,8*g,3*g+.2f,Leaf);ellipse(104,79-i*20*grow,7*g,3*g+.2f,Green);}}
    if(grow>.55f){const float size=rise(grow,.55f,1);for(unsigned i=0;i<6;i++){const float angle=i*Pi/3;ellipse(headX+cosf(angle)*9*size,headY+sinf(angle)*8*size,5*size,4*size,Palette::glow);}ellipse(headX,headY,4*size,4*size,Palette::body);}
    return {headX,headY};
  }
  void garden(){
    const float surprise=rise(t,3.5f,4.5f),close=rise(t,5.7f,6.25f),hat=rise(t,4.9f,5.7f);
    Pose a=actor(35-7*surprise,100,mix(mix(.62f,.36f,surprise),.6f,close));a.look=1.7f;a.eyeY=-1.5f;a.left=2;a.right=4;a.angle=-.08f*hop(t,4,4.8f);a.eye=hop(t,6.25f,6.55f)>.2f?1.6f:6.5f;
    fade(a.weight,[&]{gentleFloor();const Point head=flower();fade(1-rise(t,2.65f,3.1f),[&]{
      rect(43,29,17,12,Steel);line(45,29,36,25,Steel);line(36,25,35,35,Steel);line(35,35,43,39,Steel);poly({{60,31},{73,37},{62,39}},Paper);
      if(t>.9f)for(unsigned i=0;i<4;i++){const float u=fmodf((t-.9f)*1.5f+i*.25f,1);rect(71+25*u,38+54*u,1,2,Blue);}
    });if(t>4.9f){const float x=mix(head.x,a.cx,hat),y=mix(head.y,a.bottom-52*a.k-3,hat)-4*hop(t,4.9f,5.7f);ellipse(x,y,8,3,Palette::glow);rect(x-3,y+2,6,1,Palette::body);}});pet(a);
  }
  void paperPlane(float x,float y,float s,float angle,float flat){
    auto P=[&](float xx,float yy){return turn(x+xx*s,y+yy*s*(1-flat),x,y,angle);};
    poly({P(-25,0),P(25,-4),P(1,10)},Paper);const Point a=P(-23,0),b=P(4,2),c=P(24,-4);line(a.x,a.y,b.x,b.y,Steel);line(b.x,b.y,c.x,c.y,Dark);
  }
  void plane(){
    const float u=rise(t,1.45f,5.7f),far=sinf(u*Pi),x=67.5f+34*sinf(u*Pi*2),y=83-42*far,s=mix(.95f,.52f,far),dismount=rise(t,5.75f,6.55f),angle=.25f*sinf(u*Pi*2);
    const float k=.29f*s,cx=x+26*k*sinf(angle),bottom=y+1+26*k*(1-cosf(angle));
    Pose a=actor(mix(cx,52,dismount),mix(bottom,98,dismount)-13*hop(t,5.9f,6.55f),mix(k,.65f,dismount));a.look=1.5f;a.angle=angle*(1-dismount);a.left=3;a.right=4;a.steps[0]=a.steps[2]=-1;a.steps[1]=a.steps[3]=1;
    fade(a.weight,[&]{ellipse(17,19,10,3,Dark);ellipse(113,35,15,3,Dark);ellipse(25,92,14,2,Dark);
      for(unsigned i=1;i<8;i++){const float prev=std::max(0.f,u-i*.018f);fade(.6f-i*.06f,[&]{rect(67.5f+34*sinf(prev*Pi*2),83-42*sinf(prev*Pi)+5,1,1,Dim);});}
      paperPlane(x,mix(y,99,dismount),s,angle*(1-dismount),rise(t,6.05f,6.7f));
    });pet(a);
  }
  void whaleBody(float x,float y,float s){
    ellipse(x,y,42*s,18*s,Whale);ellipse(x+8*s,y+6*s,33*s,10*s,Belly);const float tail=3*sinf(t*3);
    poly({{x-34*s,y},{x-55*s,y-12*s+tail},{x-51*s,y+tail},{x-55*s,y+12*s+tail},{x-34*s,y+5*s}},Whale);
    poly({{x+2*s,y+6*s},{x+16*s,y+24*s},{x+20*s,y+8*s}},Fin);rect(x+29*s,y-6*s,3*s,3*s,Palette::eye);rect(x+30*s,y-6*s,s,s,Paper);line(x+33*s,y+3*s,x+40*s,y+3*s,Fin);
  }
  void whale(){
    const float far=hop(t,1.2f,3.1f),near=rise(t,2.8f,3.7f),off=rise(t,5.25f,6.35f),s=mix(.84f,.51f,far)+.13f*near;
    const float x=mix(73+4*sinf(t),123,off),y=80+2*sinf(t*2),lift=hop(t,3.65f,5.15f),slide=rise(t,5.25f,5.9f),stepOff=rise(t,5.85f,6.45f);
    Pose a=actor(mix(x-(9+42*slide)*s,38,stepOff),mix(y-18*s*(1-slide)-19*lift,98,stepOff)-6*hop(t,5.85f,6.45f),mix(.3f*s,.65f,stepOff));
    a.look=1.3f;a.angle=.1f*sinf(t*3)+.15f*lift-.15f*hop(t,5.25f,6.1f);a.left=3;a.right=2;a.steps[1]=a.steps[3]=2*lift;
    fade(a.weight,[&]{travelSky(.18f);whaleBody(x,y,s);for(unsigned i=0;i<3;i++){
      const float at=2.7f+i*.5f,u=clamp((t-at)/1.6f);if(t<at||t>at+1.6f)continue;const float px=x+15*s+14*u,py=y-13*s-42*u;
      fade(1-rise(u,.8f,1),[&]{ellipse(px,py,3,3,Blue,true);star(px,py,.55f);});
    }fade(lift,[&]{ellipse(a.cx,a.bottom+1,9,3,Blue,true);star(a.cx,a.bottom+1,.6f);});fade(gate(t,6.15f,6.3f,6.8f,7.1f),[&]{star(92,31,.8f);});});pet(a);
  }
  void portalRing(float x,uint16_t c){
    ellipse(x,66,7,25,Plum);ellipse(x,66,7,25,c,true);ellipse(x,66,4,21,Palette::screen,true);
    for(unsigned i=0;i<5;i++){const float angle=t*4+i*Pi*2/5;rect(x+cosf(angle)*9,66+sinf(angle)*27,1,1,c);}
  }
  void box(float x,float y,float s=1,float angle=0){
    const float cx=x+6.5f*s,cy=y+5.5f*s;auto P=[&](float xx,float yy){return turn(x+xx*s,y+yy*s,cx,cy,angle);};
    poly({P(0,0),P(13,0),P(13,11),P(0,11)},Palette::lite);poly({P(5,0),P(8,0),P(8,11),P(5,11)},Palette::glow);
    const Point a=P(0,3),b=P(13,3);line(a.x,a.y,b.x,b.y,Palette::body);
  }
  void portal(){
    const float enter=rise(t,1.15f,2.65f),exit=rise(t,2.95f,3.95f),stack=rise(t,4.15f,5.3f),nest=rise(t,5.5f,6.45f);const bool before=t<2.85f;
    const float squeeze=before?rise(t,1.8f,2.5f):1-exit;
    Pose a=actor(before?mix(60,23,enter):mix(111,85,exit),before?mix(97,80,enter):mix(80,97,exit),.5f);
    a.look=before?-1.5f:-1;a.angle=-.1f*stack*(1-nest);a.squash=1+.3f*squeeze;a.xscale=1-.85f*squeeze;a.left=2;a.right=3;a.eye=hop(t,6.4f,6.65f)>.2f?1.6f:6.5f;
    const float visible=1-gate(t,2.5f,2.7f,2.95f,3.15f);
    fade(a.weight,[&]{gentleFloor();portalRing(23,Violet);portalRing(111,Blue);if(t<2.7f)box(mix(67,18,enter),45-6*hop(t,1.1f,2.7f),mix(.9f,.2f,enter));else if(t<4.2f)box(mix(109,77,exit),45,.9f);});
    fade(1-a.weight*(1-visible),[&]{pet(a);});fade(a.weight,[&]{if(t>=4.2f)for(unsigned i=0;i<3;i++){
      fade(rise(t,4.15f+i*.22f,4.45f+i*.22f),[&]{const float x=mix(70+(i%2?3:0)+sinf(t*6+i)*(1-nest),93,nest),y=mix(52-float(i)*13,87,nest),s=mix(.9f,.9f-i*.17f,nest);box(x,y,s,(int(i)-1)*.09f*stack*(1-nest));});
    }});
  }
  void repair(){
    const float fixed=rise(t,4.8f,5.2f);Pose a=actor(mix(48,61,rise(t,4.6f,5.4f)),99,.72f);
    a.look=2.2f;a.eyeY=2*gate(t,1.1f,1.4f,6.2f,6.5f);a.eye=6.5f-4.8f*hop(t,6.25f,6.45f);
    a.missing=t>1.25f?3:-1;a.foot=rise(t,4.7f,5.15f);
    fade(a.weight,[&]{floor();fade(rise(t,1.05f,1.3f)*(1-fixed),[&]{
      rect(mix(63,92,rise(t,1.2f,1.8f)),mix(92,99,rise(t,1.2f,1.8f))-7*hop(t,1.8f,2.3f)-5,6,5,Palette::body);
    });fade(gate(t,2,2.35f,6.3f,6.9f),[&]{
      const float y=58-7*hop(t,3.05f,3.4f)-6*hop(t,3.8f,4.15f);
      rect(89,y+4,4,16,Metal);rect(85,y,12,6,Metal);rect(89,y-1,4,4,Palette::screen);
    });fade(gate(t,3.05f,3.2f,4.4f,4.65f),[&]{star(83,85+sinf(t*10)*2,.6f);star(105,88+cosf(t*10)*2,.5f);});});pet(a);
  }
  void tent(float build,bool panels=false){
    const float top=mix(94,45,build),lx=mix(86,54,build),rx=mix(86,123,build),door=mix(94,59,build),dl=mix(86,76,build),dr=mix(86,102,build);
    if(!panels){poly({{lx,94},{86,top},{rx,94}},Palette::lite);poly({{dl,93},{86,door},{dr,93}},Deep);}
    else{poly({{lx,94},{86,top},{86,door},{dl,94}},Palette::lite);poly({{86,top},{rx,94},{dr,94},{86,door}},Palette::lite);}
    line(86,top,86,door,Paper);line(lx-7,98,lx,94,Metal);line(rx,94,std::min(130.f,rx+7),98,Metal);
  }
  void camp(){
    const float walk=rise(t,2.3f,3.8f),inside=rise(t,3.75f,4.25f),out=rise(t,6.75f,7.15f);
    Pose a=actor(mix(29,86,walk),mix(99,92,walk),mix(.52f,.34f,walk));a.look=2;a.left=2;a.step=sinf(t*12)*gate(t,2.3f,2.5f,3.55f,3.8f);
    fade(a.weight,[&]{floor();sky();tent(rise(t,.85f,2.15f));const float travel=rise(t,3.3f,5.05f);
      fade(1-rise(t,4.95f,5.2f),[&]{star(mix(mix(21,110,rise(t,1.8f,3.1f)),96,travel),mix(mix(24,49,rise(t,1.8f,3.1f)),85,travel),mix(1.5f,.55f,travel));});
    });fade(1-inside+out,[&]{pet(a);});fade(a.weight,[&]{tent(rise(t,.85f,2.15f),true);fade(inside*(1-out),[&]{
      const bool blink=hop(t,5.15f,5.35f)>.2f||hop(t,6.15f,6.35f)>.2f;
      rect(82,80,2,blink?1:3,Palette::body);rect(88,80,2,blink?1:3,Palette::body);
      fade(rise(t,5.05f,5.35f),[&]{const bool sb=hop(t,5.6f,5.8f)>.2f;rect(92,86,2,sb?1:2,Palette::glow);rect(97,86,2,sb?1:2,Palette::glow);});
    });});
  }
  void gravity(){
    const float flip=rise(t,2.25f,4.15f),release=rise(t,4.75f,6.1f);
    Pose a=actor(mix(mix(40,52,flip),81,release),mix(mix(99,8+52*.66f,flip),78,release),mix(.66f,.48f,release));
    a.look=2;a.angle=Pi*flip*(1-release);a.eye=6.5f-4.8f*hop(t,6.45f,6.65f);a.left=3*gate(t,2,2.5f,5.8f,6.2f);
    fade(a.weight,[&]{fade(1-release,[&]{
      const float cs=cosf(Pi*flip),sn=sinf(Pi*flip);line(67.5f-61.5f*cs-46*sn,54-61.5f*sn+46*cs,67.5f+61.5f*cs-46*sn,54+61.5f*sn+46*cs,Dim);
    });fade(release,[&]{sky();ellipse(81,93,47,15,Paper);ellipse(96,94,5,3,Palette::lite);ellipse(62,96,7,3,Palette::lite);});
      const float ax=mix(102,35,release),ay=mix(mix(86,15,rise(t,1.1f,2.15f)),34,release);
      ellipse(ax,ay,5,6,Palette::body);rect(ax,ay-9,1,4,Metal);rect(ax+1,ay-8,3,1,Palette::ok);
      fade(gate(t,1.15f,1.3f,2.15f,2.35f),[&]{line(ax-5,ay+12,ax-6,ay+20,Dim);line(ax+5,ay+12,ax+6,ay+20,Dim);});
    });pet(a);
  }
  void stamp(float y){rect(49,y-18,36,11,Metal);rect(61,y-34,12,16,Palette::lite);rect(44,y-7,46,7,Plum);}
  void sticker(){
    const float crush=rise(t,2.05f,2.5f),peel=rise(t,3.65f,4.6f),bounce=rise(t,4.7f,5.4f);
    Pose a=actor(67,mix(99,99-25*hop(t,4.7f,5.55f),bounce),mix(mix(.78f,1,crush),.78f,bounce));
    a.squash=mix(mix(1,.23f,crush),1+.1f*hop(t,5,5.5f),bounce);a.look=1.5f;a.angle=-.06f*peel*(1-bounce);a.eye=6.5f-4.8f*hop(t,5.75f,5.95f);
    fade(a.weight,[&]{floor();fade(rise(t,1.75f,2),[&]{rect(30,83,77,17,Paper);poly({{98,83},{108,83},{108,mix(94,65,peel)*(1-bounce)+83*bounce}},Palette::lite);});});pet(a);
    fade(a.weight,[&]{fade(gate(t,.8f,1.05f,2.75f,3.3f),[&]{stamp(mix(-8,58,rise(t,1,2.05f))+29*crush-120*rise(t,2.75f,3.3f));});
      fade(gate(t,4.8f,4.95f,5.6f,5.8f),[&]{line(42,95,36,104,Palette::glow);line(94,94,103,101,Palette::glow);});});
  }
  void hole(){
    const float suck=rise(t,2.15f,3.95f),eject=rise(t,4.35f,5.75f),hide=gate(t,3.85f,4.05f,4.35f,4.5f);
    Pose a=actor(mix(mix(38,101,suck),38,eject),mix(mix(99,61,suck),99,eject)-31*hop(t,4.35f,5.75f),mix(mix(.7f,.07f,suck),.7f,eject));
    a.look=2;a.angle=t<5.75f?Pi*2*eject:0;a.eye=6.5f-4.8f*hop(t,6.5f,6.7f);
    fade(a.weight,[&]{floor();const float burp=hop(t,4.1f,4.55f);
      ellipse(103,54,22+burp*4,27-burp*7,Plum);ellipse(103,54,15+burp*3,20-burp*5,Palette::screen);ellipse(103,54,22+burp*4,27-burp*7,Violet,true);
      fade(gate(t,2.3f,2.5f,3.9f,4.05f),[&]{line(60,47,71,50,Dim);line(50,65,66,63,Palette::glow);line(66,76,75,72,Dim);});
      fade(gate(t,4.25f,4.35f,4.75f,5),[&]{line(78,51,71,46,Violet);line(79,68,71,73,Palette::glow);});
    });fade(1-hide,[&]{pet(a);});fade(a.weight*rise(t,5.6f,6.1f),[&]{
      const float x=63+cosf((t-5.7f)*3)*10,y=42+sinf((t-5.7f)*3)*5;ellipse(x,y,5,5,Blue);line(x-8,y+3,x+8,y-3,Paper);
    });
  }
};
}
