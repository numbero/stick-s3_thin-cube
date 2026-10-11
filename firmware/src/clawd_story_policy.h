#pragma once
#include "ui_timing.h"
#include <stdint.h>

namespace ClawdStory {
// Append stories so the original diagnostic numbers remain compatible.
enum class Id : uint8_t { Repair, Camp, Gravity, Sticker, Hole, Space, Water, Photo, Dance, Garden, Plane, Whale, Portal, Count, None=255 };
constexpr unsigned Count=unsigned(Id::Count);
inline bool valid(Id id){return unsigned(id)<Count;}
inline Id demoNext(Id id){return valid(id)?Id((unsigned(id)+1)%Count):Id::Repair;}
inline Id demoCommand(const char* text){
  unsigned number=0;
  if(!text||!*text)return Id::None;
  for(const char* p=text;*p;p++){
    if(*p<'0'||*p>'9')return Id::None;
    number=number*10+unsigned(*p-'0');
    if(number>Count)return Id::None;
  }
  return number?Id(number-1):Id::None;
}
inline uint32_t duration(Id id){return id==Id::Sticker?7000:valid(id)?8000:0;}
inline bool rare(Id id){return id==Id::Gravity||id==Id::Hole||id==Id::Space||id==Id::Whale||id==Id::Portal;}
inline const char* name(Id id){
  static const char* const names[]={"repair","camp","gravity","sticker","hole","space","water","photo","dance","garden","plane","whale","portal"};
  static_assert(sizeof(names)/sizeof(*names)==Count,"Every story needs a diagnostic name");
  return valid(id)?names[uint8_t(id)]:"none";
}

struct Inputs {
  bool awake=false,home=false,takeover=true,motionHealthy=false;
  bool interrupt=false,manualInterrupt=false,manualSafe=false,charging=false;
  bool preferRepair=false,preferGravity=false;
  int battery=-1;
  uint32_t quietMs=0,stableMs=0,sleepRemainingMs=0;
};
enum class Event { None, Started, Finished, Cancelled };

// Pure timing policy. No hardware, drawing, activity timestamps or allocation.
class Policy {
 public:
  static constexpr uint32_t Minute=60000,Hour=60*Minute;
  void begin(uint32_t now,uint32_t random){
    *this=Policy();initialized=true;lastMs=now;nextAt=longWait(random);
  }
  uint64_t awakeMs()const{return awake;}
  uint64_t opportunityAt()const{return nextAt;}
  Id current()const{return active;}
  bool playing()const{return valid(active);}
  bool isDemo()const{return demo;}
  uint32_t elapsed(uint32_t now)const{return petStateElapsed(now,started);}
  unsigned usedBudget()const{
    unsigned count=0;for(unsigned i=0;i<2;i++)if(occupied[i]&&awake<releaseAt[i])count++;
    return count;
  }
  bool cooling(Id id)const{return valid(id)&&awake<coolUntil[uint8_t(id)];}

  Event update(uint32_t now,const Inputs& in,uint32_t random){
    if(!initialized)begin(now,random);
    if(wasAwake)awake+=petStateElapsed(now,lastMs);
    lastMs=now;wasAwake=in.awake;
    if(in.battery>=0&&in.battery<=20)batteryPaused=true;
    else if(in.battery>=25)batteryPaused=false;
    const bool powerOK=in.battery>=0&&in.battery<=100&&(in.charging||!batteryPaused);
    if(playing()){
      const bool cancel=demo?(!in.manualSafe||in.manualInterrupt||!powerOK):
        (!in.awake||!in.home||in.takeover||!in.motionHealthy||in.interrupt||!powerOK);
      if(cancel){finish(random);return Event::Cancelled;}
      if(elapsed(now)>=duration(active)){finish(random);return Event::Finished;}
      return Event::None;
    }
    if(pending&&(!in.awake||in.takeover||!in.home))discard(random);
    if(pending&&petStateElapsed(now,pendingSince)>=90000)discard(random);
    if(!in.awake||in.takeover||!in.home||!powerOK)return Event::None;
    if(!pending&&awake>=nextAt){pending=true;pendingSince=now;}
    if(!pending||in.interrupt||!in.motionHealthy||in.quietMs<12000||in.stableMs<3000||usedBudget()>=2)return Event::None;
    const Id selected=choose(in,random);
    // Select once per opportunity. Re-rolling every loop while common clips
    // cool down would turn a 10% rare chance into a near-certain rare clip.
    if(!valid(selected)||in.sleepRemainingMs<duration(selected)+1000){discard(random);return Event::None;}
    pending=false;active=selected;demo=false;started=now;
    for(unsigned i=0;i<2;i++)if(!occupied[i]||awake>=releaseAt[i]){
      occupied[i]=true;releaseAt[i]=UINT64_MAX;budgetSlot=i;break;
    }
    return Event::Started;
  }
  bool startDemo(Id id,uint32_t now,const Inputs& in,uint32_t random){
    // Explicit diagnostic preview bypasses rarity/quiet windows, never voice,
    // setup, rotation, power or input ownership. It does not spend auto quota.
    if(!valid(id)||!in.manualSafe||in.battery<0||in.battery>100||(!in.charging&&(in.battery<=20||batteryPaused)))return false;
    if(playing())finish(random);
    active=id;started=now;demo=true;pending=false;return true;
  }
  Event cancel(uint32_t random){if(!playing())return Event::None;finish(random);return Event::Cancelled;}

 private:
  bool initialized=false,wasAwake=false,pending=false,demo=false,batteryPaused=false;
  uint32_t lastMs=0,started=0,pendingSince=0;
  uint64_t awake=0,nextAt=0,coolUntil[Count]={},releaseAt[2]={};
  bool occupied[2]={};unsigned budgetSlot=0;
  Id active=Id::None,previous=Id::None;
  static uint32_t longWait(uint32_t r){return 20*Minute+r%(20*Minute+1);}
  void discard(uint32_t r){pending=false;nextAt=awake+5*Minute+r%(5*Minute+1);}
  void finish(uint32_t r){
    if(!demo){
      releaseAt[budgetSlot]=awake+Hour;
      coolUntil[uint8_t(active)]=awake+(rare(active)?4:2)*uint64_t(Hour);
      previous=active;nextAt=awake+longWait(r);
    }
    active=Id::None;demo=false;
  }
  Id pick(bool unusual,const Inputs& in,uint32_t random)const{
    unsigned total=0,weights[Count]={};
    for(unsigned i=0;i<Count;i++){
      const Id id=Id(i);if(rare(id)!=unusual||id==previous||cooling(id))continue;
      weights[i]=((id==Id::Repair&&in.preferRepair)||(id==Id::Gravity&&in.preferGravity))?2:1;
      total+=weights[i];
    }
    if(!total)return Id::None;
    unsigned ticket=random%total;
    for(unsigned i=0;i<Count;i++){if(ticket<weights[i])return Id(i);ticket-=weights[i];}
    return Id::None;
  }
  Id choose(const Inputs& in,uint32_t random)const{
    const uint32_t ticket=(random>>8)^(random*1664525u);
    if(awake>=Hour&&random%100<10){const Id id=pick(true,in,ticket);if(valid(id))return id;}
    return pick(false,in,ticket);
  }
};
}
