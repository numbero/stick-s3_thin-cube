#include "power_save.h"
#include "power_save_policy.h"
#include "ble_link.h"
#include <Arduino.h>
namespace { PowerWorkMode mode=PowerWorkMode::Performance; }
void powerSaveTick(bool idle,bool home){
  const auto next=powerWorkMode(idle,home);
  if(mode!=next){
    const unsigned target=powerWorkMHz(next);
    if(getCpuFrequencyMhz()==target||setCpuFrequencyMhz(target)){
      mode=next;
      Serial.printf("[power-save] online %s cpu %u MHz\n",idle?"idle":home?"home":"active",getCpuFrequencyMhz());
    }
  }
  // A visible home screen keeps the fast radio/input/render profile, including
  // continuous local sound. Only settled standby requests a slower BLE link.
  bleSetIdle(powerSaveActive());
}
void powerSavePerformance(){powerSaveTick(false);}
bool powerSaveActive(){return mode==PowerWorkMode::Standby;}
bool powerSaveReduced(){return mode!=PowerWorkMode::Performance;}
