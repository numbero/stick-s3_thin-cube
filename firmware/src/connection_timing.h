#pragma once
#include <stdint.h>

enum class ConnectionStage : uint8_t {
  Boot, Peripherals, Motion, Device, Radio, Advertising, Channel, Secured, MTU,
  ControlSubscribed, AudioSubscribed, Info, Challenge, Authenticated, Session,
  SnapshotQueued, ControlReady, PerformanceReady, AudioReady, FirstAudioQueued,
  WifiStart, WifiAssociated, WifiIP, Services, Bonjour, Disconnected, Count
};

// No clocks or I/O: monotonic uint32 arithmetic also handles millis() wrap.
struct ConnectionTiming {
  uint32_t start=0,previous=0,seen=0;
  bool active=false;
  void begin(uint32_t now){start=previous=now;seen=0;active=true;}
  bool mark(ConnectionStage stage,uint32_t now,uint32_t& elapsed,uint32_t& step){
    const auto bit=unsigned(stage);
    if(!active||bit>=unsigned(ConnectionStage::Count)||(seen&(1u<<bit)))return false;
    seen|=1u<<bit;elapsed=uint32_t(now-start);step=uint32_t(now-previous);previous=now;
    return true;
  }
};
void connectionBootStart();
void connectionBootMark(ConnectionStage stage);
void connectionTimingBegin(const char* transport,uint32_t attempt);
void connectionTimingMark(ConnectionStage stage,uint32_t value=0);
void connectionTimingFlush();
void connectionTimingObserveWiFi();
