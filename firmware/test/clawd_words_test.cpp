#include "../src/clawd_words_render.h"
#include "../src/idle_power.h"
#include <assert.h>
#include <stdio.h>
#include <set>
#include <string>
#include <vector>
using namespace ClawdWords;
Gate safe(){Gate g;g.awake=g.portrait=g.healthy=true;return g;}
std::string joined(const Text& t){return std::string(t.line[0])+"\n"+t.line[1];}
struct PixelCanvas {
  unsigned marks=0;
  void fillRect(int x,int y,int w,int h,uint16_t){
    assert(x>=0&&x+w<=135&&y>=158&&y+h<=205);marks++;
  }
  void drawRect(int x,int y,int w,int h,uint16_t c){
    fillRect(x,y,w,1,c);fillRect(x,y+h-1,w,1,c);fillRect(x,y,1,h,c);fillRect(x+w-1,y,1,h,c);
  }
};
int main(){
  {
    std::set<std::string> unique;
    assert(DailyCount==384);
    for(unsigned id=0;id<DailyCount;id++){
      const auto t=daily(id);assert(unique.insert(joined(t)).second);
      for(auto& line:t.line){assert(strlen(line)>0&&strlen(line)<=20);
        for(const char* p=line;*p;p++)assert(*p>=32&&*p<=126);
      }
      assert(t.line[0][0]>='A'&&t.line[0][0]<='Z');
      assert(t.line[1][strlen(t.line[1])-1]=='.');
    }
    for(unsigned e=1;e<EventCount;e++)for(unsigned v=0;v<Variations;v++){
      const auto& x=exchange(Event(e),v);
      for(auto lines:{x.before,x.said,x.after})for(auto line:{lines.first,lines.second}){
        assert(strlen(line)>0&&strlen(line)<=20);
        for(const char* p=line;*p;p++)assert(*p>=32&&*p<=126);
      }
    }
    assert(joined(daily(0))=="I hid a tiny star\nin my pocket.");
    assert(command("poke")==Event::Poke&&command("catch")==Event::Catch);
    assert(command("unknown")==Event::None);
  }
  {
    // Every actual interaction returns to its own private afterthought.
    for(unsigned e=1;e<EventCount;e++)for(unsigned variant=0;variant<Variations;variant++){
      Policy p;auto g=safe();p.update(100,g,Event(e),variant);
      assert(p.talking()&&p.event()==Event(e));
      Text expected;expected.set(exchange(Event(e),variant).said);assert(joined(p.text())==joined(expected));
      p.update(4019,g,Event::None,0);assert(p.talking());
      p.update(4020,g,Event::None,0);assert(!p.talking()&&p.event()==Event(e));
      expected.set(exchange(Event(e),variant).after);assert(joined(p.text())==joined(expected));
      p.update(5000,g,Event::Poke,0);assert(!p.talking());
      p.update(12100,g,Event::None,0);assert(!p.talking()); // no delayed replay
      p.update(12101,g,Event::Poke,0);assert(p.talking());
    }
  }
  {
    // Takeovers cancel both text and pending speech, including manual previews.
    bool Gate::* blockers[]={&Gate::voice,&Gate::preparing,&Gate::inverted,&Gate::rotation,
      &Gate::game,&Gate::setup,&Gate::operation,&Gate::gesture,&Gate::story};
    for(auto member:blockers){
      Policy p;auto g=safe();assert(p.preview(Event::Poke,true,100,g));g.*member=true;
      assert(!p.preview(Event::Wake,true,101,g));p.update(101,g,Event::Wake,0);
      assert(!p.visible()&&!p.isPreview());g.*member=false;p.update(102,g,Event::None,0);
      assert(p.visible()&&!p.talking()&&p.event()==Event::None);
    }
    for(auto member:{&Gate::awake,&Gate::portrait,&Gate::healthy}){
      Policy p;auto g=safe();p.update(0,g,Event::Poke,0);g.*member=false;
      p.update(1,g,Event::None,0);assert(!p.visible());assert(!p.preview(Event::Poke,true,2,g));
    }
    Policy p;auto g=safe();assert(!p.preview(Event::None,true,0,g));
    assert(p.preview(Event::Poke,true,100,g));p.update(8099,g,Event::None,0);assert(p.isPreview());
    p.update(8100,g,Event::None,0);assert(!p.isPreview()&&!p.talking());
    p.update(8101,g,Event::Wake,0);assert(p.talking()); // demo doesn't consume real cooldown
    assert(p.preview(Event::Poke,false,9000,g));p.stopPreview(9001,1);assert(!p.isPreview()&&!p.talking());
  }
  {
    Policy p;auto g=safe();p.update(0,g,Event::None,0);const auto first=p.revision();
    p.update(20001,g,Event::None,1);assert(p.revision()==first+1);
    p.update(100000,g,Event::None,2);assert(p.revision()==first+1); // one automatic change per quiet spell
    // Exercise selection under repeated and pseudorandom seeds: no duplicate
    // in 64 picks, no three matching templates, no noun reuse in three picks.
    for(unsigned pass=0;pass<2;pass++){
      Policy q;std::vector<unsigned> ids;uint32_t random=7;
      for(unsigned i=0;i<3000;i++){
        if(pass)random=random*1664525u+1013904223u;
        g.awake=false;q.update(i*10,g,Event::None,random);g.awake=true;q.update(i*10+1,g,Event::None,random);
        const unsigned id=q.id();
        for(unsigned n=ids.size()>64?ids.size()-64:0;n<ids.size();n++)assert(ids[n]!=id);
        if(i>=2)assert(group(id)!=group(ids[i-1])||group(id)!=group(ids[i-2]));
        if(topic(id)!=255)for(unsigned n=i>3?i-3:0;n<i;n++)assert(topic(id)!=topic(ids[n]));
        ids.push_back(id);
      }
    }
  }
  {
    Policy p;auto g=safe();p.update(0xffffff00u,g,Event::Poke,0);
    assert(p.entry(0xfffffeffu)==0);assert(p.entry(64)==1);
    p.update(3663,g,Event::None,0);assert(p.talking());
    p.update(3664,g,Event::None,0);assert(!p.talking());
    // Words don't refresh existing sleep / power-off deadlines.
    IdlePowerPolicy power;power.activity(0);
    for(uint32_t now=0;now<30001;now+=100)p.update(now,g,Event::None,now);
    assert(petIdleSleepDue(30001,power.lastActivity,31,false,false,false));
    assert(power.update(300000,false,false,true)==IdlePowerPolicy::Action::Sleep);
    assert(power.update(600000,false,true,true)==IdlePowerPolicy::Action::None);
    assert(power.deepSleepDue(power.elapsed(600000)));
    assert(power.update(1200000,false,true,true)==IdlePowerPolicy::Action::PowerOff);
  }
  {
    // Both complete lines retain sentence case, one amber ink and centered
    // measured advances. No border/text can reach Clawd, footer or connection row.
    for(bool talk:{false,true})for(unsigned elapsed:{0,1,84,224,319,320,3000}){
      Policy p;assert(p.preview(Event::Poke,talk,100,safe()));PixelCanvas canvas;unsigned lines=0;
      draw(canvas,[&](const char* line,int x,int y,uint16_t ink){
        assert(ink==0x1234);assert(!strcmp(line,p.text().line[lines]));
        const int w=int(strlen(line))*6;assert(x>=4&&x+w<=131&&abs(x-(135-x-w))<=1);
        assert(y>=169&&y+16<=205);lines++;
      },[](const char* line){return int(strlen(line))*6;},p,100+elapsed,0x1234,0x5678,0);
      assert(lines==(elapsed==0?0:elapsed<84?1:2));
      assert(talk?canvas.marks>0:canvas.marks==0);
    }
  }
  puts("Clawd words: 384 unique thoughts, paired dialogue, interruptions, history, wrap, power and layout passed");
}
