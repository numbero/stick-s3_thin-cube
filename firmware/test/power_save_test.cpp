#include "../src/power_save_policy.h"
#include "../src/voice_state.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
int main(){
  {
    BLEPowerPolicy p;
    for(uint16_t interval:{uint16_t(12),uint16_t(24)}){
      p.connected(interval);assert(p.audioReady());assert(!p.requestDue(0,false));
    }
    p.connected(72);assert(!p.audioReady());assert(p.requestDue(0,false));
    p.requested(0,true);assert(!p.audioReady());p.observed(24,true);assert(p.audioReady());
    for(uint16_t invalid:{uint16_t(0),uint16_t(5),uint16_t(3201),uint16_t(65535)}){
      p.connected(invalid);assert(p.actual==0&&!p.audioReady());assert(p.requestDue(0,false));
    }
    p.connected(24);p.requestDue(100,true);p.requested(100,true);p.observed(72,true);
    assert(!p.audioReady());p.requestDue(200,false);p.requested(200,true);assert(!p.audioReady());
    p.observed(24,true);assert(p.audioReady());p.connected(0);assert(!p.audioReady());
  }
  {
    BLEPowerPolicy p;assert(!p.audioReady());assert(p.requestDue(0,false));p.requested(0,true);
    p.observed(18,true);assert(p.audioReady());assert(!p.requestDue(100,false));
    assert(p.requestDue(200,true));p.requested(200,true);assert(!p.audioReady());
    assert(!p.requestDue(210,false));p.observed(72,true);assert(!p.audioReady());
    assert(p.requestDue(220,false));p.requested(220,true);assert(!p.audioReady());
    p.observed(18,true);assert(p.audioReady());assert(!p.requestDue(230,false));
  }
  {
    BLEPowerPolicy p;p.observed(18,true);assert(p.requestDue(100,true));p.requested(100,true);
    p.observed(18,false);assert(!p.requestDue(200,true));assert(p.audioReady()&&p.idleDisabled);
    p.reset();p.observed(18,true);assert(p.requestDue(0,true));p.requested(0,true);
    assert(!p.requestDue(1999,true));assert(p.requestDue(2000,true));
    p.requested(2000,true);assert(!p.requestedIdle&&p.idleDisabled);
    p.observed(18,true);assert(p.audioReady());
    p.reset();p.observed(18,true);assert(p.requestDue(100,true));p.requested(100,true);
    p.observed(40,true);assert(p.idleDisabled&&!p.audioReady());assert(p.requestDue(200,true));
    p.requested(200,true);p.observed(24,true);assert(p.audioReady());
  }
  {
    VoiceState v;v.allowed=v.peer=true;v.leaseAt=100;v.captureReady=false;
    assert(v.requestManual(false,100));assert(v.pendingManual&&!v.streaming);
    v.tick(200);assert(!v.streaming);v.captureReady=true;v.tick(300);assert(v.streaming&&v.manual);
    v.stop();v.captureReady=false;assert(v.requestManual(false,400));v.setPeer(false,401);
    assert(!v.pendingManual&&!v.streaming);
    v.setPeer(true,1000);assert(v.requestManual(false,1000));
    for(unsigned ms=1200;ms<6000;ms+=200){v.setPeer(true,ms);v.tick(ms);}
    v.setPeer(true,6000);v.tick(6000);assert(!v.pendingManual&&!v.streaming);
    v.setPeer(true,7000);v.inverted=true;v.pendingGesture=true;
    assert(v.requestManual(false,7000));v.setPose(false,false);v.captureReady=true;v.tick(7100);
    assert(v.streaming&&v.manual&&!v.pendingGesture);
  }
  using D=DeepSleepPolicy::Decision;
  {
    DeepSleepPolicy p;p.begin(100,42,true);assert(!p.ack(41));assert(p.update(500,true,true)==D::Wait);
    assert(p.ack(42));assert(p.update(501,true,true)==D::Commit);
    p.begin(1000,43,true);assert(p.update(2999,true,true)==D::Wait);
    assert(p.update(3000,true,true)==D::Cancel&&!p.pending);
    p.begin(4000,44,false);assert(p.update(4349,true,false)==D::Wait);
    assert(p.update(4350,true,false)==D::Commit);
    p.begin(5000,45,false);assert(p.update(5350,true,true)==D::Wait);
    assert(p.ack(45));assert(p.update(5351,true,true)==D::Commit);
    p.begin(6000,46,true);p.ack(46);assert(p.update(6350,true,false)==D::Cancel);
    p.begin(7000,47,true);p.ack(47);assert(p.update(7350,false,true)==D::Cancel);
    p.begin(0xfffffff0u,48,false);assert(p.update(0x160,true,false)==D::Commit);
  }
  puts("idle BLE negotiation/timeout, audio readiness/manual intent and sleep ACK/admission scenarios passed");
}
