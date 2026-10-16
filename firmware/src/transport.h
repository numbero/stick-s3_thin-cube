#pragma once
#include "transport_policy.h"
#include <mutex>
void transportBegin();
void transportStarted();
void transportTick(uint32_t now);
TransportMode transportMode();
const TransportRecord& transportRecord();
bool transportBusy();
bool transportBootVerified();
const char* requestTransportSwitch(TransportMode target,const char* source,const char* requestId);
bool transportCommit(const char* requestId);

void transportProvisioningOverride();
bool transportHasChoice();
bool transportSetMode(TransportMode mode);
// Caller holds transportAdmission(). Also supports restarting the current mode.
bool transportScheduleRestart(TransportMode mode,const char* source,const char* requestId);

std::mutex& transportAdmission();
