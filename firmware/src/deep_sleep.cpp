#include "deep_sleep.h"
#include "imu_service.h"
#include "motion_wake_registers.h"
#include "deep_sleep_timer.h"
#include "power_save_policy.h"
#include "idle_power.h"
#include "power_save.h"
#include "power_status.h"
#include "device.h"
#include "ui.h"
#include "audio_input.h"
#include "sound_effects.h"
#include "transport.h"
#include "ota.h"
#include "maintenance.h"
#include "wifi_prov.h"
#include <M5Unified.h>
#include <esp_system.h>
#include <atomic>

namespace {
struct Bus {
  bool bmiRead(uint8_t r,uint8_t* v,size_t n){return imuRead(r,v,n);}
  bool bmiWrite(uint8_t r,const uint8_t* v,size_t n){return imuWrite(r,v,n);}
  bool pmRead(uint8_t r,uint8_t* v,size_t n){return M5.Power.M5pm1.readRegister(r,v,n);}
  bool pmWrite(uint8_t r,uint8_t v){return M5.Power.M5pm1.writeRegister(r,&v,1);}
  void pause(unsigned ms){delay(ms);}
};
#ifdef THINCUBE_IMU_LEGACY
using WakeRegisters=MotionWakeRegisters<Bus,true>;
#else
using WakeRegisters=MotionWakeRegisters<Bus>;
#endif
DeepSleepPolicy policy;
using SleepTimer=DeepSleepTimer<Bus>;
std::atomic<bool> pending{false};
bool available=false,timerReady=false;
bool cancelTimer(Bus& bus){
  bool ok=false;
  for(unsigned n=0;n<3&&!ok;++n)ok=SleepTimer::cancel(bus);
  return ok;
}
bool inputSafe(uint32_t now){
  return !otaActive()&&!maintenanceActive()&&!maintenancePending()&&!provActive()&&!transportBusy()&&
    !devicePairWindow()&&!deviceStreaming()&&!soundPlaying()&&powerBatteryLevel(now)>=0&&
    !powerBatteryLow(now)&&powerConfirmAutoOff();
}
void failed(Bus& bus,WakeRegisters& registers){
  const bool cancelled=cancelTimer(bus);
  timerReady=cancelled;
  const bool restored=registers.restore(bus);available=available&&restored&&cancelled;
  if(!restored){
    const bool normal=imuBegin();
    Serial.printf("[deep-sleep] normal IMU reinitialize %d\n",normal);
  }else imuResumeAfterSleep();
  Serial.printf("[deep-sleep] cancelled restore %d available %d\n",restored,available);
  M5.Display.wakeup();deepSleepCancel();deviceSleepChanged();
}
}
void deepSleepBoot(bool userBoot){
  Bus bus;const auto boot=SleepTimer::consumeBoot(bus);
  timerReady=boot.clean||cancelTimer(bus);
  if(userBoot||!boot.expired||!timerReady||!SleepTimer::mayPowerOff(bus))return;
  M5.Display.setBrightness(0);
  bool disarmed=false;
  for(unsigned n=0;n<3&&!disarmed;++n)disarmed=WakeRegisters::disarm(bus);
  if(!disarmed){timerReady=false;return;}
  Serial.println("[deep-sleep] 20m idle deadline reached; full power-off before UI/radio startup");
  for(unsigned n=0;n<3&&SleepTimer::mayPowerOff(bus);++n){
    M5.Power.M5pm1.powerOff();delay(200);
  }
  // A successful physical cut never returns. Power-off failure, newly inserted
  // USB or user input continues a usable normal boot with no old deadline.
  Serial.println("[deep-sleep] timed power-off returned; continuing normal boot");
}
void deepSleepBegin(){
  Bus bus;uint8_t wake=0;const bool read=bus.pmRead(0x05,&wake,1);
  bool clean=false;
  for(unsigned n=0;n<3&&!clean;++n)clean=WakeRegisters::disarm(bus);
  bool cleared=M5.Power.M5pm1.clearWakeSource(0x7f);
  cleared=M5.Power.M5pm1.clearGPIOIRQStatus()&&cleared;
  cleared=M5.Power.M5pm1.clearSystemIRQStatus()&&cleared;
  cleared=M5.Power.M5pm1.clearButtonIRQStatus()&&cleared;
  available=timerReady&&clean&&cleared&&imuEnabled()&&(imuLegacy()||M5.Imu.getType()==m5::imu_t::imu_bmi270);
  Serial.printf("[deep-sleep] wake_src %u read %d cleaned %d available %d\n",unsigned(wake),read,clean,available);
}
bool deepSleepAvailable(){return available;}
bool deepSleepPending(){return pending.load();}
uint32_t deepSleepSequence(){return policy.id;}
bool deepSleepAcknowledge(uint32_t sequence){return policy.ack(sequence);}
void deepSleepCancel(){
  if(policy.pending)policy.retryLater(millis());else policy.cancel();
  pending=false;
}
bool deepSleepDisarm(){
  Bus bus;const bool cancelled=cancelTimer(bus);
  timerReady=cancelled;
  const bool ok=WakeRegisters::disarm(bus)&&cancelled;if(!ok)available=false;return ok;
}
bool deepSleepDiagnostic(){
  if(!imuLegacy()||!available||deepSleepPending()||otaActive()||maintenanceActive()||maintenancePending()||provActive()||transportBusy()||
     devicePairWindow()||deviceStreaming()||deviceVoicePreparing()||M5.BtnA.isPressed()||M5.BtnB.isPressed()){
    Serial.println("[sleep-check] refused: idle healthy device required");return false;
  }
  powerSavePerformance();
  if(!imuSuspendForSleep()){imuResumeAfterSleep();Serial.println("[sleep-check] suspend failed");return false;}
  Bus bus;WakeRegisters registers;
  const bool prepared=registers.prepare(bus);
  const bool restored=registers.restore(bus);
  if(restored)imuResumeAfterSleep();else{available=false;imuBegin();}
  Serial.printf("[sleep-check] prepared=%d restored=%d tap=%d no_poweroff=1\n",prepared,restored,imuTapSnapshot().enabled);
  return prepared&&restored;
}
bool deepSleepTick(uint32_t now,bool safe,bool connected){
  if(!timerReady){
    // Keep retrying cleanup after a persistent bus error, so a later recovery
    // cannot leave an old PM1 timer armed during normal interactive use.
    static uint32_t retriedAt=0;
    if(uint32_t(now-retriedAt)>=1000){Bus bus;timerReady=cancelTimer(bus);retriedAt=now;}
  }
  if(uiIdleAgeMs(now)>=IdlePowerPolicy::OffMs){
    // Give an overdue attempt back to the online off path without resetting
    // activity. In particular, do not start a fresh ten-minute timer at 20m.
    const bool wasPending=policy.pending;policy.cancel();pending=false;
    if(wasPending)deviceSleepChanged();return false;
  }
  safe=safe&&available&&powerAutoOffAllowed(now)&&
    powerBatteryLevel(now)>=0&&!powerBatteryLow(now);
  if(!policy.pending){
    if(!safe||!policy.ready(now)||!IdlePowerPolicy::deepSleepDue(uiIdleAgeMs(now)))return false;
    policy.begin(now,esp_random(),connected);pending=true;
    powerSavePerformance();devicePause();deviceSleepChanged();
    Serial.printf("[deep-sleep] announcing id %u ack_required %d\n",unsigned(policy.id),connected);return true;
  }
  powerSavePerformance();const auto decision=policy.update(now,safe,connected);
  if(decision==DeepSleepPolicy::Decision::Cancel){pending=false;deviceSleepChanged();return true;}
  if(decision!=DeepSleepPolicy::Decision::Commit)return true;
  std::unique_lock<std::mutex> admission(transportAdmission(),std::try_to_lock);
  if(!admission.owns_lock())return true;
  if(!inputSafe(millis())){deepSleepCancel();deviceSleepChanged();return true;}
  devicePause();soundCancel();audioTick(false);M5.Speaker.end();
  if(!audioPowerDown()||!deviceSaveForSleep()){deepSleepCancel();deviceSleepChanged();return true;}
  if(!imuSuspendForSleep()){imuResumeAfterSleep();deepSleepCancel();deviceSleepChanged();return true;}
  Bus bus;WakeRegisters registers;
  if(!registers.prepare(bus)){failed(bus,registers);return true;}
  M5.Display.setBrightness(0);M5.Display.sleep();
  Serial.printf("[deep-sleep] entering PM1 L1 id %u\n",unsigned(policy.id));
  if(!inputSafe(millis())||!WakeRegisters::lineHigh(bus)){failed(bus,registers);return true;}
  const uint32_t idleAge=uiIdleAgeMs(millis());
  if(!SleepTimer::arm(bus,idleAge)||!inputSafe(millis())||!WakeRegisters::lineHigh(bus)){
    failed(bus,registers);return true;
  }
  Serial.printf("[deep-sleep] full-off in %us from idle age %ums\n",
    unsigned(SleepTimer::remainingSeconds(idleAge)),unsigned(idleAge));
  const bool sent=M5.Power.M5pm1.powerOff();delay(200);
  // A successful physical cut never reaches rollback; no unwakeable ESP sleep.
  Serial.printf("[deep-sleep] power-off returned sent %d; recovering online\n",sent);failed(bus,registers);return true;
}
