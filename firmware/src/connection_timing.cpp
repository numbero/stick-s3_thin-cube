#include "connection_timing.h"
#include <Arduino.h>
#include <algorithm>
#include <WiFi.h>

namespace {
const char* names[]={"boot","peripherals","motion","device","radio","advertising","channel","secured","mtu",
  "control_subscribed","audio_subscribed","info","challenge","authenticated","session","snapshot_queued",
  "control_ready","performance_ready","audio_ready","first_audio_queued","wifi_start","wifi_associated",
  "wifi_ip","services","bonjour","disconnected"};
static_assert(sizeof(names)/sizeof(*names)==unsigned(ConnectionStage::Count),"milestone names");
portMUX_TYPE timingMux=portMUX_INITIALIZER_UNLOCKED;
ConnectionTiming boot,linkTimeline;
const char* mode="none";
uint32_t attempt=0,dropped=0;
struct Row {const char* mode;uint32_t attempt,elapsed,step,value;ConnectionStage stage;};
Row rows[64];unsigned head=0,count=0;
void enqueue(ConnectionTiming& timeline,const char* transport,uint32_t id,ConnectionStage stage,uint32_t value){
  uint32_t elapsed,step;
  if(!timeline.mark(stage,millis(),elapsed,step))return;
  if(count==64){dropped++;return;}
  rows[(head+count)%64]={transport,id,elapsed,step,value,stage};count++;
}
}
void connectionBootStart(){portENTER_CRITICAL(&timingMux);boot.begin(millis());enqueue(boot,"boot",0,ConnectionStage::Boot,0);portEXIT_CRITICAL(&timingMux);}
void connectionBootMark(ConnectionStage stage){portENTER_CRITICAL(&timingMux);enqueue(boot,"boot",0,stage,0);portEXIT_CRITICAL(&timingMux);}
void connectionTimingBegin(const char* transport,uint32_t id){
  portENTER_CRITICAL(&timingMux);mode=transport;attempt=id;linkTimeline.begin(millis());portEXIT_CRITICAL(&timingMux);
}
void connectionTimingMark(ConnectionStage stage,uint32_t value){
  portENTER_CRITICAL(&timingMux);enqueue(linkTimeline,mode,attempt,stage,value);portEXIT_CRITICAL(&timingMux);
}
void connectionTimingFlush(){
  // GAP callbacks only enqueue. Emit complete lines when the serial buffer has room.
  static char line[192];static size_t length=0,offset=0;
  if(offset==length){
    Row row{};uint32_t lost=0;bool have=false;
    portENTER_CRITICAL(&timingMux);
    if(count){row=rows[head];head=(head+1)%64;count--;have=true;lost=dropped;dropped=0;}
    portEXIT_CRITICAL(&timingMux);
    if(!have)return;
    length=snprintf(line,sizeof(line),"[connect-timing] %s #%u %s t=%u dt=%u v=%u lost=%u\n",
      row.mode,unsigned(row.attempt),names[unsigned(row.stage)],unsigned(row.elapsed),unsigned(row.step),unsigned(row.value),unsigned(lost));
    length=std::min(length,sizeof(line)-1);offset=0;
  }
  const int available=Serial.availableForWrite();
  if(available>=int(length-offset))offset+=Serial.write(reinterpret_cast<const uint8_t*>(line+offset),length-offset);
}

void connectionTimingObserveWiFi(){
  WiFi.onEvent([](WiFiEvent_t event,WiFiEventInfo_t info){
    static uint32_t wifiAttempt=0;
    if(event==ARDUINO_EVENT_WIFI_STA_START){connectionTimingBegin("wifi",++wifiAttempt);connectionTimingMark(ConnectionStage::WifiStart);}
    else if(event==ARDUINO_EVENT_WIFI_STA_CONNECTED)connectionTimingMark(ConnectionStage::WifiAssociated);
    else if(event==ARDUINO_EVENT_WIFI_STA_GOT_IP)connectionTimingMark(ConnectionStage::WifiIP);
    else if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED){
      connectionTimingMark(ConnectionStage::Disconnected,info.wifi_sta_disconnected.reason);
      connectionTimingBegin("wifi",++wifiAttempt);
    }
  });
}
