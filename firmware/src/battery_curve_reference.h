/*
 * Copyright 2024 Embeint Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * LiPo OCV points from Zephyr's BATTERY_OCV_CURVE_LITHIUM_ION_POLYMER_DEFAULT:
 * https://github.com/zephyrproject-rtos/zephyr/blob/dc4e2c2f780ada5cfb0f3862395778842fbe9e63/include/zephyr/dt-bindings/battery/battery.h
 * Upstream cites Analog Devices Application Note 4189, Table 1.
 * Converted from microvolts to millivolts, rounded to nearest integer.
 * This is a generic cell reference, not a measured StickS3 battery curve.
 * Running/charging terminal voltage is not OCV; no unmeasured current,
 * temperature or load compensation is applied.
 */
#pragma once
#include "battery_curve.h"

namespace ReferenceBattery {
static constexpr const char* Model = "lipo-ref-an4189";
static constexpr BatteryCurvePoint Points[] = {
  {3306,0}, {3687,10}, {3741,20}, {3775,30}, {3793,40}, {3821,50},
  {3884,60}, {3945,70}, {4008,80}, {4086,90}, {4177,100}
};
static const BatteryCurve Curve{Points,sizeof(Points)/sizeof(Points[0])};
}
