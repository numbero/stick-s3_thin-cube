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
    BatteryGauge g;seed(g, 3940);int prev = 80;
    for(unsigned sec = 7; sec < 160; ++sec){
      const uint32_t now = sec * 1000;g.sample(now, 4020);
      const int shown = g.level(now);
      assert(shown >= prev && shown <= 90);
      prev = shown;
    }
    assert(prev >= 89 && prev <= 90);
    for(unsigned sec = 160; sec < 320; ++sec){
      const uint32_t now = sec * 1000;g.sample(now, 3940);
      const int shown = g.level(now);
      assert(shown <= prev && shown >= 80);
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
  {
    // Regression: the old UI still said 70% 148 seconds after a 37% input.
    // Test tracking, not SOC accuracy: this voltage curve is not calibrated.
    for(int targetMv : {3200, 3500, 3600, 3860, 4100}){
      BatteryGauge g;seed(g, targetMv < 3860 ? 4100 : 3300);
      for(unsigned sec=7;sec<=37;++sec)g.sample(sec*1000,targetMv);
      const int error=g.level(37000)-BatteryGauge::voltageLevel(targetMv);
      assert(error >= -5 && error <= 5);
    }
  }
  {
    // A confirmed low voltage warns before the display has finished tracking.
    BatteryGauge g;
    for(unsigned sec=0;sec<7;++sec)g.sample(sec*1000,3860,0);
    int previous=70;
    for(unsigned sec=7;sec<70;++sec){
      g.sample(sec*1000,3200,0);
      assert(g.level(sec*1000)<=previous);
      assert(previous-g.level(sec*1000)<=11); // No 68 -> 9 shortcut.
      if(sec>=12)assert(g.low(sec*1000));
      previous=g.level(sec*1000);
    }
    assert(g.level(69000)==0);
    // A rebound spike cannot cancel a confirmed warning.
    g.sample(70000,4000,0);assert(g.low(70000));
    for(unsigned sec=71;sec<82;++sec)g.sample(sec*1000,3600,0);
    assert(!g.low(81000));
  }
  {
    BatteryGauge g;
    for(unsigned sec=0;sec<7;++sec)g.sample(sec*1000,3860,0);
    for(unsigned sec=7;sec<10;++sec){g.sample(sec*1000,2500,0);assert(!g.low(sec*1000));}
    g.sample(10000,3860,0);assert(!g.low(10000));
    assert(g.level(10000)==70); // Three low outliers are still a minority.
  }
  {
    // New supply needs five new samples; the charging estimate cannot leak.
    BatteryGauge g;
    for(unsigned sec=0;sec<7;++sec)g.sample(sec*1000,4100,1);
    g.sample(7000,3600,-1);assert(g.level(7000)==100); // Unknown isn't unplug.
    for(unsigned sec=8;sec<12;++sec){
      g.sample(sec*1000,3600,0);
      assert(g.level(sec*1000)==-1 && g.otaLevel(sec*1000)==-1);
    }
    g.sample(12000,3600,0);assert(g.level(12000)==37);
    for(unsigned sec=13;sec<18;++sec)g.sample(sec*1000,4000,1);
    assert(g.level(17000)==87 && !g.low(17000));
  }
  {
    BatteryGauge g;
    for(unsigned sec=0;sec<9;++sec)g.sample(sec*1000,3300,0);
    assert(g.low(8000));assert(!g.low(13000));
    g.sample(14000,3300,0);assert(g.level(14000)==-1 && !g.low(14000));
    for(unsigned sec=15;sec<21;++sec)g.sample(sec*1000,3300,0);
    assert(g.low(20000));
    g.sample(21000,3300,-1);assert(!g.low(21000)); // Unknown supply isn't proven battery power.
    g.sample(22000,3300,1);assert(g.level(22000)==-1 && !g.low(22000));
  }
  {
    const uint32_t start=0xfffff000u;BatteryGauge g;
    for(unsigned sec=0;sec<7;++sec)g.sample(start+sec*1000,4100,1);
    for(unsigned sec=7;sec<14;++sec)g.sample(start+sec*1000,3300,0);
    assert(g.level(start+13000)==0 && g.low(start+13000));
  }
  puts("15 battery filtering, bounded tracking, low warning, supply transition, freshness and OTA scenarios passed");
}
