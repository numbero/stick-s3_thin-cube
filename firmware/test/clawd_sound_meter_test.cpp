#include "../src/clawd_sound_meter.h"
#include "../src/voice_state.h"
#include "../src/idle_power.h"
#include <cassert>
#include <array>
#include <cstdio>
using namespace ClawdSound;
Gate home(){Gate g;g.enabled=true;g.home.awake=g.home.portrait=g.home.healthy=true;return g;}
void feed(Meter& m,unsigned begin,unsigned end,float rms){for(unsigned t=begin;t<=end;t+=10)m.sample(t,rms);}
struct Canvas {
  std::array<unsigned,135*240> pixels{};
  unsigned count=0;
  void fillRect(int x,int y,int w,int h,unsigned ink){
    assert(x>=71&&x+w<=131&&y>=26&&y+h<=44&&(w==1||w==2)&&h>=1&&h<=18);count++;
    for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)pixels[yy*135+xx]=ink;
  }
};
int main(){
  auto g=home();assert(g.captureWanted());
  {
    // Opening from saved settings needs neither a toggle nor a new wake event.
    Meter m;IdlePowerPolicy power;power.activity(0);ClawdWords::Policy words;
    for(unsigned t=0;t<=28000;t+=10){
      assert(g.captureWanted());
      const bool sound=t%3000>=1000&&t%3000<1300;
      m.sample(t,sound?.012f:.0006f);
      if(t%3000==1100){assert(m.level(t)>.59f&&m.attention(t)>.7f);}
      if(t%3000==2900){assert(m.level(t)<.01f);}
      words.update(t,g.home,ClawdWords::Event::None,t*73);
      assert(!words.talking());
    }
    assert(m.samples()>2700&&g.captureWanted()); // No ten-second/one-round exit.
    assert(power.lastActivity==0&&petIdleSleepDue(30001,0,31,false,false,false));
  }
  {
    Meter m;m.sample(0,.42f);feed(m,10,190,.42f);assert(m.level(190)==0&&m.flinch(190)==0);
    feed(m,200,400,.0006f);assert(m.level(400)==0);
    feed(m,410,600,.003f);const auto soft=m.level(600);assert(soft>.2f&&soft<.3f);
    feed(m,610,800,.012f);assert(m.level(800)>soft+.3f);
    feed(m,810,900,.0006f);feed(m,910,1010,.06f);assert(m.level(1010)>.95f&&m.flinch(1010)>.5f);
    feed(m,1020,1600,.0006f);assert(m.level(1600)<.02f&&m.flinch(1600)==0);
    feed(m,1610,1700,.06f);assert(m.level(1700)>.9f&&m.flinch(1700)==0); // Wave still reacts during reflex cooldown.
    feed(m,1710,4500,.0006f);feed(m,4510,4630,.06f);assert(m.flinch(4630)>.7f);
    feed(m,4640,10000,.06f);assert(m.level(10000)>.98f&&m.flinch(10000)==0); // A steady loud room does not repeatedly flinch.
    assert(m.level(10251)==0);m.sample(10300,.4f);assert(m.level(10300)==0); // Restart pop ignored after lost frames.
    m.sample(10310,NAN);assert(m.level(10310)==0);m.sample(10320,-1);assert(m.level(10320)==0);
  }
  {
    bool ClawdWords::Gate::* busy[]={&ClawdWords::Gate::voice,&ClawdWords::Gate::preparing,&ClawdWords::Gate::inverted,
      &ClawdWords::Gate::rotation,&ClawdWords::Gate::game,&ClawdWords::Gate::setup,&ClawdWords::Gate::operation,&ClawdWords::Gate::gesture,&ClawdWords::Gate::story};
    for(auto member:busy){auto blocked=g;blocked.home.*member=true;assert(!blocked.captureWanted());blocked.home.*member=false;assert(blocked.captureWanted());}
    for(auto member:{&ClawdWords::Gate::awake,&ClawdWords::Gate::portrait,&ClawdWords::Gate::healthy}){auto blocked=g;blocked.home.*member=false;assert(!blocked.captureWanted());}
    auto off=g;off.enabled=false;assert(!off.captureWanted());off=g;off.light=true;assert(!off.captureWanted());off=g;off.cue=true;assert(!off.captureWanted());
    Meter m;feed(m,0,500,.03f);assert(m.level(500)>0);m.reset();assert(m.level(501)==0&&m.flinch(501)==0);
    feed(m,600,1000,.003f);assert(m.level(1000)>.2f); // Every automatic resume is live again.
    VoiceState v;assert(v.captureWanted(false,g.captureWanted()));v.localSound=false;assert(!v.captureWanted(false,true));
    v.allowed=true;v.setPeer(true,0);assert(v.captureWanted(true,false));v.captureReady=true;v.requestManual(false,0);assert(v.captureWanted());v.stop();assert(!v.captureWanted());
  }
  {
    // One short sound enters at the right and travels 60 pixels in 3.6s.
    // Quiet input does not erase the old peak or create any replay action.
    Trace trace;trace.sample(0,0);trace.sample(180,0);trace.sample(360,1);
    assert(trace.bar(360,0).x==130&&trace.bar(360,0).height==18);
    for(unsigned t=540;t<=3960;t+=180){
      trace.sample(t,0);
      const unsigned index=(t-360)/180;
      const auto peak=trace.bar(t,index);
      assert(peak.height==18&&peak.x==130-int((t-360)/60));
      if(t==1260)assert(peak.x==115);
      if(t==2160)assert(peak.x==100);
      if(t==3060)assert(peak.x==85);
      if(t==3960)assert(peak.age==Trace::WindowMs);
    }
    trace.sample(4140,0);
    for(unsigned i=0;i<Trace::Slots;i++)assert(trace.bar(4140,i).height==1);
    // Scrolling uses elapsed time, including between samples and over wrap.
    Trace fast,slow,wrapped;
    for(unsigned t=0;t<=7200;t+=10){
      const float value=t%900==0?1:0;
      fast.sample(t,value);wrapped.sample(0xfffff000u+t,value);
      if(t%30==0)slow.sample(t,value);
      if(t%180==0)for(unsigned i=0;i<Trace::Slots;i++){
        const auto a=fast.bar(t+60,i),b=slow.bar(t+60,i),c=wrapped.bar(uint32_t(0xfffff000u+t+60),i);
        assert(a.x==b.x&&a.height==b.height&&a.age==b.age);
        assert(a.x==c.x&&a.height==c.height&&a.age==c.age);
      }
    }
    assert(fast.bar(7200+60,0).x==129&&fast.bar(7200+120,0).x==128);
    fast.sample(13000,0); // A long gap clears every old slot.
    for(unsigned i=0;i<Trace::Slots;i++)assert(fast.bar(13000,i).height==1);
    assert(sizeof(Meter)<=128); // No PCM buffers or heap allocation.
  }
  {
    unsigned previous=0;
    for(float rms:{.0006f,.003f,.012f,.1f}){
      Meter m;feed(m,0,4000,rms);
      for(unsigned phase=0;phase<180;phase+=10){
        Canvas c;draw(c,m,4000+phase,true,0xf580,0xfe49,0x6a60);
        assert(c.count>=19&&c.count<=21);
        unsigned pixels=0;for(auto p:c.pixels)if(p)pixels++;
        if(phase==0){assert(pixels>=previous);previous=pixels;}
        // Title, state label, Clawd, text and bottom controls are untouched.
        for(unsigned y=0;y<240;y++)for(unsigned x=0;x<135;x++)
          if(x<71||x>=131||y<26||y>=44)assert(c.pixels[y*135+x]==0);
      }
      Canvas off;draw(off,m,4000,false,1,2,3);assert(off.count==20);
      for(unsigned y=0;y<240;y++)for(unsigned x=0;x<135;x++)
        if(y!=35)assert(off.pixels[y*135+x]==0);
      Canvas stale;draw(stale,m,4251,true,1,2,3);assert(stale.pixels==off.pixels);
      m.reset();Canvas reset;draw(reset,m,4300,true,1,2,3);assert(reset.pixels==off.pixels);
      feed(m,4400,4500,.1f);assert(!m.traceReady(4500)); // Warmup does not draw a false peak.
    }
    assert(fade(0x6a60,0xf580,0)==0x6a60&&fade(0x6a60,0xf580,255)==0xf580);
  }
  {
    Meter m;for(unsigned t=0;t<2000;t+=10)m.sample(0xfffffe00u+t,t>800&&t<1000?.012f:.0006f);
    assert(m.samples()>150&&m.level(uint32_t(0xfffffe00u+1990))<.01f);
  }
  puts("Ambient sound: continuous response, preserved reflex/power/priority, 3.6s right-to-left peak trace, time-based scrolling, wrap, expiry, clipping and reset passed");
}
