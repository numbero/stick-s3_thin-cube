#pragma once
#include <stdint.h>
void audioBegin(bool ble=false);
bool audioInitialized(); // configured microphone and allocated local capture queue
void audioTick(bool capture);
bool audioPowerDown(); // main loop only, after capture and sound playback stop
bool audioReady();
bool audioDelivered();
uint32_t audioFrameCount(); // completed 10 ms frames, main-loop diagnostic
uint32_t audioDropCount();  // capture queue overflow, diagnostic only
float audioLevel();
float audioRms(); // Last completed frame before user gain; current local sound amplitude.
bool audioTake(int16_t* out); // 160 (BLE) or 480 (Wi-Fi) completed mono samples; never an in-flight buffer
