#pragma once
#include <stdint.h>
#include <esp_http_server.h>

// Called before transportBegin: consume the one-shot NVS intent and snapshot
// unfinished transport transactions before their ordinary boot recovery.
void maintenancePrepareBoot();
bool maintenanceCaptureBoot(bool aInitially,bool bInitially);
void maintenanceBegin(bool physicalRequested,bool blockedByBootTransaction=false);
bool maintenanceActive();
bool maintenanceReady();
bool maintenancePending();
// May run in a control worker. Only admission/NVS/atomic publication here;
// main-loop maintenanceTick owns radio, peripheral and restart operations.
const char* maintenanceRequest(const char* requestId);
void maintenanceTick(uint32_t now);
void maintenanceTouch();
void maintenanceExit();
bool maintenanceAuthorized(httpd_req_t* request);
const char* maintenanceApSsid();
const char* maintenanceCode();
const char* maintenanceLastErr();
int maintenanceIdleLeftSec(uint32_t now);
