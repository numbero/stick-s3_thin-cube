#pragma once
#include <stdint.h>

void powerTick(uint32_t ms);
int powerBatteryLevel(uint32_t ms);
bool powerCharging();
void powerReport(uint32_t ms);
