#pragma once
#include <stdint.h>
#include <stddef.h>

void powerTick(uint32_t ms);
int powerBatteryLevel(uint32_t ms);
bool powerCharging();
bool powerBatteryLow(uint32_t ms);
uint32_t powerSampleAt();
bool powerFormatJSON(char* out,size_t capacity,uint32_t ms);
void powerReportConfig();
int powerInputSource(uint32_t ms); // fresh PM1 input: 0 battery, 1 external, -1 unknown
bool powerAutoOffAllowed(uint32_t ms); // fresh, successful PM1 reads prove battery-only operation
bool powerConfirmAutoOff();           // re-read input immediately before physical power-off
void powerReport(uint32_t ms);
bool powerCalibrationArm(); // opt-in, RAM only; requires authenticated BLE + USB
void powerCalibrationCancel(int reason=1);
bool powerCalibrationOwnsUI();
bool powerCalibrationRunning();
int powerCalibrationBrightness(uint32_t ms);
const char* powerCalibrationLabel();
bool powerCalibrationShutdownDue(uint32_t ms);
void powerCalibrationShutdownAttempted();
