#pragma once
#include "battery_curve_reference.h"
#include "battery_curve_generated.h"

struct BatteryCurveSelection {
  const BatteryCurve* curve;
  const char* model;
};

// The default table stays active across brightness, transport and audio changes.
// Only a future, explicitly enabled per-device model can override it in the
// exact operating conditions used to characterize that model.
inline BatteryCurveSelection selectBatteryCurve(uint64_t chip, bool batteryOnly,
                                                bool quietBLE, unsigned brightness){
  if(CalibratedBattery::Enabled && CalibratedBattery::Device==chip && batteryOnly && quietBLE){
    const BatteryCurve* curve=nullptr;
    if(brightness<=16)curve=&CalibratedBattery::Dim;
    else if(brightness>=128&&brightness<=140)curve=&CalibratedBattery::Awake;
    if(curve && curve->valid())return {curve,CalibratedBattery::Model};
  }
  return {&ReferenceBattery::Curve,ReferenceBattery::Model};
}
