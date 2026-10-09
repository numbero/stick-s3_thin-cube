#pragma once
#include <stdint.h>
void audioBegin(bool ble=false);
void audioTick(bool capture);
bool audioReady();
bool audioDelivered();
uint32_t audioFrameCount(); // completed 10 ms frames, main-loop diagnostic
uint32_t audioDropCount();  // capture queue overflow, diagnostic only
float audioLevel();
bool audioTake(int16_t* out); // 160 (BLE) or 480 (Wi-Fi) completed mono samples; never an in-flight buffer
