#pragma once
#include "clawd_words_policy.h"
#include <cmath>

namespace ClawdLight {
enum class Stage : uint8_t { Idle, Invite, Fold, Play, Store };
enum class Event : uint8_t { None, Stored, Missed, Cancelled };
inline const char* name(Stage stage){
  static const char* const names[]={"idle","invite","fold","play","store"};return names[unsigned(stage)];
}
constexpr ClawdWords::Lines Invitation={"I dropped a thought.","Catch it gently?"};
constexpr ClawdWords::Lines Caught={"You caught it!","Can we keep it?"};
constexpr ClawdWords::Lines Trusted={"A star trusted me.","I'll keep it warm."};
constexpr ClawdWords::Lines Missed={"It got shy.","Maybe next time."};
constexpr ClawdWords::Lines Later={"Maybe it needs time.","So do I, sometimes."};
constexpr const char* Hint="Tilt gently.";
// In portrait, positive sensor X is a leftward screen tilt on this board.
inline float tiltDegrees(float x,float y,float z){
  if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||x*x+y*y+z*z<.01f)return NAN;
  return -std::atan2(x,std::hypot(y,z))*57.2957795f;
}
class Policy {
 public:
  static constexpr uint32_t CooldownMs=60000,PlayMs=8000,HoldMs=1200,FoldMs=260,StoreMs=600;
  static constexpr uint32_t InviteMs=ClawdWords::Policy::EntryMs+ClawdWords::Policy::TalkMs;
  bool active()const{return phase!=Stage::Idle;}
  bool locksOrientation()const{return active();}
  bool playing()const{return phase==Stage::Play;}
  bool drawingLight()const{return phase==Stage::Play||phase==Stage::Store;}
  bool folding()const{return phase==Stage::Fold;}
  bool isDemo()const{return demo;}
  Stage stage()const{return phase;}
  float x()const{return position;}
  float velocity()const{return speed;}
  float progress()const{return held/HoldMs;}
  float fraction(uint32_t now)const{
    const uint32_t duration=folding()?FoldMs:StoreMs;
    return std::min(1.f,float(petStateElapsed(now,stateAt))/duration);
  }
  bool frightened(uint32_t now)const{return startled&&petStateElapsed(now,frightAt)<450;}
  float reach(uint32_t now)const{return frightened(now)?-.4f:phase==Stage::Store?1.f:.2f+progress()*.8f;}
  bool ready(uint32_t now)const{return !active()&&(!offered||petStateElapsed(now,offerAt)>=CooldownMs);}
  bool start(uint32_t now,const ClawdWords::Gate& gate,float tilt,bool preview=false,bool direct=false){
    if(!gate.allowed()||!std::isfinite(tilt)||active()||(!preview&&!ready(now)))return false;
    demo=preview;phase=Stage::Invite;stateAt=tickAt=now;position=90;speed=held=0;startled=false;base=tilt;
    if(!preview){offered=true;offerAt=now;}
    if(direct)beginPlay(now,tilt);
    return true;
  }
  bool cancel(){const bool was=active();phase=Stage::Idle;held=speed=0;return was;}
  bool tease(uint32_t now){if(!playing())return false;frighten(now,22);return true;}
  Event update(uint32_t now,const ClawdWords::Gate& gate,float tilt,bool strong=false){
    if(!active())return Event::None;
    if(!gate.allowed()||!std::isfinite(tilt)){cancel();return Event::Cancelled;}
    if(phase==Stage::Invite){if(petStateElapsed(now,stateAt)>=InviteMs){phase=Stage::Fold;stateAt=now;}return Event::None;}
    if(phase==Stage::Fold){if(petStateElapsed(now,stateAt)>=FoldMs)beginPlay(now,tilt);return Event::None;}
    if(phase==Stage::Store){if(petStateElapsed(now,stateAt)>=StoreMs){phase=Stage::Idle;return Event::Stored;}return Event::None;}
    if(petStateElapsed(now,stateAt)>=PlayMs){phase=Stage::Idle;held=0;return Event::Missed;}
    const float dt=std::min(40u,petStateElapsed(now,tickAt))*.001f;tickAt=now;
    if(dt<=0)return Event::None;
    const float target=std::max(-18.f,std::min(18.f,tilt-base));
    smoothed+=(target-smoothed)*dt/(.08f+dt);
    const float angular=std::abs(smoothed-lastTilt)/dt;lastTilt=smoothed;
    if((strong||angular>70)&&(!startled||petStateElapsed(now,frightAt)>=700))frighten(now,15);
    // Calm hands invite it closer. A small dead band rejects hand tremor;
    // tilt still leads the light, while a fright briefly suspends its trust.
    const float magnitude=std::abs(smoothed);
    const float strength=std::min(1.f,std::max(0.f,magnitude-2)/12);
    const float drive=(smoothed<0?-1:1)*24*strength*strength;
    const bool calm=magnitude<4&&angular<25&&!frightened(now);
    // Only neutral hands invite it inward; a deliberate tilt always leads in
    // that direction. The quadratic curve keeps small corrections slow, caps
    // travel at 24 px/s, and damps momentum when the hand returns to neutral.
    const float approach=calm&&magnitude<=2?(67.5f-position)*.36f:0;
    speed+=(drive+approach-speed)*5.5f*dt;position+=speed*dt;
    if(position<14){position=14;speed=std::abs(speed)*.35f;}
    if(position>121){position=121;speed=-std::abs(speed)*.35f;}
    if(std::abs(position-67.5f)<8&&std::abs(speed)<8&&calm)held+=dt*1000;
    else held=0;
    if(held>=HoldMs){held=HoldMs;phase=Stage::Store;stateAt=now;}
    return Event::None;
  }
 private:
  Stage phase=Stage::Idle;
  bool offered=false,demo=false,startled=false;
  uint32_t offerAt=0,stateAt=0,tickAt=0,frightAt=0;
  float position=90,speed=0,held=0,base=0,smoothed=0,lastTilt=0;
  void beginPlay(uint32_t now,float tilt){
    phase=Stage::Play;stateAt=tickAt=now;speed=held=0;position=90;
    // Keep the A-press reference: moving during the invitation must not silently
    // redefine neutral just as the star appears.
    smoothed=lastTilt=std::max(-18.f,std::min(18.f,tilt-base));
  }
  void frighten(uint32_t now,float impulse){speed+=(position>=67.5f?1:-1)*impulse;held=0;startled=true;frightAt=now;}
};
}
