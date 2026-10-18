#pragma once
#include <stdint.h>
#include <stddef.h>

struct ImuVector {
  float x,y,z;
  ImuVector(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
};
struct ImuSample {
  ImuVector accel,gyro;
  uint32_t sequence=0,accelAt=0,gyroAt=0;
  bool hasAccel=false,hasGyro=false;
};
// Main-loop owner of initialization, status consumption, samples and tap pages.
bool imuBegin();
void imuPoll(uint32_t now);
bool imuEnabled();
bool imuHealthy(uint32_t now);
ImuSample imuSample();
bool imuLegacy();
bool imuRead(uint8_t reg,uint8_t* data,size_t length);
bool imuWrite(uint8_t reg,const uint8_t* data,size_t length);
bool imuSuspendForSleep();
void imuResumeAfterSleep();
bool imuDiagnosticCommand(const char* command);
void imuReport();
struct TapSnapshot {
  bool supported,enabled;uint32_t raw,accepted,blocked;
  TapSnapshot(bool s=false,bool e=false,uint32_t r=0,uint32_t a=0,uint32_t b=0):supported(s),enabled(e),raw(r),accepted(a),blocked(b){}
};
TapSnapshot imuTapSnapshot();
