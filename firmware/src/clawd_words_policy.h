#pragma once
#include "clawd_words_content.h"
#include "ui_timing.h"
#include <algorithm>

namespace ClawdWords {
struct Gate {
  bool awake=false,portrait=false,healthy=false;
  bool voice=false,preparing=false,inverted=false,rotation=false,game=false;
  bool setup=false,operation=false,gesture=false,story=false;
  bool allowed()const{return awake&&portrait&&healthy&&!voice&&!preparing&&!inverted&&!rotation&&!game&&!setup&&!operation&&!gesture&&!story;}
};
enum class Stage : uint8_t { Hidden, Thought, Talk };
class Policy {
 public:
  static constexpr uint32_t EntryMs=320,TalkMs=3600,CooldownMs=12000,PreviewMs=8000;
  void begin(){*this=Policy();}
  Stage stage()const{return current;}
  bool visible()const{return current!=Stage::Hidden;}
  bool talking()const{return current==Stage::Talk;}
  bool isPreview()const{return previewing;}
  Event event()const{return context;}
  unsigned id()const{return dailyId;}
  unsigned revision()const{return changed;}
  const Text& text()const{return shown;}
  bool canSpeak(uint32_t now)const{return !talking()&&(!hasSpoken||petStateElapsed(now,lastTalk)>=CooldownMs);}
  void hide(){if(visible())changed++;current=Stage::Hidden;previewing=false;context=Event::None;}
  bool say(Lines said,Lines after,uint32_t now,const Gate& gate,bool followup=false){
    if(!gate.allowed()||(!followup&&!canSpeak(now)))return false;
    shown.set(said);later.set(after);current=Stage::Talk;context=Event::None;
    enteredAt=lastTalk=now;hasSpoken=true;previewing=false;changed++;return true;
  }
  float entry(uint32_t now)const{return float(std::min(uint32_t(EntryMs),petStateElapsed(now,enteredAt)))/EntryMs;}
  void update(uint32_t now,const Gate& gate,Event event,uint32_t random){
    if(!gate.allowed()){
      if(visible())changed++;
      current=Stage::Hidden;previewing=false;context=Event::None;return;
    }
    if(!visible()){pick(now,random);autoUsed=false;}
    if(valid(event)){
      if(previewing){pick(now,random);previewing=false;}
      if(canSpeak(now)){
        unsigned choice=random%Variations;
        if(lastChoice[unsigned(event)]==choice)choice=(choice+1)%Variations;
        lastChoice[unsigned(event)]=choice;
        const auto& lines=exchange(event,choice);shown.set(lines.said);later.set(lines.after);
        current=Stage::Talk;context=event;enteredAt=lastTalk=now;hasSpoken=true;changed++;
      }
    }
    if(previewing){if(petStateElapsed(now,enteredAt)>=PreviewMs){previewing=false;pick(now,random);}return;}
    if(talking()&&petStateElapsed(now,enteredAt)>=EntryMs+TalkMs){
      shown=later;current=Stage::Thought;enteredAt=now;holdMs=12000+random%8001;autoUsed=false;changed++;
    }else if(current==Stage::Thought&&!autoUsed&&petStateElapsed(now,enteredAt)>=holdMs){
      pick(now,random);autoUsed=true;
    }
  }
  bool preview(Event event,bool talk,uint32_t now,const Gate& gate){
    if(!gate.allowed()||!valid(event))return false;
    const auto& lines=exchange(event,0);shown.set(talk?lines.said:lines.before);later.set(lines.after);
    current=talk?Stage::Talk:Stage::Thought;context=event;enteredAt=now;previewing=true;changed++;return true;
  }
  void stopPreview(uint32_t now,uint32_t random){if(previewing){previewing=false;if(visible())pick(now,random);}}
 private:
  Stage current=Stage::Hidden;Event context=Event::None;
  Text shown,later;
  bool previewing=false,hasSpoken=false,autoUsed=false;
  uint32_t enteredAt=0,lastTalk=0,holdMs=12000;
  unsigned dailyId=0,changed=0,count=0,head=0,lastGroup=255,previousGroup=255;
  uint16_t history[64]{};
  uint8_t topics[3]={255,255,255},lastChoice[EventCount]={255,255,255,255,255,255,255};
  bool recent(unsigned id)const{for(unsigned i=0;i<count;i++)if(history[i]==id)return true;return false;}
  void pick(uint32_t now,uint32_t random){
    unsigned selected=0;
    for(unsigned i=0;i<DailyCount;i++){
      const unsigned id=(random%DailyCount+i*73)%DailyCount,kind=group(id),noun=topic(id);
      if(recent(id)||(lastGroup==previousGroup&&kind==lastGroup))continue;
      if(noun!=255&&(noun==topics[0]||noun==topics[1]||noun==topics[2]))continue;
      selected=id;break;
    }
    dailyId=selected;shown=daily(selected);history[head]=selected;head=(head+1)%64;if(count<64)count++;
    previousGroup=lastGroup;lastGroup=group(selected);topics[2]=topics[1];topics[1]=topics[0];topics[0]=topic(selected);
    current=Stage::Thought;context=Event::None;enteredAt=now;holdMs=12000+(random>>8)%8001;changed++;
  }
};
}
