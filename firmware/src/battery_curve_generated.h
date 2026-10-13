#pragma once
#include "battery_curve.h"

// Optional per-device override. The user cancelled physical calibration;
// no partial charge log is used to create or enable a measured curve.
// Normal operation uses battery_curve_reference.h.
namespace CalibratedBattery {
static constexpr uint64_t Device = 0;
static constexpr bool Enabled = false;
static constexpr const char* Model = "untrained";
static const BatteryCurve Dim{nullptr,0}, Awake{nullptr,0};
}
