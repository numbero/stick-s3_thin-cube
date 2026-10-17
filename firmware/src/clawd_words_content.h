#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace ClawdWords {
enum class Event : uint8_t { None, Poke, Pickup, Wake, Charge, Game, Catch, Count };
struct Lines { const char* first; const char* second; };
struct Text {
  char line[2][21]{};
  void set(Lines value){snprintf(line[0],sizeof(line[0]),"%s",value.first);snprintf(line[1],sizeof(line[1]),"%s",value.second);}
};
struct Exchange { Lines before,said,after; };
constexpr unsigned EventCount=unsigned(Event::Count),Variations=3;
inline bool valid(Event event){return event>Event::None&&event<Event::Count;}
inline const char* name(Event event){
  static const char* const names[]={"none","poke","pickup","wake","charge","game","catch"};
  return unsigned(event)<EventCount?names[unsigned(event)]:"none";
}
inline Event command(const char* text){
  for(unsigned i=1;i<EventCount;i++)if(!strcmp(text,name(Event(i))))return Event(i);
  return Event::None;
}
inline const Exchange& exchange(Event event,unsigned variation){
  static const Exchange items[EventCount-1][Variations]={
    {
      {{"I was making plans.","Very small ones."},{"Hey, I was thinking!","Give me a second."},{"... That was nice.","Don't tell anyone."}},
      {{"My shadow is busy.","Doing shadow things."},{"Oh! Was that you?","My shadow jumped."},{"It wasn't my shadow.","It was me."}},
      {{"I have a tiny plan.","It needs a hat."},{"You found me!","I was almost hiding."},{"I left one foot out.","Just in case."}}
    },{
      {{"I wonder how far","this desk can go."},{"Where are we going?","I packed a star."},{"The desk got small.","Or I got brave."}},
      {{"A cloud looks lost.","It may need a map."},{"A little adventure?","I'll bring my cloud."},{"The cloud came too.","It travels light."}},
      {{"My feet have plans.","Quiet little plans."},{"Shall we explore?","My feet said yes."},{"They meant it.","All four of them."}}
    },{
      {{"I folded the night","into a small boat."},{"Oh, you're back!","I kept your spot."},{"I saved a tiny star.","Just in case."}},
      {{"I dreamt of a cat.","It had four corners."},{"Hello again!","Was I dreaming?"},{"The cat waved.","With all four feet."}},
      {{"The moon was here.","It forgot its hat."},{"What's next?","I'm ready to go."},{"I packed the hat.","The moon needs it."}}
    },{
      {{"One tiny spark left.","Saving it for later."},{"A little sunshine?","Thank you."},{"Saving this warmth","for our next trip."}},
      {{"My pocket is cold.","The star is napping."},{"Oh, a warm spot!","Can I stay a while?"},{"The star woke up.","It likes this spot."}},
      {{"My plans need fuel.","A very small amount."},{"Thanks for a spark!","I'll save you one."},{"One spark is yours.","The brave one."}}
    },{
      {{"I practiced my turn.","Inside my head."},{"One more round?","I have a tiny plan."},{"That was my plan.","Well, most of it."}},
      {{"Stars can turn left.","I saw it do that."},{"Did you see that?","We almost flew."},{"My feet stayed here.","The rest of me flew."}},
      {{"I counted the stars.","They kept moving."},{"Good game!","Time for a tiny nap?"},{"I kept one star.","For the next round."}}
    },{
      {{"That light is shy.","So am I, a little."},{"You caught it!","Can we keep it?"},{"It says my pocket","is warm enough."}},
      {{"A star needs a home.","A very small home."},{"We found a star!","You're quick!"},{"It picked my pocket.","I didn't even ask."}},
      {{"I wished for a star.","A pocket-sized one."},{"Look what we found!","It likes you."},{"It likes me too.","It said so quietly."}}
    }
  };
  return items[valid(event)?unsigned(event)-1:0][variation%Variations];
}

constexpr unsigned ThingCount=12,PlaceCount=10,GiftCount=8,EndingCount=10;
constexpr unsigned Combinations=ThingCount*(PlaceCount+GiftCount+EndingCount),CuratedCount=48;
constexpr unsigned DailyCount=Combinations+CuratedCount;
inline unsigned group(unsigned id){return id<ThingCount*PlaceCount?0:id<ThingCount*(PlaceCount+GiftCount)?1:id<Combinations?2:3;}
inline unsigned topic(unsigned id){
  return id<ThingCount*PlaceCount?id/PlaceCount:id<ThingCount*(PlaceCount+GiftCount)?(id-ThingCount*PlaceCount)/GiftCount:id<Combinations?(id-ThingCount*(PlaceCount+GiftCount))/EndingCount:255;
}
inline Text daily(unsigned id){
  static const char* const things[]={"a tiny star","a half moon","a blue cloud","a soft wind","an old dream","a small wish","a shy shadow","a paper boat","a raindrop","a slow comet","a small smile","a little sun"};
  static const char* const nouns[]={"star","moon","cloud","wind","dream","wish","shadow","boat","raindrop","comet","smile","sun"};
  static const char* const places[]={"in my pocket.","behind the screen.","under a small cloud.","inside tomorrow.","under my pillow.","in a paper boat.","inside a soft dream.","behind my shadow.","near your desk.","in a quiet corner."};
  static const char* const gifts[]={"my tiny hat.","a little chair.","one soft pillow.","a small cookie.","a bit of courage.","a spare pocket.","my paper wings.","a quiet minute."};
  static const char* const endings[]={"It won't sit still.","It lost its socks.","It can't turn left.","It likes your desk.","It waved at me.","It hid in a cloud.","It has big plans.","It is very polite.","It asked for a hat.","It thinks I'm tall."};
  static const Lines curated[]={
    {"I was making plans.","Very small ones."},{"I dreamt of a cat.","It had four corners."},
    {"The moon was here.","It forgot its hat."},{"I folded the night","into a small boat."},
    {"My shadow is late.","It took a long way."},{"A cloud needs a map.","I drew it a circle."},
    {"I counted my feet.","Still four. Good."},{"A wish sat down.","It had a long day."},
    {"I found a door.","It leads to a cloud."},{"My hat has a dream.","It dreams of boats."},
    {"This desk is a sea.","My feet are islands."},{"I kept a soft word.","For a rainy day."},
    {"A star said hello.","I got a little shy."},{"I trust my shadow.","Most of the time."},
    {"A small wish asked","for a little chair."},{"I lost a tiny worry.","I won't look for it."},
    {"My pocket is brave.","It holds the dark."},{"I drew a small moon.","It drew me a smile."},
    {"Tomorrow looks far.","Small steps for me."},{"A comet paused.","It wanted to say hi."},
    {"A cloud wore my hat.","It looked so proud."},{"My feet are quiet.","My plans are loud."},
    {"I hid a tiny laugh.","It keeps escaping."},{"A dream moved in.","It brought a pillow."},
    {"The stars are small.","So am I. It helps."},{"I have paper wings.","For indoor flying."},
    {"My boat has no sea.","It is very patient."},{"My shadow waved.","I waved four feet."},
    {"I saved a spark.","For a brave day."},{"My wish grew a leaf.","I gave it a window."},
    {"The wind has a bag.","Full of lost hats."},{"I met a quiet echo.","It let me go first."},
    {"A sun took a nap.","Under my tiny hat."},{"My star has a plan.","It won't tell me."},
    {"I like this corner.","It knows my shape."},{"A raindrop stopped.","To admire the view."},
    {"I put a wish away.","It needed some rest."},{"A cloud followed me.","Very politely."},
    {"My courage is small.","It fits in a pocket."},{"I found half a moon.","The shy half."},
    {"A little idea grew.","It needs a big hat."},{"I lit a small star.","It likes to share."},
    {"My desk has a shore.","Only I can see it."},{"I drew a path home.","It looks like you."},
    {"A wish knocked.","I made room for it."},{"The night is soft.","Like a folded cloud."},
    {"I tried to be tall.","My shadow helped."},{"A thought waved.","I think it was mine."}
  };
  static_assert(sizeof(curated)/sizeof(*curated)==CuratedCount,"Count every reviewed thought");
  Text text;id%=DailyCount;const unsigned kind=group(id),noun=topic(id);
  if(kind==0){snprintf(text.line[0],sizeof(text.line[0]),"I hid %s",things[noun]);snprintf(text.line[1],sizeof(text.line[1]),"%s",places[id%PlaceCount]);}
  else if(kind==1){snprintf(text.line[0],sizeof(text.line[0]),"My %s found",nouns[noun]);snprintf(text.line[1],sizeof(text.line[1]),"%s",gifts[(id-ThingCount*PlaceCount)%GiftCount]);}
  else if(kind==2){snprintf(text.line[0],sizeof(text.line[0]),"I met %s.",things[noun]);snprintf(text.line[1],sizeof(text.line[1]),"%s",endings[(id-ThingCount*(PlaceCount+GiftCount))%EndingCount]);}
  else text.set(curated[id-Combinations]);
  return text;
}
}
