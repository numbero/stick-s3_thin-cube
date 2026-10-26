#include "audio_input.h"
#include "power_save.h"
#include <M5Unified.h>
#include <math.h>
#include <string.h>
#include <atomic>

namespace {
int16_t samples[2][480];
std::atomic<bool> running{false}, refillFailed{false};
bool delivered = false;
QueueHandle_t completed = nullptr;
std::atomic<uint32_t> dropped{0};
float level = 0,rawRms=0;
uint32_t retryAt = 0;
uint32_t frames = 0;
unsigned sampleCount=480, sampleRate=48000;
// Refill from the capture task, before it moves to the next pending buffer.
// LCD drawing and network work on the main loop must not interrupt sampling.
void captured(void*, void* data, size_t length) {
  if(!running.load()||length!=sampleCount)return;
  if(xQueueSend(completed,data,0)!=pdTRUE)dropped.fetch_add(1);
  if(running.load()&&!M5.Mic.record(static_cast<int16_t*>(data),sampleCount,sampleRate))refillFailed.store(true);
}
}
void audioBegin(bool ble) {
  sampleCount=ble?160:480;sampleRate=ble?16000:48000;
  M5.Speaker.end();
  auto cfg = M5.Mic.config();
  cfg.sample_rate = sampleRate;
  cfg.magnification = 1;
  cfg.over_sampling = 1;
  M5.Mic.config(cfg);
  completed=xQueueCreate(8,sampleCount*sizeof(int16_t));
  M5.Mic.setBufferReleaseCallback(nullptr,captured);
}
void audioTick(bool capture,bool voice) {
  if(capture&&voice)powerSavePerformance();
  if (!capture||refillFailed.load()) {
    const bool wasRunning=running.exchange(false);
    if(wasRunning){M5.Mic.end();if(!capture)retryAt=millis()-1000;} // intentional handoff can restart immediately
    if(completed)xQueueReset(completed);
    refillFailed.store(false);level = rawRms = 0;
    if(!capture)return;
  }
  if (!running.load()&&completed&&uint32_t(millis()-retryAt)>=1000) {
    retryAt=millis();
    if(M5.Mic.begin()) {
      running.store(true);
      if(!M5.Mic.record(samples[0],sampleCount,sampleRate)||!M5.Mic.record(samples[1],sampleCount,sampleRate))refillFailed.store(true);
    }
  }
}
bool audioInitialized() { return completed&&M5.Mic.isEnabled(); }
bool audioReady() { return running.load()&&!refillFailed.load(); }
unsigned audioSampleRate() { return sampleRate; }
bool audioPowerDown() {
  if(running.load()||M5.Speaker.isPlaying())return false;
  // Speaker.end disables the amplifier but leaves the shared ES8311 powered
  // after the sleep cue. Repeat the pinned Mic driver's shutdown sequence;
  // Mic/Speaker.begin reinitializes the codec on the next wake/playback.
  const bool analog=M5.In_I2C.writeRegister8(0x18,0x0D,0xFC,100000);
  const bool adc=M5.In_I2C.writeRegister8(0x18,0x0E,0x6A,100000);
  const bool clock=M5.In_I2C.writeRegister8(0x18,0x00,0x00,100000);
  return analog&&adc&&clock;
}
bool audioDelivered() { return delivered; }
uint32_t audioFrameCount() { return frames; }
uint32_t audioDropCount() { return dropped.load(); }
float audioLevel() { return level; }
float audioRms() { return rawRms; }
bool audioTake(int16_t* out) {
  if(!audioReady()||xQueueReceive(completed,out,0)!=pdTRUE)return false;
  delivered = true;
  frames++;
  double sum = 0;
  for(unsigned i=0;i<sampleCount;i++)sum+=double(out[i])*out[i];
  const float rms = sqrt(sum/sampleCount)/32768.0;rawRms=rms;
  level = fminf(1, fmaxf(0, (20*log10f(fmaxf(rms, .00001f))+60)/60));
  return true;
}
