#pragma once
bool deepSleepDiagnostic(); // USB-only prepare/rollback check; never powers off
#include <stdint.h>
void deepSleepBoot(); // immediately after M5.begin, before UI / radio startup
void deepSleepBegin();
bool deepSleepAvailable();
bool deepSleepPending();
uint32_t deepSleepSequence();
bool deepSleepAcknowledge(uint32_t sequence);
void deepSleepCancel();
bool deepSleepTick(uint32_t now,bool safe,bool connected);
bool deepSleepDisarm();
