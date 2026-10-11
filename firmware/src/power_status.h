#pragma once
#include <stdint.h>

void powerTick(uint32_t ms);
int powerBatteryLevel(uint32_t ms);
bool powerCharging();
int powerInputSource(uint32_t ms); // fresh PM1 input: 0 battery, 1 external, -1 unknown
bool powerAutoOffAllowed(uint32_t ms); // fresh, successful PM1 reads prove battery-only operation
bool powerConfirmAutoOff();           // re-read input immediately before physical power-off
void powerReport(uint32_t ms);
