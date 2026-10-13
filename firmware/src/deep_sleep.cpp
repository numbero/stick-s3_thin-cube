#include "deep_sleep.h"
#include "motion_wake_registers.h"
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
#include "wifi_prov.h"
#include <M5Unified.h>
#include <esp_system.h>
#include <atomic>

namespace {
struct Bus {
  m5::IMU_Base* imu=M5.Imu.getImuInstancePtr(0);
  bool bmiRead(uint8_t r,uint8_t* v,size_t n){return imu&&imu->readRegister(r,v,n);}
  bool bmiWrite(uint8_t r,const uint8_t* v,size_t n){return imu&&imu->writeRegister(r,const_cast<uint8_t*>(v),n);}
  bool pmRead(uint8_t r,uint8_t* v,size_t n){return M5.Power.M5pm1.readRegister(r,v,n);}
  bool pmWrite(uint8_t r,uint8_t v){return M5.Power.M5pm1.writeRegister(r,&v,1);}
  void pause(unsigned ms){delay(ms);}
};
DeepSleepPolicy policy;
std::atomic<bool> pending{false};
bool available=false;
bool inputSafe(uint32_t now){
  return deviceDeepSleepEnabled()&&!otaActive()&&!provActive()&&!transportBusy()&&
    !devicePairWindow()&&!deviceStreaming()&&!soundPlaying()&&powerBatteryLevel(now)>=0&&
    !powerBatteryLow(now)&&powerConfirmAutoOff();
}
void failed(Bus& bus,MotionWakeRegisters<Bus>& registers){
  const bool restored=registers.restore(bus);available=available&&restored;
  if(!restored){
    const bool normal=M5.Imu.begin(nullptr,M5.getBoard());
    Serial.printf("[deep-sleep] normal IMU reinitialize %d\n",normal);
  }
  Serial.printf("[deep-sleep] cancelled restore %d available %d\n",restored,available);
  M5.Display.wakeup();deepSleepCancel();deviceSleepChanged();
}
}
void deepSleepBegin(){
  Bus bus;uint8_t wake=0;const bool read=bus.pmRead(0x05,&wake,1);
  bool clean=false;
  for(unsigned n=0;n<3&&!clean;++n)clean=MotionWakeRegisters<Bus>::disarm(bus);
  bool cleared=M5.Power.M5pm1.clearWakeSource(0x7f);
  cleared=M5.Power.M5pm1.clearGPIOIRQStatus()&&cleared;
  cleared=M5.Power.M5pm1.clearSystemIRQStatus()&&cleared;
  cleared=M5.Power.M5pm1.clearButtonIRQStatus()&&cleared;
  available=clean&&cleared&&M5.Imu.getType()==m5::imu_t::imu_bmi270;
  Serial.printf("[deep-sleep] wake_src %u read %d cleaned %d available %d\n",unsigned(wake),read,clean,available);
}
bool deepSleepAvailable(){return available;}
bool deepSleepPending(){return pending.load();}
uint32_t deepSleepSequence(){return policy.id;}
bool deepSleepAcknowledge(uint32_t sequence){return policy.ack(sequence);}
void deepSleepCancel(){
  const bool wasPending=policy.pending;policy.cancel();pending=false;
  if(wasPending)uiPowerActivity();
}
bool deepSleepDisarm(){Bus bus;const bool ok=MotionWakeRegisters<Bus>::disarm(bus);if(!ok)available=false;return ok;}
bool deepSleepTick(uint32_t now,bool safe,bool connected){
  safe=safe&&available&&deviceDeepSleepEnabled()&&powerAutoOffAllowed(now)&&
    powerBatteryLevel(now)>=0&&!powerBatteryLow(now);
  if(!policy.pending){
    if(!safe||uiIdleAgeMs(now)<IdlePowerPolicy::SleepMs)return false;
    policy.begin(now,esp_random(),connected);pending=true;
    powerSavePerformance();deviceStop();deviceSleepChanged();
    Serial.printf("[deep-sleep] announcing id %u ack_required %d\n",unsigned(policy.id),connected);return true;
  }
  powerSavePerformance();const auto decision=policy.update(now,safe,connected);
  if(decision==DeepSleepPolicy::Decision::Cancel){pending=false;uiPowerActivity();deviceSleepChanged();return true;}
  if(decision!=DeepSleepPolicy::Decision::Commit)return true;
  std::unique_lock<std::mutex> admission(transportAdmission(),std::try_to_lock);
  if(!admission.owns_lock())return true;
  if(!inputSafe(millis())){deepSleepCancel();deviceSleepChanged();return true;}
  deviceStop();soundCancel();audioTick(false);M5.Speaker.end();
  if(!audioPowerDown()||!deviceSaveForSleep()){deepSleepCancel();deviceSleepChanged();return true;}
  Bus bus;MotionWakeRegisters<Bus> registers;
  if(!registers.prepare(bus)){failed(bus,registers);return true;}
  M5.Display.setBrightness(0);M5.Display.sleep();
  Serial.printf("[deep-sleep] entering PM1 L1 id %u\n",unsigned(policy.id));
  if(!inputSafe(millis())||!MotionWakeRegisters<Bus>::lineHigh(bus)){failed(bus,registers);return true;}
  const bool sent=M5.Power.M5pm1.powerOff();delay(200);
  // A successful physical cut never reaches rollback; no unwakeable ESP sleep.
  Serial.printf("[deep-sleep] power-off returned sent %d; recovering online\n",sent);failed(bus,registers);return true;
}
