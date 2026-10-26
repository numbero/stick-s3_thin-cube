#pragma once
#include <stdint.h>
void powerSaveTick(bool idle,bool home=false);
void powerSavePerformance(); // main loop, before starting audio/UI work
bool powerSaveActive(); // online standby only: slow BLE/input/render profile
bool powerSaveReduced(); // CPU/JSON saving, including the responsive home screen
