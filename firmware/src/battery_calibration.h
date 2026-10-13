#pragma once
#include <stdint.h>

// RAM-only opt-in protocol. A reboot cancels it. Alternate display load while
// keeping BLE, CPU, animation and silent audio policy fixed across both runs.
class BatteryCalibration {
 public:
  enum State { Off, Charging, Ready, Settling, Running, Complete, Aborted };
  enum Reason { None, Cancelled, Uncharged, Plugged, LinkLost, ReadLost, Timeout, Movement, LowVoltage };
  static constexpr uint32_t ChargeStableMs=180000, SettleMs=15000, PhaseMs=60000;
  static constexpr uint32_t MaxChargeMs=4*60*60*1000, MaxRunMs=6*60*60*1000, FinalMs=10000;
  static constexpr int DimBrightness=12, AwakeBrightness=134, EndMv=3300;

  void arm(uint32_t now,uint32_t run){
    *this=BatteryCalibration();state_=Charging;id_=run?run:1;armedAt_=validAt_=linkedAt_=sourceAt_=now;
  }
  void cancel(Reason reason=Cancelled){if(ownsUI()&&state_!=Complete){state_=Aborted;reason_=reason;}}
  void update(uint32_t now,int raw,int median,int source,bool chargeKnown,bool charging,bool linked){
    if(!ownsUI()||state_==Complete)return;
    if(linked)linkedAt_=now;
    if(source==0||source==1)sourceAt_=now;
    if(uint32_t(now-sourceAt_)>=10000){cancel(ReadLost);return;}
    if(uint32_t(now-linkedAt_)>=15000){cancel(LinkLost);return;}
    const bool valid=raw>=2500&&raw<=4500;
    if(valid)validAt_=now;
    if(uint32_t(now-validAt_)>=10000){cancel(ReadLost);return;}
    if(state_==Charging||state_==Ready){
      if(uint32_t(now-armedAt_)>=MaxChargeMs){cancel(Timeout);return;}
      if(source==0){
        if(state_==Ready){state_=Settling;settleAt_=now;stable_=false;}
        else cancel(Uncharged);
        return;
      }
      if(source!=1||!chargeKnown||charging||!valid||median<3300){stable_=false;state_=Charging;return;}
      if(!stable_||median<stableMin_-20||median>stableMax_+20){
        stable_=true;stableAt_=now;stableMin_=stableMax_=median;state_=Charging;
      }
      if(median<stableMin_)stableMin_=median;
      if(median>stableMax_)stableMax_=median;
      if(stableMax_-stableMin_>20){stableAt_=now;stableMin_=stableMax_=median;state_=Charging;}
      if(linked&&uint32_t(now-stableAt_)>=ChargeStableMs)state_=Ready;
      return;
    }
    if(source==1){cancel(Plugged);return;}
    if(state_==Settling){
      if(source==0&&valid&&linked&&uint32_t(now-settleAt_)>=SettleMs){state_=Running;startedAt_=now;}
      return;
    }
    if(uint32_t(now-startedAt_)>=MaxRunMs){cancel(Timeout);return;}
    // Endpoint measured under the bright phase after it has settled; transient
    // low readings or the dim phase cannot prematurely end the reference run.
    if(source==0&&valid&&phase(now)==1&&phaseAge(now)>=10000&&raw<=EndMv&&median>=2500&&median<=EndMv){
      if(lowCount_<5)++lowCount_;
      if(lowCount_>=5){state_=Complete;reason_=LowVoltage;endedAt_=now;}
    }else lowCount_=0;
  }
  bool ownsUI() const {return state_>=Charging&&state_<=Complete;}
  bool active() const {return state_>=Charging&&state_<=Running;}
  State state()const{return state_;}
  Reason reason()const{return reason_;}
  uint32_t id()const{return id_;}
  uint32_t elapsed(uint32_t now)const{return state_==Complete?uint32_t(endedAt_-startedAt_):state_==Running?uint32_t(now-startedAt_):0;}
  uint32_t phaseAge(uint32_t now)const{return elapsed(now)%PhaseMs;}
  int phase(uint32_t now)const{return int((elapsed(now)/PhaseMs)%2);}
  int brightness(uint32_t now)const{return state_==Complete?0:state_==Running&&phase(now)==1?AwakeBrightness:DimBrightness;}
  uint32_t chargeStable(uint32_t now)const{return stable_?uint32_t(now-stableAt_):0;}
  bool shutdownDue(uint32_t now)const{return state_==Complete&&shutdownAttempts_<3&&uint32_t(now-endedAt_)>=FinalMs+shutdownAttempts_*10000;}
  void shutdownAttempted(){++shutdownAttempts_;}
 private:
  State state_=Off;Reason reason_=None;
  uint32_t id_=0,armedAt_=0,stableAt_=0,settleAt_=0,startedAt_=0,endedAt_=0,validAt_=0,linkedAt_=0,sourceAt_=0;
  int stableMin_=0,stableMax_=0;
  unsigned lowCount_=0,shutdownAttempts_=0;
  bool stable_=false;
};
