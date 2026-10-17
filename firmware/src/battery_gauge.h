#pragma once
#include <algorithm>
#include <stdint.h>
#include "battery_curve.h"

// Voltage-based estimate, not a coulomb counter. A null display curve selects
// the pinned M5Unified conversion. An independent curve may retain a more
// conservative OTA cap without changing the displayed percentage.
// Filter millivolts before converting, so integer percentages do not amplify
// ADC noise. No heap allocation or blocking sampling burst.
class BatteryGauge {
 public:
  static constexpr uint32_t SampleMs = 1000, StaleMs = 5000;
  static int voltageLevel(int mv){
    return std::max(0, std::min(100, (mv - 3300) / 8));
  }
  void setCurve(const BatteryCurve* curve){
    if(curve&&!curve->valid())curve=nullptr;
    if(curve_!=curve){curve_=curve;reset();}
  }
  void setOtaCurve(const BatteryCurve* curve){
    otaCurve_=curve&&curve->valid()?curve:nullptr;
  }
  int estimate(int mv)const{return curve_?curve_->level(mv):voltageLevel(mv);}

  void sample(uint32_t now, int mv, int source = -1){
    source_ = source == 0 || source == 1 ? source : -1;
    // Charging voltage must not seed the next battery-only estimate. Unknown
    // input reads are not an unplug event; compare the last two known sources.
    if(source_ >= 0){
      if(lastSource_ >= 0 && source_ != lastSource_)reset();
      lastSource_ = source_;
    }
    rawMv_ = (mv >= 2500 && mv <= 4500) ? mv : -1;
    if(source_ != 0){low_ = false;lowCount_ = recoveryCount_ = 0;}
    if(rawMv_ < 0){lowCount_ = recoveryCount_ = 0;return;}
    if(haveValid_ && uint32_t(now - validAt_) >= StaleMs)reset();
    haveValid_ = true;validAt_ = now;
    samples_[next_] = mv;next_ = (next_ + 1) % Window;
    if(count_ < Window)++count_;
    if(count_ < Warmup)return;
    int sorted[Window];
    std::copy(samples_, samples_ + count_, sorted);
    std::sort(sorted, sorted + count_);
    medianMv_ = sorted[count_ / 2];
    if(!filterQ_)filterQ_ = medianMv_ * 16;
    else filterQ_ += (medianMv_ * 16 - filterQ_) / 8;

    // Warn on confirmed low voltage independently of the slower display EMA.
    // These are warning thresholds, not changes to the PM1 hardware cutoff.
    if(source_ == 0){
      if(medianMv_ <= 3420 && mv <= 3420){
        recoveryCount_ = 0;if(lowCount_ < 3)++lowCount_;
        if(lowCount_ >= 3)low_ = true;
      }else if(medianMv_ >= 3500 && mv >= 3500){
        lowCount_ = 0;if(recoveryCount_ < 3)++recoveryCount_;
        if(recoveryCount_ >= 3)low_ = false;
      }else lowCount_ = recoveryCount_ = 0;
    }
    const int target = estimate(filteredMillivolts());
    if(shown_ < 0){shown_ = target;return;}
    const int delta = target - shown_;
    // The voltage is already filtered. An additional 1%/5s ramp used to hide
    // low charge for minutes, then jump at 10%. Track the filtered estimate
    // with rounding hysteresis instead, including the entire low-charge tail.
    if(delta >= 2 || delta <= -2 || target == 0 || target == 100)shown_ = target;
  }

  int level(uint32_t now) const {return fresh(now) ? shown_ : -1;}
  bool low(uint32_t now) const {return fresh(now) && source_ == 0 && low_;}
  int otaLevel(uint32_t now) const {
    if(level(now) < 0 || rawMv_ < 0)return -1;
    // Never let a slowly falling display or a single upward spike authorize OTA.
    const int safety=std::min(voltageLevel(rawMv_),
                    std::min(voltageLevel(medianMv_), voltageLevel(filteredMillivolts())));
    return std::min(safety,std::min(otaEstimate(rawMv_),
                    std::min(otaEstimate(medianMv_),otaEstimate(filteredMillivolts()))));
  }
  int rawMillivolts() const {return rawMv_;}
  int medianMillivolts() const {return medianMv_;}
  int filteredMillivolts() const {return filterQ_ ? (filterQ_ + 8) / 16 : -1;}

 private:
  int otaEstimate(int mv) const {
    const int display=estimate(mv);
    return otaCurve_?std::min(display,otaCurve_->level(mv)):display;
  }
  void reset(){
    count_ = next_ = 0;filterQ_ = 0;shown_ = medianMv_ = -1;
    haveValid_ = low_ = false;lowCount_ = recoveryCount_ = 0;
  }
  bool fresh(uint32_t now) const {return haveValid_ && uint32_t(now - validAt_) < StaleMs;}
  static constexpr unsigned Window = 7, Warmup = 5;
  int samples_[Window]{};
  const BatteryCurve* curve_=nullptr;
  const BatteryCurve* otaCurve_=nullptr;
  unsigned count_ = 0, next_ = 0;
  int rawMv_ = -1, medianMv_ = -1, filterQ_ = 0, shown_ = -1;
  bool haveValid_ = false, low_ = false;
  unsigned lowCount_ = 0, recoveryCount_ = 0;
  int source_ = -1, lastSource_ = -1;
  uint32_t validAt_ = 0;
};
