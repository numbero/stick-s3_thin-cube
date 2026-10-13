#pragma once
#include <stdint.h>
void powerSaveTick(bool idle);
void powerSavePerformance(); // main loop, before starting audio/UI work
bool powerSaveActive();
