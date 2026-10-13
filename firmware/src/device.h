#pragma once
#include <stdint.h>
#include <esp_http_server.h>
#include "pose_detector.h"
#include "ui_status.h"

void deviceBegin();
void deviceRegister(httpd_handle_t server);
void deviceTick(uint32_t ms, bool sleeping, bool busy);
bool deviceAuthorized(httpd_req_t* req);
void deviceStop();
bool deviceStreaming();
bool deviceHeld();
bool deviceMuted();
bool deviceLocalSound();
bool deviceInverted();
uint8_t deviceDisplayRotation();
bool deviceHealthy();
bool deviceControlConnected(uint32_t ms);
UIStatus::VoiceInputs deviceVoiceStatus(uint32_t ms);
const char* deviceVoiceBlock();
float deviceLevel();
uint8_t deviceBrightness();
const char* deviceName();
const char* devicePairPin();
bool devicePairWindow();
void deviceOpenPairing();
void deviceToggleMute();
void deviceMotion(float x, float y, float z, float gyro, uint32_t ms);
DevicePose devicePose();
bool deviceTakeNetwork(char* ssid, char* pass);
bool deviceDeepSleepEnabled();
bool deviceSaveForSleep();
void deviceSleepChanged();
