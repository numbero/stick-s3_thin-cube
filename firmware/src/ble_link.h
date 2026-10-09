#pragma once
#include <stdint.h>
#include <stddef.h>
bool bleBegin(const char* name,uint32_t passkey);
void bleTick(uint32_t now);
bool bleTake(char* out,size_t capacity);
bool bleSendControl(const char* text);
void bleSendStatus(const char* text);
void bleSendSnapshot(const char* text);
uint16_t bleMTU();
uint32_t bleAudioDropped();
bool bleSendAudio(const uint8_t* data,size_t length);
bool bleConnected();
bool bleConnecting(); // Physical link exists; link security is not complete yet.
bool bleHealthy();
bool bleAudioReady();
uint32_t bleGeneration();
bool blePairVisible();
void bleForgetBonds(uint32_t passkey);
void bleClearAudio();
