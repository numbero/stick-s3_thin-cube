#include "flick_diagnostic.h"
#include "flick_detector.h"
#include "flick_capture.h"
#include <esp_heap_caps.h>
#include "imu_service.h"
#include "device.h"
#include "deep_sleep.h"
#include "ota.h"
#include "wifi_prov.h"
#include "transport.h"
#include "connection_setup.h"
#include "power_status.h"
#include "ui.h"
#include "pet.h"
#include <M5Unified.h>

namespace {
FlickDetector detector;
FlickCaptureBuffer capture;
uint32_t captureId=0,captureGyroAt=0;
bool captureHasGyro=false,exporting=false;
size_t exportOffset=0;
#if defined(THINCUBE_FLICK_DIAG) || defined(THINCUBE_FLICK_SHORTCUT)
bool enabled=true;
#else
bool enabled=false;
#endif
bool tracing=false,haveGyro=false;
uint32_t lastGyroAt=0,traceAt=0,traceSince=0;
#ifdef THINCUBE_FLICK_SHORTCUT
constexpr bool DiagnosticOnly=false;
#else
constexpr bool DiagnosticOnly=true;
#endif
void captureReport(){
  Serial.printf("[flick-record] id=%u state=%s count=%u started_ms=%u max_gap_ms=%u capacity=6000 limit_ms=60000\n",
    unsigned(captureId),FlickCaptureBuffer::name(capture.stop),unsigned(capture.count),unsigned(capture.started),unsigned(capture.maxGap));
}
void captureBegin(uint32_t now){
  if(capture.active()||exporting){Serial.println("[flick-record-error] busy");return;}
  if(!enabled||!imuHealthy(now)||!deviceControlConnected(now)||otaActive()||provActive()||
     connectionSetupActive()||devicePairWindow()||transportBusy()||deepSleepPending()||powerCalibrationOwnsUI()){
    Serial.println("[flick-record-error] device_not_ready");return;
  }
  if(!capture.points){
    auto* points=static_cast<FlickCapturePoint*>(heap_caps_malloc(6000*sizeof(FlickCapturePoint),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!points){Serial.println("[flick-record-error] no_memory");return;}
    capture=FlickCaptureBuffer(points,6000);
  }
  if(!++captureId)++captureId;
  captureHasGyro=false;capture.start(now);captureReport();
}
void captureUsbTick(){
  if(!exporting)return;
  if(deviceStreaming()||otaActive()||deepSleepPending()){
    exporting=false;Serial.println("TCFLICK-ERROR busy");return;
  }
  for(unsigned n=0;n<8&&exportOffset<capture.count;++n,++exportOffset){
    const auto& p=capture.points[exportOffset];
    Serial.printf("TCFLICK %u %u %.6f %.6f %.6f %.5f %.5f %.5f %u %u %u %u\n",
      unsigned(exportOffset),unsigned(p.ms),p.ax,p.ay,p.az,p.gx,p.gy,p.gz,p.flags,p.stage,p.event,p.reason);
  }
  if(exportOffset==capture.count){exporting=false;Serial.printf("TCFLICK-END %u %u\n",unsigned(captureId),unsigned(capture.count));}
}
bool context(uint32_t now){
  return enabled&&deviceStreaming()&&deviceInverted()&&!deviceMuted()&&
    deviceControlConnected(now)&&!otaActive()&&!provActive()&&!connectionSetupActive()&&!devicePairWindow()&&
    !transportBusy()&&!deepSleepPending()&&!powerCalibrationOwnsUI()&&!uiLandscapeActive()&&petState()!=PET_SLEEP;
}
void logResult(FlickDetector::Result result,uint32_t now){
  if(result.event==FlickDetector::Event::None)return;
  const char* tag=result.event==FlickDetector::Event::Started?"candidate":result.event==FlickDetector::Event::FirstComplete?"first":result.event==FlickDetector::Event::Recognized?"event":"reject";
  if(result.event==FlickDetector::Event::Recognized&&!DiagnosticOnly)deviceDoubleFlick(now);
  Serial.printf("[flick-%s] ms=%u starts=%u hits=%u rejected=%u reason=%s axis=%u first_dps=%.1f second_dps=%.1f reverse_dps=%.1f travel_deg=%.1f accel_dev_g=%.3f duration_ms=%u strokes=%u diagnostic_only=%d\n",
    tag,unsigned(now),unsigned(detector.starts),unsigned(detector.hits),unsigned(detector.rejected),FlickDetector::name(result.reason),detector.axis,
    detector.peakFirst,detector.peakSecond,detector.peakReverse,detector.travel,detector.peakAccelDev,unsigned(detector.duration),detector.strokes,DiagnosticOnly);
}
}
void flickTick(uint32_t now){
  captureUsbTick();
  const bool active=context(now),button=M5.BtnA.isPressed()||M5.BtnB.isPressed();
  const auto d=imuSample();
  const auto record=[&](FlickDetector::Result result){
    capture.tick(now);
    if(!capture.active()||!d.hasAccel||!d.hasGyro||(captureHasGyro&&captureGyroAt==d.gyroAt))return;
    captureHasGyro=true;captureGyroAt=d.gyroAt;
    capture.append({now,d.accel.x,d.accel.y,d.accel.z,d.gyro.x,d.gyro.y,d.gyro.z,
      uint8_t(unsigned(active)|(unsigned(button)<<1)|(unsigned(imuHealthy(now))<<2)),
      uint8_t(detector.stage),uint8_t(result.event),uint8_t(result.reason)});
  };
  if(!active||button||!imuHealthy(now)){
    const auto result=detector.cancel(!active?FlickDetector::Reason::Context:button?FlickDetector::Reason::Button:FlickDetector::Reason::Sensor);
    record(result);logResult(result,now);
    haveGyro=false;return;
  }
  if(haveGyro&&d.gyroAt==lastGyroAt){record({});return;}
  haveGyro=true;lastGyroAt=d.gyroAt;
  const auto result=detector.update(now,d.accel.x,d.accel.y,d.accel.z,d.gyro.x,d.gyro.y,d.gyro.z,true,false);
  record(result);logResult(result,now);
  if(tracing&&uint32_t(now-traceSince)>=60000)tracing=false;
  if(tracing&&uint32_t(now-traceAt)>=10){
    traceAt=now;Serial.printf("[flick-sample] ms=%u state=%s a=%.3f,%.3f,%.3f g=%.1f,%.1f,%.1f\n",unsigned(now),FlickDetector::name(detector.stage),
      d.accel.x,d.accel.y,d.accel.z,d.gyro.x,d.gyro.y,d.gyro.z);
  }
}
FlickSnapshot flickSnapshot(){return {enabled,detector.starts,detector.hits,detector.rejected,FlickDetector::name(detector.stage)};}
void flickReport(){
  Serial.printf("[flick-status] ms=%u enabled=%d context=%d healthy=%d state=%s starts=%u hits=%u rejected=%u required_strokes=2 start_dps=300 peak_dps=500 reverse_dps=150 direction_cos=0.65 grip_filter_ms=350 arm_ms=350 settle_ms=100 first_burst_ms=220 min_separation_ms=180 max_separation_ms=900 pair_ms=1600 cooldown_ms=1200 diagnostic_only=%d\n",
    unsigned(millis()),enabled,context(millis()),imuHealthy(millis()),FlickDetector::name(detector.stage),unsigned(detector.starts),unsigned(detector.hits),unsigned(detector.rejected),DiagnosticOnly);
}
bool flickCommand(const char* command){
  if(strncmp(command,"flick",5)||!(command[5]==0||command[5]==' '))return false;
  if(!strcmp(command,"flick record")){captureBegin(millis());return true;}
  else if(!strcmp(command,"flick finish")){capture.finish();captureReport();return true;}
  else if(!strcmp(command,"flick samples")){captureReport();return true;}
  else if(!strcmp(command,"flick export")){
    if(capture.active()||!capture.count||deviceStreaming()||exporting)Serial.println("TCFLICK-ERROR stopped_capture_and_idle_voice_required");
    else{exportOffset=0;exporting=true;Serial.printf("TCFLICK-BEGIN %u %u\n",unsigned(captureId),unsigned(capture.count));}
    return true;
  }
  else if(!strcmp(command,"flick on")){enabled=true;detector.cancel(FlickDetector::Reason::Context);haveGyro=false;}
  else if(!strcmp(command,"flick off")){enabled=false;detector.cancel(FlickDetector::Reason::Context);haveGyro=false;tracing=false;}
  else if(!strcmp(command,"flick reset")){detector={};haveGyro=false;}
  else if(!strcmp(command,"flick trace")){tracing=true;traceSince=millis();traceAt=0;}
  else if(!strcmp(command,"flick trace off"))tracing=false;
  else if(strcmp(command,"flick")&&strcmp(command,"flick status"))Serial.println("[flick-help] flick on | off | reset | status | trace | trace off; valid voice session required");
  flickReport();return true;
}
