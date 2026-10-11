#include "../src/clawd_story_policy.h"
#include "../src/idle_power.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
using namespace ClawdStory;
Inputs safe(){Inputs in;in.awake=true;in.home=true;in.takeover=false;in.motionHealthy=true;
  in.manualSafe=true;in.battery=80;in.quietMs=13000;in.stableMs=4000;in.sleepRemainingMs=17000;return in;}
int main(){
  {
    // Diagnostic A must reach every story exactly once, then wrap, while old
    // USB numbers retain their identity and invalid input cannot start a clip.
    assert(Count==13);assert(demoCommand("1")==Id::Repair);assert(demoCommand("5")==Id::Hole);
    assert(demoCommand("10")==Id::Garden);assert(demoCommand("13")==Id::Portal);
    for(const char* text:{"","0","14","255","-1","1x","1 2","stop"})assert(demoCommand(text)==Id::None);
    Id cursor=Id::Repair;bool seen[Count]={};unsigned common=0,special=0;
    for(unsigned i=0;i<Count;i++){
      assert(valid(cursor)&&!seen[unsigned(cursor)]);seen[unsigned(cursor)]=true;
      if(rare(cursor))special++;else common++;
      cursor=demoNext(cursor);
    }
    assert(cursor==Id::Repair&&common==8&&special==5);
    assert(!valid(Id::Count)&&!valid(Id::None));
  }
  {
    Policy p;p.begin(500,0);assert(p.opportunityAt()==20*Policy::Minute);
    p.begin(500,20*Policy::Minute);assert(p.opportunityAt()==40*Policy::Minute);
    auto in=safe();p.update(500,in,0);p.update(1500,in,0);assert(p.awakeMs()==1000);
    in.awake=false;p.update(2500,in,0);p.update(3002500,in,0);assert(p.awakeMs()==2000);
    in.awake=true;p.update(3003500,in,0);assert(p.awakeMs()==2000);
    p.update(3004500,in,0);assert(p.awakeMs()==3000);
  }
  {
    Policy p;p.begin(0,0);auto in=safe();p.update(0,in,0);
    assert(p.update(20*Policy::Minute-1,in,0)==Event::None);
    in.interrupt=true;assert(p.update(20*Policy::Minute,in,0)==Event::None);
    in.interrupt=false;in.quietMs=11999;assert(p.update(1200001,in,0)==Event::None);
    in.quietMs=12000;in.stableMs=2999;assert(p.update(1200002,in,0)==Event::None);
    in.stableMs=3000;in.sleepRemainingMs=8999;assert(p.update(1200003,in,0)==Event::None);
    assert(p.opportunityAt()>p.awakeMs());
    // A missed deadline discards the old chance; a new one must fit exactly.
    p.begin(0,0);p.update(0,in,0);
    in.sleepRemainingMs=9000;assert(p.update(1200004,in,0)==Event::Started);
    assert(!rare(p.current())&&p.usedBudget()==1);
    const auto id=p.current();in.interrupt=true;
    assert(p.update(1200005,in,0)==Event::Cancelled);assert(p.cooling(id));assert(p.usedBudget()==1);
    assert(p.opportunityAt()==p.awakeMs()+20*Policy::Minute);
  }
  {
    // A missed deadline consumes the opportunity once; subsequent frame
    // randomness cannot reroll it into a shorter/common/rare clip.
    Policy p;p.begin(0,0);auto in=safe();p.update(0,in,0);
    const uint32_t opportunity=Policy::Hour;in.sleepRemainingMs=0;
    assert(p.update(opportunity,in,10)==Event::None);
    const auto retry=p.opportunityAt();assert(retry>p.awakeMs());
    in.sleepRemainingMs=17000;
    for(uint32_t now=opportunity+1;now<opportunity+1000;now++)assert(p.update(now,in,0)==Event::None);
    assert(p.opportunityAt()==retry);
    assert(p.update(uint32_t(retry),in,0)==Event::Started);assert(rare(p.current()));
  }
  {
    Policy p;p.begin(0,0);auto in=safe();p.update(0,in,0);
    in.quietMs=0;p.update(1200000,in,0);p.update(1290000,in,0);
    assert(p.opportunityAt()==p.awakeMs()+5*Policy::Minute);
    p.update(1590000,in,0);in.awake=false;p.update(1590001,in,0);
    const auto next=p.opportunityAt();p.update(2590001,in,0);assert(p.opportunityAt()==next);
  }
  {
    Policy p;p.begin(0,0);auto in=safe();p.update(0,in,0);
    in.battery=20;assert(p.update(1200000,in,0)==Event::None);
    in.battery=24;assert(p.update(1200001,in,0)==Event::None);
    in.battery=-1;in.charging=true;assert(p.update(1200002,in,0)==Event::None);
    in.battery=25;in.charging=false;assert(p.update(1200003,in,0)==Event::Started);
    in.motionHealthy=false;assert(p.update(1200004,in,0)==Event::Cancelled);
    p.begin(0,0);in=safe();in.battery=19;in.charging=true;p.update(0,in,0);
    assert(p.update(1200000,in,0)==Event::Started);
    in.charging=false;assert(p.update(1200001,in,0)==Event::Cancelled);
  }
  {
    // Long-clock simulation checks actual rolling windows and cooldowns,
    // including cancellation, rare eligibility and intervals across hours.
    Policy p;p.begin(0,0);auto in=safe();p.update(0,in,0);
    std::vector<uint64_t> starts;uint64_t lastEnd[Count]={};bool seen[Count]={};unsigned rareCount=0;
    Id previous=Id::None,active=Id::None;uint32_t random=7;
    for(uint32_t now=1000;now<=10*Policy::Hour;now+=1000){
      random=random*1664525u+1013904223u;
      const auto old=p.current();const auto event=p.update(now,in,random);
      if(event==Event::Started){
        active=p.current();assert(active!=previous);const auto awake=p.awakeMs();
        unsigned recent=0;for(auto at:starts)if(awake-at<Policy::Hour)recent++;assert(recent<2);starts.push_back(awake);
        if(seen[uint8_t(active)])assert(awake-lastEnd[uint8_t(active)]>=(rare(active)?4:2)*uint64_t(Policy::Hour));
        if(rare(active)){assert(awake>=Policy::Hour);rareCount++;}
      }else if(event==Event::Finished){seen[uint8_t(old)]=true;lastEnd[uint8_t(old)]=p.awakeMs();previous=old;}
      assert(p.usedBudget()<=2);
    }
    assert(starts.size()>=5);(void)active;(void)rareCount;
  }
  {
    // Rare pool is gated by accumulated awake time, not a sleeping wall clock.
    Policy p;p.begin(0,0);auto in=safe();in.quietMs=0;p.update(0,in,0);
    p.update(Policy::Hour,in,0);in.quietMs=13000;
    assert(p.update(Policy::Hour+1,in,0)==Event::Started);assert(rare(p.current()));
  }
  {
    // Context changes weights within a category, not the 10% category share.
    unsigned baseGravity=0,preferredGravity=0,rareCount=0,selected[Count]={};
    for(uint32_t random=0;random<10000;random++){
      for(unsigned preferred=0;preferred<2;preferred++){
        Policy p;p.begin(0,0);auto in=safe();p.update(0,in,0);
        in.preferGravity=preferred;
        assert(p.update(Policy::Hour,in,random)==Event::Started);
        selected[unsigned(p.current())]++;
        if(preferred&&rare(p.current()))rareCount++;
        if(p.current()==Id::Gravity){if(preferred)preferredGravity++;else baseGravity++;}
      }
    }
    assert(rareCount==1000);assert(baseGravity>=150&&baseGravity<=250);
    assert(preferredGravity>=260&&preferredGravity<=410);assert(preferredGravity>baseGravity*1.3f);
    for(unsigned count:selected)assert(count>0);
  }
  {
    Policy p;p.begin(0,0);auto in=safe();p.update(0,in,0);in.takeover=true;
    for(unsigned i=0;i<Count;i++){
      const auto id=Id(i);assert(p.startDemo(id,i*10000,in,0));assert(p.isDemo());assert(p.usedBudget()==0);
      assert(p.update(i*10000+duration(id),in,0)==Event::Finished);assert(!p.cooling(id));
    }
    const uint32_t now=Count*10000;
    in.manualSafe=false;assert(!p.startDemo(Id::Portal,now,in,0));
    in.manualSafe=true;assert(p.startDemo(Id::Portal,now,in,0));in.manualInterrupt=true;
    assert(p.update(now+1,in,0)==Event::Cancelled);
    in.manualInterrupt=false;assert(p.startDemo(Id::Portal,now+2,in,0));in.battery=20;
    assert(p.update(now+3,in,0)==Event::Cancelled);
    in.battery=23;p.update(now+4,in,0);assert(!p.startDemo(Id::Portal,now+4,in,0));
    in.battery=25;p.update(now+5,in,0);assert(p.startDemo(Id::Portal,now+5,in,0));
  }
  {
    Policy p;p.begin(0,0);auto in=safe();p.update(0,in,0);in.takeover=true;
    Id cursor=Id::Repair;
    for(unsigned i=0;i<=Count;i++){
      assert(p.startDemo(cursor,i*100,in,0));assert(p.current()==cursor);
      assert(p.usedBudget()==0&&!p.cooling(cursor));cursor=demoNext(cursor);
    }
    assert(p.current()==Id::Repair);assert(p.cancel(0)==Event::Cancelled);assert(!p.playing());
    // Charger indication can change while a manually requested preview stays
    // on the same input. Actual input changes still use manualInterrupt.
    in.charging=true;assert(p.startDemo(Id::Garden,2000,in,0));
    in.charging=false;in.interrupt=true;
    assert(p.update(6000,in,0)==Event::None);assert(p.current()==Id::Garden);
    in.manualInterrupt=true;assert(p.update(6001,in,0)==Event::Cancelled);
  }
  {
    Policy p;p.begin(0xfffffff0u,0);auto in=safe();p.update(0xfffffff0u,in,0);
    p.update(24,in,0);assert(p.awakeMs()==40);p.update(23,in,0);assert(p.awakeMs()==40);
    assert(p.startDemo(Id::Sticker,0xffffff00u,in,0));assert(p.elapsed(0x100)==512);
    // Drawing never resets the existing visual/standby/power-off deadlines.
    IdlePowerPolicy power;power.activity(0);assert(petIdleSleepDue(30001,power.lastActivity,31,false,false,false));
    assert(power.update(300000,false,false,true)==IdlePowerPolicy::Action::Sleep);
    assert(power.update(600000,false,true,true)==IdlePowerPolicy::Action::PowerOff);
  }
  puts("12 story scheduling scenarios: all 13 candidates, diagnostic cycle, awake/sleep, single selection, weights, budget, cooldown, power and wrap passed");
}
