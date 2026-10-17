#include "../src/clawd_light_render.h"
#include "../src/idle_power.h"
#include "../src/display_motion.h"
#include "../src/pose_detector.h"
#include <assert.h>
#include <stdio.h>
#include <array>
using namespace ClawdLight;
ClawdWords::Gate safe(){ClawdWords::Gate g;g.awake=g.portrait=g.healthy=true;return g;}
struct Canvas {
  std::array<uint16_t,135*240> pixels{};
  void fillRect(int x,int y,int w,int h,uint16_t ink){
    assert(x>=0&&x+w<=135&&y>=120&&y+h<=204);
    for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)pixels[yy*135+xx]=ink;
  }
};
uint32_t catchOne(Policy& light,uint32_t start,const ClawdWords::Gate& gate,unsigned step=20,float baseline=0){
  for(uint32_t now=start+step;now<start+Policy::PlayMs;now+=step){
    const float control=std::max(-9.f,std::min(9.f,(67.5f-light.x())*.8f-light.velocity()*.25f));
    assert(light.update(now,gate,baseline+control)==Event::None);
    if(light.stage()==Stage::Store)return now;
  }
  assert(!"A gentle closed-loop tilt must catch within eight seconds");return 0;
}
int main(){
  auto gate=safe();
  {
    Policy light;assert(light.start(0,gate,0));assert(light.stage()==Stage::Invite);
    assert(!light.start(1,gate,0));light.update(Policy::InviteMs-1,gate,0);assert(light.stage()==Stage::Invite);
    light.update(Policy::InviteMs,gate,0);assert(light.folding());
    light.update(Policy::InviteMs+Policy::FoldMs,gate,0);assert(light.playing());
    const uint32_t play=Policy::InviteMs+Policy::FoldMs;
    // A quiet hand invites it closer without requiring a perfect tilt.
    for(uint32_t now=play+20;now<play+1000;now+=20)light.update(now,gate,0);
    assert(light.x()<90&&light.x()>75.5f&&light.progress()==0);
    // An unsettled grip keeps it shy; the eight-second ending is still bounded.
    for(uint32_t now=play+1000;now<play+Policy::PlayMs;now+=20)light.update(now,gate,12);
    assert(light.update(play+Policy::PlayMs,gate,3)==Event::Missed);assert(!light.active());
    assert(light.update(play+Policy::PlayMs+1,gate,3)==Event::None);
    assert(!light.start(59999,gate,0));assert(light.ready(60000));
    light.update(60000,gate,0);assert(!light.active()); // no deferred autoplay
    assert(light.start(60000,gate,0));
  }
  {
    // Sensor direction matches the actual portrait screen; a tilt made during
    // the invitation keeps its direction when play starts, rather than re-zeroing.
    assert(tiltDegrees(.1f,.99f,0)<0&&tiltDegrees(-.1f,.99f,0)>0);
    assert(!std::isfinite(tiltDegrees(0,0,0))&&!std::isfinite(tiltDegrees(NAN,1,0)));
    for(float direction:{-1.f,1.f}){
      Policy light;assert(light.start(0,gate,5));
      light.update(Policy::InviteMs,gate,5+direction*6);
      const auto play=Policy::InviteMs+Policy::FoldMs;
      light.update(play,gate,5+direction*6);
      for(uint32_t t=play+20;t<=play+500;t+=20)light.update(t,gate,5+direction*6);
      assert((light.x()-90)*direction>0);
    }
    // A five-degree correction moves only about one pixel in a second;
    // larger angles offer more travel, capped without continuing acceleration.
    float travel[3]={};unsigned i=0;
    for(float angle:{5.f,8.f,18.f}){
      Policy light;assert(light.start(0,gate,0,false,true));
      for(uint32_t t=20;t<=1000;t+=20)light.update(t,gate,angle);
      travel[i++]=light.x()-90;assert(light.velocity()<=24.01f);
    }
    assert(travel[0]>.2f&&travel[0]<2&&travel[1]>travel[0]*2&&travel[2]>travel[1]*2);
    Policy light;assert(light.start(0,gate,0,false,true));
    for(uint32_t t=20;t<=600;t+=20)light.update(t,gate,8);
    const float before=light.x();
    // Return to a small correction: residual momentum should settle promptly.
    for(uint32_t t=620;t<=1600;t+=20)light.update(t,gate,2.1f);
    assert(light.x()-before<3&&std::abs(light.velocity())<.2f);
  }
  {
    // Invite, folding, play and collection all hold the display in portrait.
    // Releasing it requires fresh orientation confirmation; pose detection is
    // independent so inverted speech and face-down rest can still take over.
    DisplayOrientation display;PoseDetector pose;pose.uprightSign=1;
    assert(pose.calibrateFaceUp(0,0,1));
    Policy light;assert(light.start(0,gate,0));
    for(uint32_t t=20;t<Policy::InviteMs+Policy::FoldMs+1000;t+=20){
      light.update(t,gate,0);assert(light.locksOrientation());
      display.update(1,0,0,0,1,t,light.locksOrientation());assert(display.rotation==0&&!display.pending);
    }
    light.cancel();assert(!light.locksOrientation());
    display.update(1,0,0,0,1,6000,light.locksOrientation());
    display.update(1,0,0,0,1,6089);assert(display.rotation==0);
    display.update(1,0,0,0,1,6090);assert(display.rotation==1);
    for(bool inverted:{false,true}){
      Policy active;assert(active.start(0,gate,0,false,true));
      PoseDetector detected;detected.uprightSign=1;assert(detected.calibrateFaceUp(0,0,1));
      for(uint32_t t=20;t<=240;t+=20){
        detected.update(0,inverted?-1.f:0,inverted?0:-1.f,0,t);
        auto priority=gate;priority.inverted=detected.candidate==(inverted?DevicePose::Inverted:DevicePose::FaceDown);
        active.update(t,priority,0);
      }
      assert(!active.locksOrientation());
    }
    Policy collecting;assert(collecting.start(0,gate,0,false,true));catchOne(collecting,0,gate);
    assert(collecting.stage()==Stage::Store&&collecting.locksOrientation());
  }
  {
    // Stillness itself earns trust. Gentle steering is optional, and a tease
    // makes it retreat before it will approach a quiet hand again.
    for(bool tease:{false,true}){
      Policy light;assert(light.start(0,gate,7,false,true));bool stored=false;
      for(uint32_t now=20;now<8000;now+=20){
        if(tease&&now==1800)assert(light.tease(now));
        light.update(now,gate,7);if(light.stage()==Stage::Store){stored=true;break;}
      }
      assert(stored);
    }
  }
  {
    // Same native physics and uninterrupted hold work at several UI rates.
    for(unsigned step:{10u,20u,33u,40u}){
      Policy light;assert(light.start(100,gate,5,false,true));
      const auto stored=catchOne(light,100,gate,step,5);assert(light.progress()==1);
      assert(light.update(stored+599,gate,5)==Event::None);
      assert(light.update(stored+600,gate,5)==Event::Stored);assert(!light.active());
      assert(light.update(stored+601,gate,5)==Event::None);
    }
  }
  {
    Policy light;assert(light.start(0,gate,0,false,true));const auto now=catchOne(light,0,gate);
    // Interrupted collection never emits success after voice/rotation takes over.
    gate.preparing=true;assert(light.update(now+1,gate,0)==Event::Cancelled);
    gate.preparing=false;assert(light.update(now+1000,gate,0)==Event::None);assert(!light.active());
  }
  {
    bool ClawdWords::Gate::* blockers[]={&ClawdWords::Gate::voice,&ClawdWords::Gate::preparing,&ClawdWords::Gate::inverted,
      &ClawdWords::Gate::rotation,&ClawdWords::Gate::game,&ClawdWords::Gate::setup,&ClawdWords::Gate::operation,
      &ClawdWords::Gate::gesture,&ClawdWords::Gate::story};
    for(auto member:blockers)for(bool direct:{false,true}){
      Policy light;assert(light.start(0,gate,0,false,direct));gate.*member=true;
      assert(light.update(10,gate,0)==Event::Cancelled);assert(!light.start(11,gate,0,true));
      gate.*member=false;assert(light.update(100,gate,0)==Event::None);assert(!light.active());
    }
    for(auto member:{&ClawdWords::Gate::awake,&ClawdWords::Gate::portrait,&ClawdWords::Gate::healthy}){
      Policy light;assert(light.start(0,gate,0,false,true));gate.*member=false;
      assert(light.update(1,gate,0)==Event::Cancelled);assert(!light.start(2,gate,0,true));gate.*member=true;
    }
    Policy light;assert(!light.start(0,gate,NAN));assert(light.start(0,gate,0,false,true));
    assert(light.update(1,gate,NAN)==Event::Cancelled);
  }
  {
    Policy light;assert(light.start(0,gate,0,false,true));
    assert(light.tease(20));assert(light.frightened(20)&&light.progress()==0&&light.velocity()>0);
    light.update(40,gate,0);assert(light.x()>90);
    light.update(100,gate,0,true);assert(light.frightened(100));
    assert(!light.frightened(471));
    for(uint32_t now=120;now<7900;now+=20){light.update(now,gate,now%40?80:-80,true);assert(light.x()>=14&&light.x()<=121&&light.progress()==0);}
    assert(light.update(8000,gate,0)==Event::Missed);assert(!light.tease(8001));
  }
  {
    // A long pause cannot substitute for fresh, uninterrupted steady samples.
    Policy light;assert(light.start(0,gate,0,false,true));uint32_t now=0;
    for(now=20;now<6000;now+=20){
      const float control=std::max(-9.f,std::min(9.f,(67.5f-light.x())*.8f-light.velocity()*.25f));
      light.update(now,gate,control);if(light.progress()>.6f)break;
    }
    assert(light.playing()&&light.progress()<1);
    const float before=light.progress();light.update(now+1000,gate,0);
    assert(light.playing()&&light.progress()<=before+.04f);
  }
  {
    Policy light;assert(light.start(0xfffffff0u,gate,0,false,true));
    assert(light.update(0xffffffefu,gate,0)==Event::None);assert(light.x()==90);
    assert(light.update(16,gate,0)==Event::None);assert(light.update(uint32_t(0xfffffff0u+8000u),gate,0)==Event::Missed);
    Policy demo;assert(demo.start(0,gate,0,true,true));assert(demo.isDemo());demo.cancel();assert(demo.ready(1));
    assert(demo.start(1,gate,0));demo.cancel();assert(!demo.ready(2));
  }
  {
    // Invitation and both results use normal Words timing; protected work can
    // reject even an internal follow-up. No stale result is replayed on return.
    ClawdWords::Policy words;assert(words.say(Invitation,Later,0,gate));
    assert(!words.say(Caught,Trusted,1,gate));assert(words.say(Caught,Trusted,2,gate,true));
    words.update(3922,gate,ClawdWords::Event::None,0);
    assert(!words.talking()&&!strcmp(words.text().line[0],Trusted.first));
    gate.voice=true;assert(!words.say(Missed,Later,3923,gate,true));words.update(3923,gate,ClawdWords::Event::None,0);
    gate.voice=false;words.update(3924,gate,ClawdWords::Event::None,0);assert(!words.talking());
  }
  {
    // Rendering uses the actual Light raster for live play and the entire
    // collection arc. Header, connection line, footer and screen edges survive.
    Policy light;assert(light.start(0,gate,0,false,true));Canvas frame;unsigned hints=0;
    auto text=[&](const char* value,int y,uint16_t){assert(!strcmp(value,Hint)&&y==169);hints++;};
    draw(frame,text,light,0,1,2,3);assert(hints==1&&frame.pixels[190*135+90]==2);
    const auto stored=catchOne(light,0,gate);
    for(unsigned ms=0;ms<600;ms+=5){Canvas image;draw(image,text,light,stored+ms,1,2,3);}
    light.cancel();Canvas empty;draw(empty,text,light,stored+700,1,2,3);
    for(auto pixel:empty.pixels)assert(pixel==0);
    IdlePowerPolicy power;power.activity(0);Policy quiet;assert(quiet.start(0,gate,0,true,true));
    for(uint32_t now=0;now<=8000;now+=20)quiet.update(now,gate,0);
    assert(power.lastActivity==0&&petIdleSleepDue(30001,power.lastActivity,31,false,false,false));
    assert(power.update(300000,false,false,true)==IdlePowerPolicy::Action::Sleep);
  }
  puts("Shy light: invitation, grip-relative tilt, hold, tease/fright, cooldown, cancel/no replay, wrap, results, raster and power passed");
}
