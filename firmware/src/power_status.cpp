#include "power_status.h"
#include "battery_gauge.h"
#include "ota.h"
#include "idle_power.h"
#include <M5Unified.h>

namespace {
BatteryGauge gauge;
bool sampled = false, charging = false, chargeCandidate = false;
uint32_t sampleAt = 0, chargeSince = 0;
int vbusMv = -1,inputSource=-1;
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
  sampled = true;sampleAt = ms;
  uint16_t mv = 0;
  gauge.sample(ms, M5.Power.M5pm1.getBatteryVoltage(&mv) ? int(mv) : -1);
  readInputPower();
  // StickS3 PM1 GPIO0 is active-low CHG_STAT. Unlike isCharging(), this API
  // reports I2C failure, so a read failure cannot masquerade as charging.
  uint8_t gpio = 0;
  if(M5.Power.M5pm1.getGPIOInputBits(&gpio)){
    const bool raw = (gpio & 1) == 0;
    if(raw != chargeCandidate){chargeCandidate = raw;chargeSince = ms;}
    else if(uint32_t(ms - chargeSince) >= 2000)charging = raw;
  } else {
    chargeSince = ms;
  }
  otaSetBattery(gauge.otaLevel(ms));
}

int powerBatteryLevel(uint32_t ms){return gauge.level(ms);}
bool powerCharging(){return charging;}
int powerInputSource(uint32_t ms){return sampled&&uint32_t(ms-sampleAt)<=2000?inputSource:-1;}
bool powerAutoOffAllowed(uint32_t ms){return sampled&&uint32_t(ms-sampleAt)<=2000&&batteryOnly;}
bool powerConfirmAutoOff(){return readInputPower();}
void powerReport(uint32_t ms){
  const int raw = gauge.rawMillivolts();
  Serial.printf("[power] raw_mv %d median_mv %d filtered_mv %d raw_pct %d shown_pct %d ota_pct %d vbus_mv %d chg %d auto_off %d\n",
                raw, gauge.medianMillivolts(), gauge.filteredMillivolts(),
                raw < 0 ? -1 : BatteryGauge::voltageLevel(raw), gauge.level(ms),
                gauge.otaLevel(ms), vbusMv, charging,powerAutoOffAllowed(ms));
}
