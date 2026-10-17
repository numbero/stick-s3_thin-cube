#pragma once
#include "battery_curve_reference.h"
#include "battery_curve_generated.h"

struct BatteryCurveSelection {
  const BatteryCurve* curve;
  const char* model;
  const BatteryCurve* otaCurve;
};

// A null display curve selects the pinned M5Unified linear conversion. Keep
// the previous reference curve as an independent OTA cap: a higher displayed
// percentage must not loosen the existing firmware-update admission policy.
inline BatteryCurveSelection selectBatteryCurve(uint64_t /*chip*/, bool /*batteryOnly*/,
                                                bool /*quietBLE*/, unsigned /*brightness*/){
  return {nullptr,"m5unified-linear",&ReferenceBattery::Curve};
}
