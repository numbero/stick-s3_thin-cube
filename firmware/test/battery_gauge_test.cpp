#include "../src/battery_gauge.h"
#include <assert.h>
#include <stdio.h>

static void seed(BatteryGauge& gauge, int mv, uint32_t start = 0){
  for(unsigned i = 0; i < 7; ++i)gauge.sample(start + i * 1000, mv);
}

int main(){
  {
    BatteryGauge g;
    assert(g.level(0) == -1 && g.otaLevel(0) == -1);
    for(unsigned i = 0; i < 4; ++i){g.sample(i * 1000, 3980);assert(g.level(i * 1000) == -1);}
    g.sample(4000, 3980);assert(g.level(4000) == 85);
    assert(BatteryGauge::voltageLevel(3300) == 0);
    assert(BatteryGauge::voltageLevel(4100) == 100);
    assert(BatteryGauge::voltageLevel(2500) == 0 && BatteryGauge::voltageLevel(4500) == 100);
  }
  {
    // The user's 80/82/85/90% trace, replayed at the old 3-second cadence.
    BatteryGauge g;seed(g, 3972);
    const int volts[] = {3940, 3956, 3980, 4020};
    int lo = 100, hi = 0;
    for(unsigned sec = 7; sec < 307; ++sec){
      g.sample(sec * 1000, volts[((sec - 7) / 3) % 4]);
      lo = std::min(lo, g.level(sec * 1000));hi = std::max(hi, g.level(sec * 1000));
    }
    printf("80/82/85/90 replay: displayed %d..%d%%\n", lo, hi);
    assert(hi - lo <= 3);
  }
  {
    BatteryGauge g;seed(g, 3980);
    // Wi-Fi/speaker load spike and rebound, surrounded by stable readings.
    for(unsigned sec = 7; sec < 60; ++sec){
      g.sample(sec * 1000, sec == 10 ? 3500 : sec == 25 ? 4400 : 3980);
      assert(g.level(sec * 1000) == 85);
    }
  }
  {
    // Sustained changes must converge, not be permanently rejected as noise.
    BatteryGauge g;seed(g, 3940);int prev = 80;uint32_t changedAt = 4000;
    for(unsigned sec = 7; sec < 160; ++sec){
      const uint32_t now = sec * 1000;g.sample(now, 4020);
      const int shown = g.level(now);
      if(shown != prev){assert(shown == prev + 1 && now - changedAt >= 5000);changedAt = now;}
      prev = shown;
    }
    assert(prev >= 89 && prev <= 90);
    for(unsigned sec = 160; sec < 320; ++sec){
      const uint32_t now = sec * 1000;g.sample(now, 3940);
      const int shown = g.level(now);
      if(shown != prev){assert(shown == prev - 1 && now - changedAt >= 5000);changedAt = now;}
      prev = shown;
    }
    assert(prev >= 80 && prev <= 81);
  }
  {
    BatteryGauge g;seed(g, 4020);
    g.sample(7000, 0);assert(g.level(7000) == 90 && g.otaLevel(7000) == -1);
    g.sample(8000, 5000);assert(g.otaLevel(8000) == -1);
    assert(g.level(11000) == -1);
    // Reconnecting must not revive an old high percentage without new samples.
    for(unsigned sec = 12; sec < 16; ++sec){g.sample(sec * 1000, 3700);assert(g.level(sec * 1000) == -1);}
    g.sample(16000, 3700);assert(g.level(16000) == 50);
  }
  {
    BatteryGauge g;seed(g, 3980);
    g.sample(7000, 3650);
    assert(g.level(7000) == 85 && g.otaLevel(7000) < 50);
    BatteryGauge low;seed(low, 3650);low.sample(7000, 4050);
    assert(low.otaLevel(7000) < 50); // Upward spike cannot authorize OTA either.
  }
  {
    BatteryGauge g;seed(g, 4020);
    for(unsigned sec = 7; sec < 90; ++sec)g.sample(sec * 1000, 3332);
    assert(g.level(89000) <= 10 && g.otaLevel(89000) < 10);
  }
  {
    const uint32_t start = 0xfffff000u;
    BatteryGauge g;seed(g, 3980, start);
    assert(g.level(start + 6000) == 85);
    for(unsigned sec = 7; sec < 90; ++sec)g.sample(start + sec * 1000, 4020);
    assert(g.level(start + 89000) >= 89);
    assert(g.level(start + 94000) == -1);
  }
  {
    BatteryGauge full;seed(full, 4092);
    for(unsigned sec = 7; sec < 70; ++sec)full.sample(sec * 1000, 4200);
    assert(full.level(69000) == 100);
    BatteryGauge empty;seed(empty, 3308);
    for(unsigned sec = 7; sec < 70; ++sec)empty.sample(sec * 1000, 3250);
    assert(empty.level(69000) == 0);
  }
  puts("9 battery noise, outlier, trend, freshness, OTA, low-charge and wrap scenarios passed");
}
