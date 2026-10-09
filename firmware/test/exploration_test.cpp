#include "../src/exploration.h"
#include <assert.h>
#include <stdio.h>

int main(){
  {
    // An A click starts aiming, never a timed discovery without a target.
    Exploration e;assert(e.begin(0,100));assert(!e.begin(0,300));
    assert(e.update(0,true,10000)==Exploration::Event::None);
    assert(e.active&&!e.scanning&&!e.revealed&&e.discoveries==0);
  }
  {
    Exploration e;e.begin(7,100);e.update(9.9f,true,190);assert(e.offset==0);
    e.update(22,true,280);assert(e.offset==1);
    e.update(-8,true,370);assert(e.offset==-1);
    e.update(-90,true,460);assert(e.offset==-1);
    e.update(7,true,505);assert(e.offset==-.5f);
    assert(e.aimX()>=Exploration::CenterX-Exploration::SpanX);
  }
  {
    Exploration e;e.begin(0,100);e.update(-12,true,190);assert(e.near);
    assert(e.probeX()==e.targetX());
    assert(e.update(-12,true,439)==Exploration::Event::None);
    assert(e.update(-12,true,440)==Exploration::Event::Locked&&e.scanning);
    assert(e.update(90,true,1539)==Exploration::Event::None&&e.probeX()==e.targetX());
    assert(e.update(90,true,1540)==Exploration::Event::Found);
    assert(!e.active&&!e.scanning&&e.revealed&&e.discoveries==1&&e.artifact()==0);
    assert(e.update(0,true,1800)==Exploration::Event::None&&e.discoveries==1);
  }
  {
    Exploration e;e.begin(0,100);
    e.update(-10,true,190);assert(e.near);
    assert(e.probeX()<e.aimX()&&fabsf(e.aimX()-e.targetX())<8);
    e.update(0,true,280);assert(!e.near);
    e.update(-12,true,370);assert(e.near);
    assert(e.update(-12,true,619)==Exploration::Event::None);
    assert(e.update(-12,true,620)==Exploration::Event::Locked);
  }
  {
    // Small boundary jitter retains the candidate, but a real departure resets it.
    Exploration e;e.begin(0,100);e.update(-10,true,190);assert(e.near);
    e.update(-9,true,280);assert(e.near);
    e.update(-8,true,370);assert(!e.near);
    e.update(-12,true,460);e.update(NAN,true,550);assert(!e.near&&!e.scanning);
    e.update(-12,true,640);assert(e.near);
    assert(e.update(-12,true,889)==Exploration::Event::None);
    assert(e.update(-12,true,890)==Exploration::Event::Locked);
  }
  {
    Exploration e;e.begin(0,100);e.update(-12,true,190);e.cancel();
    assert(!e.active&&!e.near&&!e.scanning);
    e.update(-12,true,2000);assert(!e.revealed&&e.discoveries==0);
    e.begin(0,2100);e.update(-12,true,2190);e.update(-12,true,2440);assert(e.scanning);
    e.update(-12,false,3000);assert(!e.active&&!e.scanning&&!e.revealed);
    e.update(-12,true,5000);assert(e.discoveries==0);
  }
  {
    Exploration e;e.begin(0,0xfffffff0u);e.update(-12,true,74);
    assert(e.update(-12,true,323)==Exploration::Event::None);
    assert(e.update(-12,true,324)==Exploration::Event::Locked);
    assert(e.update(-12,true,1424)==Exploration::Event::Found);
  }
  {
    Exploration e;const float degrees[]={-12,12,-7.2f,7.8f};
    for(unsigned i=0;i<8;i++){
      const uint32_t now=i*2000;e.begin(0,now);
      assert(e.targetX()>=Exploration::CenterX-Exploration::SpanX+8);
      assert(e.targetX()<=Exploration::CenterX+Exploration::SpanX-8);
      e.update(degrees[i%4],true,now+90);
      assert(e.update(degrees[i%4],true,now+340)==Exploration::Event::Locked);
      assert(e.update(0,true,now+1440)==Exploration::Event::Found);
      assert(e.artifact()==i%4&&e.discoveries==i+1);
    }
  }
  {
    // Both landscape rotations and acceleration units give the same screen-relative tilt.
    const float x=cosf(.2f),y=sinf(.2f);
    assert(fabsf(LandscapeTilt::tiltDegrees(x,-y,1)-LandscapeTilt::tiltDegrees(-x,y,3))<.001f);
    assert(fabsf(LandscapeTilt::tiltDegrees(x,-y,1)-LandscapeTilt::tiltDegrees(x*9.81f,-y*9.81f,1))<.001f);
    assert(!isfinite(LandscapeTilt::tiltDegrees(0,0,1)));
    Exploration e;assert(!e.begin(NAN,100));
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
  puts("12 exploration aiming, lock, scan, input ownership and timer scenarios passed");
}
