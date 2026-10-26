#pragma once
#include <stdint.h>
#include <stddef.h>
bool bleBegin(const char* name);
void bleTick(uint32_t now);
bool bleTake(char* out,size_t capacity);
bool bleSendControl(const char* text);
void bleSendStatus(const char* text);
void bleSendSnapshot(const char* text);
uint16_t bleMTU();
uint32_t bleAudioDropped();
bool bleSendAudio(const uint8_t* data,size_t length);
bool bleConnected();
bool bleConnecting(); // Waiting for automatic link encryption, without a passkey.
bool bleHealthy();
bool bleAudioReady();
uint32_t bleGeneration();
void bleDisconnect();
void bleClearAudio();
void bleSetIdle(bool idle);
void bleAdvertisingActivity(uint32_t now); // main loop: real button/handling only
bool blePerformanceReady();
uint16_t bleConnectionInterval(); // negotiated units of 1.25 ms; 0 unknown
