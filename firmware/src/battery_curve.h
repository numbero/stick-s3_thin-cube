#pragma once
#include <stddef.h>
#include <stdint.h>

struct BatteryCurvePoint { int mv, percent; };

// Structurally validated voltage table; validation here does not prove accuracy.
// Raw voltage safety gates remain independent of the selected display model.
struct BatteryCurve {
  const BatteryCurvePoint* points;
  size_t count;
  bool valid() const {
    if(!points || count < 2 || count > 24 || points[0].percent != 0 || points[count-1].percent != 100)return false;
    for(size_t i=0;i<count;++i){
      if(points[i].mv < 2500 || points[i].mv > 4500 || points[i].percent < 0 || points[i].percent > 100)return false;
      if(i && (points[i].mv <= points[i-1].mv || points[i].percent < points[i-1].percent))return false;
    }
    return true;
  }
  int level(int mv) const {
    if(!valid())return -1;
    if(mv <= points[0].mv)return 0;
    if(mv >= points[count-1].mv)return 100;
    for(size_t i=1;i<count;++i)if(mv <= points[i].mv){
      const auto& a=points[i-1];const auto& b=points[i];
      const int span=b.mv-a.mv;
      return a.percent+((mv-a.mv)*(b.percent-a.percent)+span/2)/span;
    }
    return -1;
  }
};
