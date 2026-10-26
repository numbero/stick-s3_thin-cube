#include "power_status.h"
#include "battery_gauge.h"
#include "ota.h"
#include "idle_power.h"
#include "battery_calibration.h"
#include "battery_curve_config.h"
#include "device.h"
#include "transport.h"
#include "ui.h"
#include "sound_effects.h"
#include "power_save.h"
#include "ble_link.h"
#include <M5Unified.h>
#include <esp_system.h>
#include <atomic>
#include <mutex>

namespace {
BatteryGauge gauge;
BatteryCalibration calibration;
// Physical calibration was explicitly cancelled. Keep its tools dormant;
// authenticated arm requests cannot start a discharge in this release.
constexpr bool CalibrationAllowed=false;
std::atomic<bool> calibrationOwns{false};
const char* curveModel="linear";
bool sampled = false, charging = false, chargeCandidate = false;
uint32_t sampleAt = 0, chargeSince = 0, chargeAt = 0, failures = 0, bootId = 0;
bool chargeRead = false, configRead = false;
int vbusMv = -1,inputSource=-1, cutoffMv=-1;
bool batteryOnly=false;
bool readInputPower(){
  uint8_t vin[2]={},source=0;
  // PM1 PWR_SRC (0x04), VIN_L/H (0x24/0x25). Convenience getters return
  // zero on I2C failure and cannot safely decide whether USB was unplugged.
  const bool sourceOK=M5.Power.M5pm1.readRegister(0x04,&source,1);
  const bool vinOK=M5.Power.M5pm1.readRegister(0x24,vin,sizeof(vin));
  vbusMv=vinOK?int(vin[0]|(uint16_t(vin[1])<<8)):-1;
  batteryOnly=idleBatteryOnly(sourceOK&&vinOK,source,vinOK?uint16_t(vbusMv):0);
  inputSource=idlePowerSource(sourceOK&&vinOK,source,vinOK?uint16_t(vbusMv):0);
  return batteryOnly;
}
}

void powerTick(uint32_t ms){
  if(sampled && uint32_t(ms - sampleAt) < BatteryGauge::SampleMs)return;
  if(!sampled)bootId=esp_random();
  sampled = true;sampleAt = ms;
  if(!configRead){powerReportConfig();configRead=cutoffMv>=0;}
  uint16_t mv = 0;
  readInputPower();
  const bool voltageOK=M5.Power.M5pm1.getBatteryVoltage(&mv);
  if(!voltageOK)++failures;
  const auto selected=selectBatteryCurve(ESP.getEfuseMac(),inputSource==0,
    transportMode()==TransportMode::BLE&&!deviceStreaming()&&!soundPlaying(),uiActualBrightness());
  gauge.setCurve(selected.curve);
  gauge.setOtaCurve(selected.otaCurve);
  curveModel=selected.model;
  gauge.sample(ms, voltageOK ? int(mv) : -1,inputSource);
  // StickS3 PM1 GPIO0 is active-low CHG_STAT. Unlike isCharging(), this API
  // reports I2C failure, so a read failure cannot masquerade as charging.
  uint8_t gpio = 0;
  if(M5.Power.M5pm1.getGPIOInputBits(&gpio)){
    chargeRead=true;chargeAt=ms;
    const bool raw = (gpio & 1) == 0;
    if(raw != chargeCandidate){chargeCandidate = raw;chargeSince = ms;}
    else if(uint32_t(ms - chargeSince) >= 2000)charging = raw;
  } else {
    chargeSince = ms;
  }
  calibration.update(ms,voltageOK?int(mv):-1,gauge.medianMillivolts(),inputSource,
    chargeRead&&uint32_t(ms-chargeAt)<BatteryGauge::StaleMs,charging,deviceControlConnected(ms));
  calibrationOwns=calibration.ownsUI();
  otaSetPower(gauge.otaLevel(ms),inputSource);
}

int powerBatteryLevel(uint32_t ms){return gauge.level(ms);}
bool powerCharging(){return chargeRead&&uint32_t(millis()-chargeAt)<BatteryGauge::StaleMs&&charging;}
bool powerBatteryLow(uint32_t ms){return gauge.low(ms);}
uint32_t powerSampleAt(){return sampleAt;}
bool powerFormatJSON(char* out,size_t capacity,uint32_t ms){
  const bool chargeKnown=chargeRead&&uint32_t(ms-chargeAt)<BatteryGauge::StaleMs;
  const int size=snprintf(out,capacity,
    "{\"boot\":%u,\"ms\":%u,\"rawMv\":%d,\"medianMv\":%d,\"filteredMv\":%d,\"percent\":%d,\"otaPercent\":%d,\"source\":%d,\"vinMv\":%d,\"charging\":%s,\"chargeKnown\":%s,\"low\":%s,\"cutoffMv\":%d,\"failures\":%u,\"chip\":\"%012llx\",\"model\":\"%s\",\"actualBrightness\":%u,\"idleMs\":%u,\"cpuMHz\":%u,\"bleInterval\":%u,\"onlineSaving\":%s,\"cal\":{\"run\":%u,\"state\":%d,\"elapsed\":%u,\"phase\":%d,\"phaseMs\":%u,\"stableMs\":%u,\"reason\":%d}}",
    unsigned(bootId),unsigned(sampleAt),gauge.rawMillivolts(),gauge.medianMillivolts(),gauge.filteredMillivolts(),
    gauge.level(ms),gauge.otaLevel(ms),powerInputSource(ms),vbusMv,
    chargeKnown&&charging?"true":"false",chargeKnown?"true":"false",gauge.low(ms)?"true":"false",cutoffMv,unsigned(failures),
    (unsigned long long)ESP.getEfuseMac(),curveModel,unsigned(uiActualBrightness()),unsigned(uiIdleAgeMs(ms)),
    unsigned(getCpuFrequencyMhz()),unsigned(bleConnectionInterval()),powerSaveReduced()?"true":"false",
    unsigned(calibration.id()),int(calibration.state()),unsigned(calibration.elapsed(ms)),calibration.phase(ms),
    unsigned(calibration.phaseAge(ms)),unsigned(calibration.chargeStable(ms)),int(calibration.reason()));
  return size>=0&&size_t(size)<capacity;
}
void powerReportConfig(){
  uint8_t regs[10]={};
  const bool ok=M5.Power.M5pm1.readRegister(0,regs,sizeof(regs));
  cutoffMv=ok?2000+(int(regs[8])*781+50)/100:-1;
  Serial.printf("[power-config] read_ok %d id %u model %u hw %u sw %u source_bits %u wake_bits %u pwr_cfg %u hold_cfg %u lvp_reg %u cutoff_mv %d i2c_cfg %u reset_reason %d\n",
    ok,regs[0],regs[1],regs[2],regs[3],regs[4],regs[5],regs[6],regs[7],regs[8],cutoffMv,regs[9],int(esp_reset_reason()));
}
int powerInputSource(uint32_t ms){return sampled&&uint32_t(ms-sampleAt)<=2000?inputSource:-1;}
bool powerAutoOffAllowed(uint32_t ms){return sampled&&uint32_t(ms-sampleAt)<=2000&&batteryOnly;}
bool powerConfirmAutoOff(){return readInputPower();}
void powerReport(uint32_t ms){
  const int raw = gauge.rawMillivolts();
  Serial.printf("[power] raw_mv %d median_mv %d filtered_mv %d raw_pct %d shown_pct %d ota_pct %d vbus_mv %d chg %d auto_off %d source %d low %d sample_ms %u failures %u\n",
                raw, gauge.medianMillivolts(), gauge.filteredMillivolts(),
                raw < 0 ? -1 : BatteryGauge::voltageLevel(raw), gauge.level(ms),
                gauge.otaLevel(ms), vbusMv, powerCharging(),powerAutoOffAllowed(ms),powerInputSource(ms),gauge.low(ms),unsigned(sampleAt),unsigned(failures));
}

bool powerCalibrationArm(){
  if(!CalibrationAllowed)return false;
  std::lock_guard<std::mutex> admission(transportAdmission());
  const uint32_t now=millis();
  if(calibration.active()||otaActive()||transportBusy()||devicePairWindow()||now<60000||
     !deviceHealthy()||!deviceControlConnected(now)||transportMode()!=TransportMode::BLE||powerInputSource(now)!=1)return false;
  deviceStop();soundCancel();calibration.arm(now,esp_random());calibrationOwns=true;
  Serial.printf("[battery-cal] armed run %u end_mv %d\n",unsigned(calibration.id()),BatteryCalibration::EndMv);
  return true;
}
void powerCalibrationCancel(int reason){
  calibration.cancel(BatteryCalibration::Reason(reason));calibrationOwns=calibration.ownsUI();
}
bool powerCalibrationOwnsUI(){return calibrationOwns.load();}
bool powerCalibrationRunning(){return calibration.state()==BatteryCalibration::Running;}
int powerCalibrationBrightness(uint32_t ms){return calibration.brightness(ms);}
const char* powerCalibrationLabel(){
  switch(calibration.state()){
    case BatteryCalibration::Charging:return "CAL CHARGE";
    case BatteryCalibration::Ready:return "CAL UNPLUG";
    case BatteryCalibration::Settling:return "CAL SETTLE";
    case BatteryCalibration::Running:return calibration.phase(millis())?"CAL BRIGHT":"CAL DIM";
    case BatteryCalibration::Complete:return "CAL DONE";
    default:return "";
  }
}
bool powerCalibrationShutdownDue(uint32_t ms){return calibration.shutdownDue(ms);}
void powerCalibrationShutdownAttempted(){calibration.shutdownAttempted();}
