#pragma once
#include <stdint.h>
void deepSleepBegin();
bool deepSleepAvailable();
bool deepSleepPending();
uint32_t deepSleepSequence();
bool deepSleepAcknowledge(uint32_t sequence);
void deepSleepCancel();
bool deepSleepTick(uint32_t now,bool safe,bool connected);
bool deepSleepDisarm();
