#pragma once
// Deterministic port for the real audio_input.cpp and power_save.cpp. It models
// completed Mic buffers and late callbacks at end(), not the electrical codec.
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>
#include "audio_input.h"
#include "power_save.h"
#include "power_save_policy.h"

struct AudioPowerHost {
  uint32_t now=2000;
  unsigned cpu=240,frequencyChanges=0,begins=0,ends=0;
  bool bleIdle=false,frequencyFails=false,beginFails=false,recordFails=false;
};
inline AudioPowerHost host;
inline uint32_t millis(){return host.now;}
inline unsigned getCpuFrequencyMhz(){return host.cpu;}
inline bool setCpuFrequencyMhz(unsigned mhz){
  if(host.frequencyFails)return false;
  host.cpu=mhz;++host.frequencyChanges;return true;
}
inline void bleSetIdle(bool idle){host.bleIdle=idle;}
struct HostSerial {void printf(const char*,...) {}};
inline HostSerial Serial;

struct HostQueue {
  size_t capacity,size;
  std::deque<std::vector<uint8_t>> entries;
};
using QueueHandle_t=HostQueue*;
constexpr int pdTRUE=1;
inline std::vector<std::unique_ptr<HostQueue>> queues;
inline QueueHandle_t xQueueCreate(size_t capacity,size_t size){
  queues.emplace_back(new HostQueue{capacity,size,{}});return queues.back().get();
}
inline int xQueueSend(QueueHandle_t queue,const void* data,int){
  if(queue->entries.size()==queue->capacity)return 0;
  const auto* begin=static_cast<const uint8_t*>(data);
  queue->entries.emplace_back(begin,begin+queue->size);return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t queue,void* data,int){
  if(queue->entries.empty())return 0;
  std::memcpy(data,queue->entries.front().data(),queue->size);
  queue->entries.pop_front();return pdTRUE;
}
inline void xQueueReset(QueueHandle_t queue){queue->entries.clear();}

struct HostMic {
  struct Config {unsigned sample_rate=16000,magnification=1,over_sampling=1;} cfg;
  struct Request {int16_t* data;size_t count;unsigned rate;};
  std::deque<Request> requests;
  void* args=nullptr;
  void (*callback)(void*,void*,size_t)=nullptr;
  bool live=false;
  Config config()const{return cfg;}
  void config(Config next){assert(!live);cfg=next;}
  bool begin(){++host.begins;live=!host.beginFails;return live;}
  void end(){
    ++host.ends;
    // An already-entered callback can arrive while the task is being joined.
    if(!requests.empty())callback(args,requests.front().data,requests.front().count);
    requests.clear();live=false;
  }
  bool isEnabled()const{return true;}
  bool record(int16_t* data,size_t count,unsigned rate){
    assert(live&&count==rate/100&&rate==cfg.sample_rate);
    if(host.recordFails)return false;
    requests.push_back({data,count,rate});return true;
  }
  void setBufferReleaseCallback(void* value,void (*fn)(void*,void*,size_t)){args=value;callback=fn;}
  void complete(int16_t value){
    assert(live&&!requests.empty());const auto r=requests.front();requests.pop_front();
    std::fill(r.data,r.data+r.count,value);callback(args,r.data,r.count);
  }
};
struct HostSpeaker {void end(){}bool isPlaying()const{return false;}};
struct HostI2C {bool writeRegister8(int,int,int,int){return true;}};
struct HostM5 {HostMic Mic;HostSpeaker Speaker;HostI2C In_I2C;};
inline HostM5 M5;
