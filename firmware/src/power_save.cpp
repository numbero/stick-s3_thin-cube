#include "power_save.h"
#include "power_save_policy.h"
#include "ble_link.h"
#include <Arduino.h>
namespace { bool idleMode=false; }
void powerSaveTick(bool idle){
  if(idleMode!=idle){
    const unsigned target=idle?IdleWorkProfile::IdleMHz:IdleWorkProfile::FastMHz;
    if(setCpuFrequencyMhz(target)){
      idleMode=idle;
      Serial.printf("[power-save] online %s cpu %u MHz\n",idle?"idle":"active",getCpuFrequencyMhz());
    }
  }
  bleSetIdle(idleMode);
}
void powerSavePerformance(){powerSaveTick(false);}
bool powerSaveActive(){return idleMode;}
