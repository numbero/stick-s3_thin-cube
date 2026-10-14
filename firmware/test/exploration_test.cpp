#include "../src/exploration.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>

int main(){
  {
    // An A click starts aiming, never a timed discovery without a target.
    Exploration e;assert(e.begin(0,0,100));assert(!e.begin(0,0,300));
    assert(e.update(0,-12,true,10000)==Exploration::Event::None);
    assert(e.active&&!e.scanning&&!e.revealed&&e.discoveries==0);
  }
  {
    Exploration e;e.begin(7,0,100);e.update(9.9f,2.9f,true,190);assert(e.offset==0);
    e.update(22,-12,true,280);assert(e.offset==1);
    e.update(-8,-12,true,370);assert(e.offset==-1);
    e.update(-90,-12,true,460);assert(e.offset==-1);
    e.update(7,-12,true,505);assert(e.offset==-.5f);
    assert(e.aimX()>=Exploration::CenterX-Exploration::SpanX);
  }
  {
    Exploration e;e.begin(0,0,100);e.update(-12,-12,true,190);assert(e.near);
    assert(e.probeX()==e.targetX()&&e.probeY()==e.targetY());
    assert(e.update(-12,-12,true,439)==Exploration::Event::None);
    assert(e.update(-12,-12,true,440)==Exploration::Event::Locked&&e.scanning);
    assert(e.update(90,-12,true,1539)==Exploration::Event::None&&e.probeX()==e.targetX()&&e.probeY()==e.targetY());
    assert(e.update(90,-12,true,1540)==Exploration::Event::Found);
    assert(!e.active&&!e.scanning&&e.revealed&&e.discoveries==1&&e.artifact()==0);
    assert(e.update(0,-12,true,1800)==Exploration::Event::None&&e.discoveries==1);
  }
  {
    Exploration e;e.begin(0,0,100);
    e.update(-10,-12,true,190);assert(e.near);
    assert(e.probeX()<e.aimX()&&fabsf(e.aimX()-e.targetX())<8);
    e.update(0,-12,true,280);assert(!e.near);
    e.update(-12,-12,true,370);assert(e.near);
    assert(e.update(-12,-12,true,619)==Exploration::Event::None);
    assert(e.update(-12,-12,true,620)==Exploration::Event::Locked);
  }
  {
    // Small boundary jitter retains the candidate, but a real departure resets it.
    Exploration e;e.begin(0,0,100);e.update(-10,-12,true,190);assert(e.near);
    e.update(-9,-12,true,280);assert(e.near);
    e.update(-8,-12,true,370);assert(!e.near);
    e.update(-12,-12,true,460);e.update(NAN,-12,true,550);assert(!e.near&&!e.scanning);
    e.update(-12,-12,true,640);assert(e.near);
    assert(e.update(-12,-12,true,889)==Exploration::Event::None);
    assert(e.update(-12,-12,true,890)==Exploration::Event::Locked);
  }
  {
    Exploration e;e.begin(0,0,100);e.update(-12,-12,true,190);e.cancel();
    assert(!e.active&&!e.near&&!e.scanning);
    e.update(-12,-12,true,2000);assert(!e.revealed&&e.discoveries==0);
    e.begin(0,0,2100);e.update(-12,-12,true,2190);e.update(-12,-12,true,2440);assert(e.scanning);
    e.update(-12,-12,false,3000);assert(!e.active&&!e.scanning&&!e.revealed);
    e.update(-12,-12,true,5000);assert(e.discoveries==0);
  }
  {
    Exploration e;e.begin(0,0,0xfffffff0u);e.update(-12,-12,true,74);
    assert(e.update(-12,-12,true,323)==Exploration::Event::None);
    assert(e.update(-12,-12,true,324)==Exploration::Event::Locked);
    assert(e.update(-12,-12,true,1424)==Exploration::Event::Found);
  }
  {
    Exploration e;const float degrees[]={-12,12,-7.2f,7.8f}, vertical[]={-12,12,9,-9};
    for(unsigned i=0;i<8;i++){
      const uint32_t now=i*2000;e.begin(0,0,now);
      assert(e.targetX()>=Exploration::CenterX-Exploration::SpanX+8);
      assert(e.targetX()<=Exploration::CenterX+Exploration::SpanX-8);
      e.update(degrees[i%4],vertical[i%4],true,now+90);
      assert(e.update(degrees[i%4],vertical[i%4],true,now+340)==Exploration::Event::Locked);
      assert(e.update(0,-12,true,now+1440)==Exploration::Event::Found);
      assert(e.artifact()==i%4&&e.discoveries==i+1);
    }
  }
  {
    // Both landscape rotations and acceleration units give the same screen-relative tilt.
    const float x=cosf(.2f),y=sinf(.2f);
    assert(fabsf(LandscapeTilt::tiltDegrees(x,-y,1)-LandscapeTilt::tiltDegrees(-x,y,3))<.001f);
    assert(fabsf(LandscapeTilt::tiltDegrees(x,-y,1)-LandscapeTilt::tiltDegrees(x*9.81f,-y*9.81f,1))<.001f);
    assert(!isfinite(LandscapeTilt::tiltDegrees(0,0,1)));
    Exploration e;assert(!e.begin(NAN,0,100));
  }
  {
    LandscapeButton b;using A=LandscapeButton::Action;
    assert(b.update(100,true,false,false,true,false)==A::None);
    assert(b.update(180,false,true,false,true,false)==A::Enter);
    assert(b.update(200,false,false,true,true,false)==A::None);
    assert(b.update(300,true,false,true,true,false)==A::None);
    assert(b.update(1499,true,false,true,true,false)==A::None);
    assert(b.update(1500,true,false,true,true,false)==A::Exit&&b.blocked);
    assert(b.update(2000,true,false,false,true,false)==A::None&&b.blocked);
    assert(b.update(2010,false,true,false,true,false)==A::None&&b.blocked);
    b.update(2020,false,false,false,true,false);assert(!b.blocked);
    assert(b.update(2100,false,true,false,true,false)==A::Enter);
  }
  {
    LandscapeButton b;using A=LandscapeButton::Action;
    b.update(100,true,false,true,true,false);
    // Discovery while holding A must not turn the same press into diagnostics.
    assert(b.update(200,true,false,false,true,false)==A::None&&b.blocked);
    assert(b.update(2000,false,true,false,true,false)==A::None);
    b.update(2010,false,false,false,true,false);
    assert(b.update(2200,true,false,false,true,true)==A::None&&b.blocked);
    assert(b.update(2300,false,true,false,true,false)==A::None); // Combo consumes release.
    b.update(2310,false,false,false,true,false);
    assert(b.update(2400,false,true,false,false,false)==A::None); // Rotation in progress.
  }
  {
    LandscapeButton b;using A=LandscapeButton::Action;
    b.update(0xfffffff0u,true,false,true,true,false);
    assert(b.update(1183,true,false,true,true,false)==A::None);
    assert(b.update(1184,true,false,true,true,false)==A::Exit);
  }
  {
    // A matching horizontal position alone cannot hold or scan a higher target.
    Exploration e;assert(e.begin(0,0,100));
    e.update(-12,0,true,190);assert(!e.near&&e.aimX()==e.targetX());
    assert(e.update(-12,0,true,5000)==Exploration::Event::None&&!e.scanning);
    e.update(-12,-12,true,5090);assert(e.near&&e.probeY()==e.targetY());
    assert(e.update(-12,-12,true,5339)==Exploration::Event::None);
    assert(e.update(-12,-12,true,5340)==Exploration::Event::Locked);
    const float x=e.probeX(),y=e.probeY();
    e.update(80,70,true,6000);assert(e.probeX()==x&&e.probeY()==y);
    assert(e.update(NAN,NAN,true,6440)==Exploration::Event::Found);
    assert(e.aimX()==e.targetX()&&e.aimY()==e.targetY());
  }
  {
    // Both axes use the entry grip, dead zone, smoothing and saturation.
    Exploration e;assert(e.begin(7,-5,100));
    e.update(9.9f,-2.1f,true,190);assert(e.offset==0&&e.offsetY==0);
    e.update(22,10,true,280);assert(e.offset==1&&e.offsetY==1);
    e.update(-8,-20,true,370);assert(e.offset==-1&&e.offsetY==-1);
    e.update(-90,-85,true,460);assert(e.offset==-1&&e.offsetY==-1);
    e.update(7,-5,true,505);assert(e.offset==-.5f&&e.offsetY==-.5f);
    e.update(7,10,true,595);assert(e.offset==0&&e.offsetY==1);
    e.update(22,-5,true,685);assert(e.offset==1&&e.offsetY==0);
    assert(e.aimY()>=52&&e.aimY()<=88);
  }
  {
    // Snap and release use a circle, including diagonal misses and hysteresis.
    Exploration e;e.begin(0,0,100);
    auto angle=[](float v){return v==0?0:v*12+(v<0?-3:3);};
    auto at=[&](float dx,float dy,uint32_t now){
      return e.update(angle((e.targetX()+dx-e.CenterX)/e.SpanX),
                      angle((e.targetY()+dy-e.CenterY)/e.SpanY),true,now);
    };
    at(6,6,190);assert(!e.near); // Each axis is under 8 px, total distance is not.
    at(5,5,280);assert(e.near);
    assert(fabsf(e.probeX()-(e.aimX()+(e.targetX()-e.aimX())*.45f))<.001f);
    assert(fabsf(e.probeY()-(e.aimY()+(e.targetY()-e.aimY())*.45f))<.001f);
    at(7,7,370);assert(e.near);
    at(7.2f,7.2f,460);assert(!e.near);
    at(0,0,550);assert(e.near);
    assert(at(0,0,799)==Exploration::Event::None);
    assert(at(0,0,800)==Exploration::Event::Locked);
  }
  {
    Exploration e;assert(!e.begin(0,NAN,100)&&!e.begin(INFINITY,0,100));
    e.begin(0,0,100);e.update(-12,-12,true,190);assert(e.near);
    const float x=e.aimX(),y=e.aimY();
    e.update(-12,NAN,true,400);assert(!e.near&&e.aimX()==x&&e.aimY()==y);
    e.update(NAN,-12,true,500);assert(!e.near&&!e.scanning);
    e.update(-12,-12,true,590);assert(e.near);
    assert(e.update(-12,-12,true,839)==Exploration::Event::None);
    assert(e.update(-12,-12,true,840)==Exploration::Event::Locked);
    e.cancel();assert(e.begin(4,8,900)&&e.offset==0&&e.offsetY==0&&e.baselineY==8);
  }
  {
    // Gravity roll and pitch remain independent in both landscape orientations.
    for(uint8_t rotation:{uint8_t(1),uint8_t(3)})for(float roll:{-15.f,0.f,15.f})for(float pitch:{-15.f,0.f,15.f}){
      const float r=roll*.01745329252f,p=pitch*.01745329252f;
      const float x=cosf(r)*cosf(p)*(rotation==1?1:-1);
      const float y=sinf(r)*cosf(p)*(rotation==1?-1:1),z=-sinf(p);
      assert(fabsf(LandscapeTilt::tiltDegrees(x,y,rotation)-roll)<.001f);
      assert(fabsf(LandscapeTilt::verticalDegrees(x,y,z)-pitch)<.001f);
      assert(fabsf(LandscapeTilt::verticalDegrees(x*9.81f,y*9.81f,z*9.81f)-pitch)<.001f);
    }
    assert(!isfinite(LandscapeTilt::verticalDegrees(0,0,0)));
    assert(!isfinite(LandscapeTilt::verticalDegrees(1,0,NAN)));
    assert(!isfinite(LandscapeTilt::verticalDegrees(INFINITY,0,0)));
  }
  {
    // Reach every target with real gravity samples, including a nonzero grip.
    Exploration e;float heights[4];
    auto input=[](float roll,float pitch,float& h,float& v){
      const float r=roll*.01745329252f,p=pitch*.01745329252f;
      const float x=cosf(r)*cosf(p),y=-sinf(r)*cosf(p),z=-sinf(p);
      h=LandscapeTilt::tiltDegrees(x,y,1);v=LandscapeTilt::verticalDegrees(x,y,z);
    };
    const float horizontal[]={-12,12,-7.2f,7.8f},vertical[]={-12,12,9,-9};
    for(unsigned i=0;i<8;i++){
      const uint32_t now=i*2000;float h,v;input(6,-7,h,v);assert(e.begin(h,v,now));
      assert(e.targetX()-10>=129&&e.targetX()+10<233);
      assert(e.targetY()-10>=39&&e.targetY()+10<99);
      if(i<4)heights[i]=e.targetY();
      input(6+horizontal[i%4],-7+vertical[i%4],h,v);
      e.update(h,v,true,now+90);assert(e.near);
      assert(e.update(h,v,true,now+340)==Exploration::Event::Locked);
      assert(e.update(0,0,true,now+1440)==Exploration::Event::Found);
    }
    for(unsigned i=0;i<4;i++)for(unsigned j=i+1;j<4;j++)assert(heights[i]!=heights[j]);
  }
  {
    // Wrapped entry angles keep both axes relative to the same grip.
    Exploration e;e.begin(179,-179,100);e.update(-166,166,true,190);
    assert(e.offset==1&&e.offsetY==-1);
  }
  puts("19 exploration 2D aiming, radial lock, scan, sensor mapping and input scenarios passed");
}
