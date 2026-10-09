#pragma once
#include <algorithm>
#include <stdint.h>

// Voltage-based estimate, not a coulomb counter. Keep the pinned M5Unified
// conversion (3300 mV -> 0%, 4100 mV -> 100%) until this battery is calibrated.
// Filter millivolts before converting, so integer percentages do not amplify
// ADC noise. No heap allocation or blocking sampling burst.
class BatteryGauge {
 public:
  static constexpr uint32_t SampleMs = 1000, StaleMs = 5000, StepMs = 5000;
  static int voltageLevel(int mv){
    return std::max(0, std::min(100, (mv - 3300) / 8));
  }

  void sample(uint32_t now, int mv){
    rawMv_ = (mv >= 2500 && mv <= 4500) ? mv : -1;
    if(rawMv_ < 0)return;  // A failed I2C read must not become 0%.
    if(haveValid_ && uint32_t(now - validAt_) >= StaleMs){
      count_ = next_ = 0;filterQ_ = 0;shown_ = medianMv_ = -1;
    }
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

    const int target = voltageLevel(filteredMillivolts());
    if(shown_ < 0){shown_ = target;stepAt_ = now;return;}
    // A sustained near-empty reading must not wait minutes behind the UI ramp.
    if(target <= 10 && shown_ > 10){shown_ = target;stepAt_ = now;return;}
    if(uint32_t(now - stepAt_) < StepMs)return;
    const int delta = target - shown_;
    // 2-point hysteresis prevents 84/85 rounding chatter. Still reach 0/100.
    if(delta >= 2 || (target == 100 && delta > 0)){
      ++shown_;stepAt_ = now;
    } else if(delta <= -2 || (target == 0 && delta < 0)){
      --shown_;stepAt_ = now;
    }
  }

  int level(uint32_t now) const {return fresh(now) ? shown_ : -1;}
  int otaLevel(uint32_t now) const {
    if(level(now) < 0 || rawMv_ < 0)return -1;
    // Never let a slowly falling display or a single upward spike authorize OTA.
    return std::min(voltageLevel(rawMv_),
                    std::min(voltageLevel(medianMv_), voltageLevel(filteredMillivolts())));
  }
  int rawMillivolts() const {return rawMv_;}
  int medianMillivolts() const {return medianMv_;}
  int filteredMillivolts() const {return filterQ_ ? (filterQ_ + 8) / 16 : -1;}

 private:
  bool fresh(uint32_t now) const {return haveValid_ && uint32_t(now - validAt_) < StaleMs;}
  static constexpr unsigned Window = 7, Warmup = 5;
  int samples_[Window]{};
  unsigned count_ = 0, next_ = 0;
  int rawMv_ = -1, medianMv_ = -1, filterQ_ = 0, shown_ = -1;
  bool haveValid_ = false;
  uint32_t validAt_ = 0, stepAt_ = 0;
};
