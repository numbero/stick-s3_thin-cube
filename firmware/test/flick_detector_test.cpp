#include "../src/flick_detector.h"
#include <assert.h>
#include <stdio.h>
#include <limits>
#include <initializer_list>
using D=FlickDetector;
struct Rig {
  D detector;uint32_t now=0;
  D::Result sample(float rate=0,unsigned axis=0,float a=1,bool active=true,bool button=false,unsigned dt=10){
    now+=dt;float gyro[3]={};gyro[axis]=rate;
    return detector.update(now,0,0,a,gyro[0],gyro[1],gyro[2],active,button);
  }
  void quiet(unsigned ms=400){for(unsigned i=0;i<ms;i+=10)sample();}
  void pulse(unsigned axis=0,float sign=1,unsigned dt=10,unsigned settle=160){
    for(unsigned t=0;t<=240+dt;t+=dt){
      float rate=t<40?900.f*t/40:t<100?900:t<140?900.f*(140-t)/40:t<240?-600:0;
      sample(sign*rate,axis,1,true,false,dt);
    }quiet(settle);
  }
  void pair(unsigned axis=0,float sign=1,unsigned dt=10){pulse(axis,sign,dt);pulse(axis,sign,dt);}
};
int main(){
  for(unsigned axis=0;axis<3;++axis)for(float sign:{-1.f,1.f})for(unsigned dt:{10u,17u,23u,33u}){
    Rig r;r.quiet();assert(r.detector.stage==D::Stage::Ready);r.pair(axis,sign,dt);
    assert(r.detector.hits==1&&r.detector.rejected==0&&r.detector.strokes==2&&r.detector.axis==axis);
    r.pulse(axis,sign,dt);assert(r.detector.hits==1);
    r.quiet(1600);r.pair(axis,sign,dt);assert(r.detector.hits==2);
  }
  {Rig r;r.quiet();r.pulse();r.quiet(1600);assert(r.detector.hits==0&&r.detector.rejected==1);}
  {Rig r;r.quiet();r.pulse(0,1,10,0);r.pulse();assert(r.detector.hits==1);}
  {Rig r;r.quiet();r.sample(320,1);r.pair(0);assert(r.detector.hits==1&&r.detector.axis==0);}
  {Rig r;r.quiet();r.pulse();r.pulse(0,-1);r.quiet(1600);assert(r.detector.hits==0);}
  {Rig r;r.quiet();r.pulse();r.pulse(1);r.quiet(1600);assert(r.detector.hits==0);}
  {Rig r;r.quiet();for(int i=0;i<5;++i)r.pulse(0,1,10,0);r.quiet(400);
    assert(r.detector.hits==0&&r.detector.rejected>=1);}
  {Rig r;r.quiet();r.pulse();r.sample(0,0,1,true,true);r.quiet();r.pulse();assert(r.detector.hits==0);}
  // Rebound is one impulse; a second full impulse is required.
  for(unsigned repeats:{1u,2u}){
    Rig r;r.quiet();
    for(unsigned n=0;n<repeats;++n){
      for(int i=0;i<8;++i)r.sample(900);
      for(int i=0;i<8;++i)r.sample(-700);
      for(int i=0;i<4;++i)r.sample(300);
      r.quiet(180);
    }
    assert(r.detector.hits==(repeats==2?1u:0u));
  }
  // Final gravity must match the initial grip, even with valid impulses.
  {Rig r;r.quiet();r.pulse();r.pulse(0,1,10,0);for(int i=0;i<30;++i)r.sample(0,0,-1);
    assert(r.detector.hits==0&&r.detector.rejected==1);}
  {Rig r;r.quiet();for(int i=0;i<300;++i)r.sample(90);assert(r.detector.starts==0);}
  {Rig r;r.quiet();for(int i=0;i<100;++i)r.sample(20,0,i%2?2.f:.3f);assert(r.detector.starts==0);}
  {Rig r;for(int i=0;i<100;++i)r.sample(900,0,1,false);assert(r.detector.starts==0);}
  {Rig r;r.quiet();for(int i=0;i<100;++i)r.sample(900,0,1,true,true);assert(r.detector.starts==0);}
  for(auto reason:{D::Reason::Context,D::Reason::Button,D::Reason::Sensor,D::Reason::Gap}){
    Rig r;r.quiet();r.sample(900);D::Result result;
    if(reason==D::Reason::Context)result=r.sample(0,0,1,false);
    else if(reason==D::Reason::Button)result=r.sample(0,0,1,true,true);
    else if(reason==D::Reason::Sensor)result=r.sample(std::numeric_limits<float>::quiet_NaN());
    else result=r.sample(0,0,1,true,false,90);
    assert(result.event==D::Event::Rejected&&result.reason==reason&&r.detector.hits==0);
    r.quiet();r.pair();assert(r.detector.hits==1);
  }
  {Rig r;r.quiet();for(int i=0;i<200;++i)r.sample(900);assert(r.detector.rejected==1&&r.detector.hits==0);}
  {Rig r;r.quiet();r.sample(900);r.quiet(1500);assert(r.detector.rejected==1&&r.detector.hits==0);}
  {Rig r;r.now=UINT32_MAX-600;r.quiet();r.pair();assert(r.detector.hits==1);}
  puts("double impulse: six directions, irregular sampling, single/opposite/cross-axis strokes, continuous pair/shaking, rebound, cooldown, final grip, slow turn/tap, gating, NaN/gap, sustained turn, isolated spike and wrap passed");
}
