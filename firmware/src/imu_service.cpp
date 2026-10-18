#include "imu_service.h"
#include "imu_conversion.h"
#include "tap_diagnostic.h"
#include <M5Unified.h>
#include "device.h"
#include "deep_sleep.h"
#include "ota.h"
#include "wifi_prov.h"
#include "transport.h"
#include "connection_setup.h"
#include "ui.h"
#include "pet.h"
#ifdef THINCUBE_IMU_LEGACY
#include <bmi270_legacy.h>
#include <nvs.h>
#include <esp_heap_caps.h>
#include "imu_fifo_capture.h"
#endif

namespace {
ImuSample sample;
bool ready=false,suspended=false;
uint32_t lastPoll=0,maxGap=0,errors=0;
bool havePolled=false;
#ifdef THINCUBE_IMU_LEGACY
bmi2_dev sensor{};
int32_t offsets[6]{};
bool offsetsLoaded=false,tapReady=false,manual=false;
#if defined(THINCUBE_FLICK_DIAG) || defined(THINCUBE_FLICK_SHORTCUT)
bool tapRequested=false;
#else
bool tapRequested=true;
#endif
int8_t lastError=0;
TapDiagnostic taps;
uint16_t sensitivity=9,axis=2,tapDataReg=1;
bool checked(int8_t result){lastError=result;if(result!=BMI2_OK){++errors;return false;}return true;}
struct FifoBus {
  bool read(uint8_t r,uint8_t* v,size_t n){return imuRead(r,v,n);}
  bool write(uint8_t r,const uint8_t* v,size_t n){return imuWrite(r,v,n);}
  void pause(unsigned ms){delay(ms);}
};
struct WavePoint {int16_t x,y,z;};
constexpr unsigned WaveCapacity=12000; // Bounded 60s, allocated in PSRAM only on request.
WavePoint* wave=nullptr;
unsigned waveCount=0,waveExport=0;
uint32_t waveStarted=0;
bool waveActive=false,waveOverflow=false,waveExporting=false,waveRestored=true;
ImuFifoCapture<FifoBus> fifoCapture;
void waveReport(){
  int16_t low[3]={32767,32767,32767},high[3]={-32768,-32768,-32768};
  for(unsigned i=0;i<waveCount;++i){const int16_t v[]={wave[i].x,wave[i].y,wave[i].z};for(unsigned a=0;a<3;++a){if(v[a]<low[a])low[a]=v[a];if(v[a]>high[a])high[a]=v[a];}}
  Serial.printf("[tap-wave] active=%d samples=%u period_us=5000 range_g=8 filtered=1 overflow=%d restored=%d min=%d,%d,%d max=%d,%d,%d\n",
    waveActive,waveCount,waveOverflow,waveRestored,low[0],low[1],low[2],high[0],high[1],high[2]);
}
bool waveStop(){
  waveActive=false;FifoBus bus;waveRestored=fifoCapture.restore(bus);waveReport();return waveRestored;
}
bool waveBegin(){
  if(waveActive||waveExporting||suspended||!ready||otaActive()||provActive()||devicePairWindow()||transportBusy()||deepSleepPending())return false;
  if(!waveRestored&&!waveStop())return false;
  if(!wave)wave=static_cast<WavePoint*>(heap_caps_malloc(WaveCapacity*sizeof(WavePoint),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!wave){Serial.println("[tap-wave] unavailable: PSRAM allocation");return false;}
  waveCount=0;waveOverflow=false;FifoBus bus;
  if(!fifoCapture.begin(bus)){waveRestored=fifoCapture.restore(bus);return false;}
  waveStarted=millis();waveActive=true;waveRestored=false;waveReport();return true;
}
void wavePoll(uint32_t now){
  if(waveActive){
    uint16_t length=0;
    if(!checked(bmi2_get_fifo_length(&length,&sensor))){waveOverflow=true;waveStop();return;}
    if(length>=2000)waveOverflow=true;
    // Drain at most 40 complete samples per loop; FIFO bridges display stalls.
    uint8_t bytes[240];bmi2_sens_axes_data points[40];
    length=uint16_t((length>sizeof(bytes)?sizeof(bytes):length)/6*6);
    if(length){
      bmi2_fifo_frame frame{};frame.data=bytes;frame.length=length;
      if(!checked(bmi2_read_fifo_data(&frame,&sensor))){waveOverflow=true;waveStop();return;}
      uint16_t count=40;const int8_t result=bmi2_extract_accel(points,&count,&frame,&sensor);
      if(result<0){checked(result);waveOverflow=true;waveStop();return;}
      for(unsigned i=0;i<count;++i){if(waveCount==WaveCapacity)break;wave[waveCount++]={points[i].x,points[i].y,points[i].z};}
    }
    if(waveCount==WaveCapacity||uint32_t(now-waveStarted)>=60000)waveStop();
  }
  if(waveExporting){
    if(deviceStreaming()||deviceCaptureRequested()||otaActive()||provActive()||deepSleepPending()){
      waveExporting=false;Serial.println("TCWAVE-CANCELLED busy");return;
    }
    for(unsigned n=0;n<8&&waveExport<waveCount;++n,++waveExport){
      const auto& p=wave[waveExport];Serial.printf("TCWAVE %u %d %d %d\n",waveExport,p.x,p.y,p.z);
    }
    if(waveExport==waveCount){waveExporting=false;Serial.println("TCWAVE-END");}
  }
}
BMI2_INTF_RETURN_TYPE readBus(uint8_t reg,uint8_t* data,uint32_t length,void*){
  return M5.In_I2C.readRegister(0x68,reg,data,length,400000)?BMI2_INTF_RET_SUCCESS:-1;
}
BMI2_INTF_RETURN_TYPE writeBus(uint8_t reg,const uint8_t* data,uint32_t length,void*){
  return M5.In_I2C.writeRegister(0x68,reg,data,length,400000)?BMI2_INTF_RET_SUCCESS:-1;
}
void pauseBus(uint32_t us,void*){if(us>=1000)delay(us/1000);if(us%1000)delayMicroseconds(us%1000);}
bool clearEvents(){uint16_t status=0;return checked(bmi2_get_int_status(&status,&sensor));}
bool configureTap(bool enable){
  uint8_t list[]={BMI2_SINGLE_TAP,BMI2_DOUBLE_TAP,BMI2_TRIPLE_TAP};
  tapReady=false;
  if(!checked(bmi270_legacy_sensor_disable(list,3,&sensor)))return false;
  uint8_t page[16]{};
  if(!checked(bmi2_get_feat_config(5,page,&sensor)))return false;
  if((page[0]&7)!=0){++errors;return false;}
  if(!enable)return clearEvents();
  bmi2_sens_config cfg{};cfg.type=BMI2_TAP;
  if(!checked(bmi270_legacy_get_sensor_config(&cfg,1,&sensor)))return false;
  cfg.cfg.tap_conf.data_reg_en=tapDataReg;cfg.cfg.tap_conf.tap_sens_thres=sensitivity;
  cfg.cfg.tap_conf.max_gest_dur=80; // 400ms at 200Hz
  cfg.cfg.tap_conf.quite_time_after_gest=80;
  cfg.cfg.tap_conf.wait_for_timeout=1;cfg.cfg.tap_conf.axis_sel=axis;
  if(!checked(bmi270_legacy_set_sensor_config(&cfg,1,&sensor)))return false;
  bmi2_sens_config readback{};readback.type=BMI2_TAP;
  if(!checked(bmi270_legacy_get_sensor_config(&readback,1,&sensor)))return false;
  const auto& v=readback.cfg.tap_conf;
  if(v.data_reg_en!=tapDataReg||v.tap_sens_thres!=sensitivity||v.max_gest_dur!=80||
     v.quite_time_after_gest!=80||v.wait_for_timeout!=1||v.axis_sel!=axis){++errors;return false;}
  // Configure the feature route as in Bosch's example, with physical INT1
  // output disabled while online. PM1 wake routing remains disarmed.
  bmi2_sens_int_config route{};route.type=BMI2_TAP;route.hw_int_pin=BMI2_INT1;
  if(!checked(bmi270_legacy_map_feat_int(&route,1,&sensor)))return false;
  uint8_t pin=0x02; // input only, output disabled
  if(!imuWrite(0x53,&pin,1)||!checked(bmi270_legacy_sensor_enable(&list[1],1,&sensor))||
     !checked(bmi2_get_feat_config(5,page,&sensor)))return false;
  if((page[0]&7)!=2){++errors;return false;}
  if(!clearEvents())return false;
  tapReady=true;return true;
}
bool diagnosticContext(uint32_t now){
  const bool safe=!otaActive()&&!provActive()&&!connectionSetupActive()&&!devicePairWindow()&&
    !transportBusy()&&!deepSleepPending()&&!uiLandscapeActive()&&petState()!=PET_SLEEP&&imuHealthy(now);
  return safe&&(manual||(deviceStreaming()&&deviceInverted()));
}
void invalidate(){sample.hasAccel=sample.hasGyro=false;taps.context=false;havePolled=false;}
#endif
}

bool imuLegacy(){
#ifdef THINCUBE_IMU_LEGACY
  return true;
#else
  return false;
#endif
}
bool imuRead(uint8_t reg,uint8_t* data,size_t length){
#ifdef THINCUBE_IMU_LEGACY
  return checked(bmi2_get_regs(reg,data,length,&sensor));
#else
  auto* imu=M5.Imu.getImuInstancePtr(0);return imu&&imu->readRegister(reg,data,length);
#endif
}
bool imuWrite(uint8_t reg,const uint8_t* data,size_t length){
#ifdef THINCUBE_IMU_LEGACY
  return checked(bmi2_set_regs(reg,data,length,&sensor));
#else
  auto* imu=M5.Imu.getImuInstancePtr(0);return imu&&imu->writeRegister(reg,const_cast<uint8_t*>(data),length);
#endif
}
bool imuBegin(){
#ifdef THINCUBE_IMU_LEGACY
  if(waveActive||!waveRestored)waveStop();waveExporting=false;
#endif
  ready=false;suspended=false;sample={};havePolled=false;
#ifdef THINCUBE_IMU_LEGACY
  sensor={};sensor.intf=BMI2_I2C_INTF;sensor.read=readBus;sensor.write=writeBus;
  sensor.delay_us=pauseBus;sensor.read_write_len=32;
  if(!checked(bmi270_legacy_init(&sensor)))return false;
  if(!checked(bmi2_set_adv_power_save(BMI2_DISABLE,&sensor)))return false;
  bmi2_sens_config config[2]{};config[0].type=BMI2_ACCEL;config[1].type=BMI2_GYRO;
  if(!checked(bmi270_legacy_get_sensor_config(config,2,&sensor)))return false;
  config[0].cfg.acc.odr=BMI2_ACC_ODR_200HZ;config[0].cfg.acc.range=BMI2_ACC_RANGE_8G;
  config[0].cfg.acc.bwp=BMI2_ACC_NORMAL_AVG4;config[0].cfg.acc.filter_perf=BMI2_PERF_OPT_MODE;
  config[1].cfg.gyr.odr=BMI2_GYR_ODR_100HZ;config[1].cfg.gyr.range=BMI2_GYR_RANGE_2000;
  config[1].cfg.gyr.bwp=BMI2_GYR_NORMAL_MODE;config[1].cfg.gyr.filter_perf=BMI2_PERF_OPT_MODE;
  config[1].cfg.gyr.noise_perf=BMI2_PERF_OPT_MODE;
  if(!checked(bmi270_legacy_set_sensor_config(config,2,&sensor)))return false;
  uint8_t sensors[]={BMI2_ACCEL,BMI2_GYRO};
  if(!checked(bmi270_legacy_sensor_enable(sensors,2,&sensor)))return false;
  uint8_t map=0x04,latch=0;
  if(!imuWrite(0x58,&map,1)||!imuWrite(0x55,&latch,1))return false;
  uint8_t regs[4]{},status=0;
  if(!imuRead(0x40,regs,4)||regs[0]!=0xa9||regs[1]!=2||regs[2]!=0xe8||regs[3]!=0||
     !imuRead(0x21,&status,1)||(status&0x6f)!=1)return false;
  // Read the old library namespace without writing/migrating user settings.
  const char* keys[]={"ax","ay","az","gx","gy","gz"};
  nvs_handle_t nvs;offsetsLoaded=nvs_open("M5Unified",NVS_READONLY,&nvs)==ESP_OK;
  for(unsigned i=0;i<6;++i){offsets[i]=0;if(offsetsLoaded)nvs_get_i32(nvs,keys[i],&offsets[i]);}
  if(offsetsLoaded)nvs_close(nvs);
  ready=true;const bool tapOK=configureTap(tapRequested);
  Serial.printf("[imu-service] backend=legacy raw=1 tap=%d offsets=%s accel=200Hz/8g gyro=100Hz/2000dps\n",tapOK,offsetsLoaded?"M5Unified":"zero");
#else
  ready=M5.Imu.isEnabled()||M5.Imu.begin(nullptr,M5.getBoard());
  Serial.printf("[imu-service] backend=base raw=%d tap=unavailable\n",ready);
#endif
  return ready;
}
bool imuEnabled(){return ready;}
bool imuHealthy(uint32_t now){return ready&&!suspended&&sample.hasAccel&&sample.hasGyro&&
  uint32_t(now-sample.accelAt)<500&&uint32_t(now-sample.gyroAt)<500;}
ImuSample imuSample(){return sample;}
void imuPoll(uint32_t now){
  if(!ready||suspended||(havePolled&&uint32_t(now-lastPoll)<10))return;
  if(havePolled){const uint32_t gap=now-lastPoll;if(gap>maxGap)maxGap=gap;}
  lastPoll=now;havePolled=true;
#ifdef THINCUBE_IMU_LEGACY
  taps.update(now,diagnosticContext(now),M5.BtnA.isPressed()||M5.BtnB.isPressed());
  uint16_t status=0;
  if(!checked(bmi2_get_int_status(&status,&sensor)))return;
  uint8_t data[12];
  if((status&0xc000)&&imuRead(0x0c,data,sizeof(data))){
    int16_t raw[6];for(unsigned i=0;i<6;++i)raw[i]=int16_t(uint16_t(data[2*i])|(uint16_t(data[2*i+1])<<8));
    float out[3];
    if(status&0x8000){imuConvert(raw,offsets,8.0f/32768,out);sample.accel={out[0],out[1],out[2]};sample.accelAt=now;sample.hasAccel=true;}
    if(status&0x4000){imuConvert(raw+3,offsets+3,2000.0f/32768,out);sample.gyro={out[0],out[1],out[2]};sample.gyroAt=now;sample.hasGyro=true;}
    ++sample.sequence;
  }
  // Read the output only for a NEW status event, including consecutive events
  // of the same type; output values can remain latched between gestures.
  if(status&BMI270_LEGACY_TAP_STATUS_MASK){
    uint8_t output=0;if(!imuRead(BMI270_LEGACY_TAP_STATUS_REG,&output,1))return;
    if(tapReady&&(output&0xe0)==BMI270_LEGACY_DOUBLE_TAP_MASK){
      const auto reason=taps.event(now);
      Serial.printf("[tap-event] ms=%u raw=%u accepted=%u blocked=%u reason=%s type=%02x mode=%s pose=%u buttons=%u\n",
        unsigned(now),unsigned(taps.raw),unsigned(taps.accepted),unsigned(taps.blocked),TapDiagnostic::name(reason),output,
        manual?"manual":"voice",unsigned(devicePose()),unsigned(M5.BtnA.isPressed())|(unsigned(M5.BtnB.isPressed())<<1));
    }else ++taps.unknown;
  }
  wavePoll(now);
#else
  const auto result=M5.Imu.update();if(!result)return;
  const auto d=M5.Imu.getImuData();
  if(result&1){sample.accel={d.accel.x,d.accel.y,d.accel.z};sample.accelAt=now;sample.hasAccel=true;}
  if(result&2){sample.gyro={d.gyro.x,d.gyro.y,d.gyro.z};sample.gyroAt=now;sample.hasGyro=true;}
  ++sample.sequence;
#endif
}
bool imuSuspendForSleep(){
#ifdef THINCUBE_IMU_LEGACY
  waveExporting=false;if((waveActive||!waveRestored)&&!waveStop())return false;
  if(!ready||!configureTap(false))return false;
  invalidate();
#endif
  suspended=true;return true;
}
void imuResumeAfterSleep(){
  suspended=false;sample.hasAccel=sample.hasGyro=false;havePolled=false;
#ifdef THINCUBE_IMU_LEGACY
  taps.context=false;configureTap(tapRequested);
#endif
}
void imuReport(){
#ifdef THINCUBE_IMU_LEGACY
  Serial.printf("[tap-status] ms=%u backend=legacy raw_ready=%d healthy=%d enabled=%d requested=%d mode=%s axis=%u sensitivity=%u source=%s window_ms=400 quiet_ms=400 raw=%u accepted=%u blocked=%u unknown=%u errors=%u last_error=%d max_poll_ms=%u\n",
    unsigned(millis()),ready,imuHealthy(millis()),tapReady,tapRequested,manual?"manual":"voice",axis,sensitivity,
    tapDataReg?"user":"internal",
    unsigned(taps.raw),unsigned(taps.accepted),unsigned(taps.blocked),unsigned(taps.unknown),unsigned(errors),lastError,unsigned(maxGap));
#else
  Serial.printf("[tap-status] backend=base tap=unavailable healthy=%d\n",imuHealthy(millis()));
#endif
}
TapSnapshot imuTapSnapshot(){
#ifdef THINCUBE_IMU_LEGACY
  return {true,tapReady,taps.raw,taps.accepted,taps.blocked};
#else
  return {};
#endif
}
bool imuDiagnosticCommand(const char* command){
  if(strncmp(command,"tap",3)||!(command[3]==0||command[3]==' '))return false;
#ifdef THINCUBE_IMU_LEGACY
  if(!strcmp(command,"tap record")){if(!waveBegin())Serial.println("[tap-wave] capture refused");return true;}
  if(!strcmp(command,"tap finish")){if(waveActive)wavePoll(millis());waveStop();return true;}
  if(!strcmp(command,"tap wave")){waveReport();return true;}
  if(!strcmp(command,"tap export")){
    if(!waveCount||waveActive||!waveRestored||deviceStreaming()||deviceCaptureRequested())Serial.println("[tap-wave] export requires stopped capture and idle voice");
    else{waveExport=0;waveExporting=true;Serial.printf("TCWAVE-BEGIN %u 200 8 sensor\n",waveCount);}
    return true;
  }
  if(waveActive&&strcmp(command,"tap status")&&strcmp(command,"tap")){
    Serial.println("[tap-wave] finish capture before changing parameters");return true;
  }
  if(!strcmp(command,"tap sleepcheck")){deepSleepDiagnostic();imuReport();return true;}
  if(!strcmp(command,"tap reset")){taps={};maxGap=0;}
  else if(!strcmp(command,"tap off")){tapRequested=false;if(ready&&!suspended)configureTap(false);}
  else if(!strcmp(command,"tap on")||!strcmp(command,"tap auto")){
    tapRequested=true;manual=!strcmp(command,"tap on");taps.context=false;
    if(ready&&!suspended)configureTap(true);
  }else if(!strncmp(command,"tap sens ",9)){
    char* end=nullptr;const long value=strtol(command+9,&end,10);
    if(end!=command+9&&*end==0&&value>=0&&value<=15){sensitivity=value;taps.context=false;if(ready&&!suspended)configureTap(tapRequested);}
    else Serial.println("[tap-help] sensitivity range 0..15; smaller is more sensitive");
  }else if(!strcmp(command,"tap internal")||!strcmp(command,"tap user")){
    tapDataReg=!strcmp(command,"tap user");taps.context=false;if(ready&&!suspended)configureTap(tapRequested);
  }else if(!strcmp(command,"tap dump")){
    uint8_t p5[16]{},p6[16]{},r[4]{},status=0,map=0,power=0;
    const bool ok=checked(bmi2_get_feat_config(5,p5,&sensor))&&checked(bmi2_get_feat_config(6,p6,&sensor))&&
      imuRead(0x40,r,4)&&imuRead(0x21,&status,1)&&imuRead(0x56,&map,1)&&imuRead(0x7d,&power,1);
    Serial.printf("[tap-config] ok=%d flags=%02x%02x sensitivity=%u duration=%u quiet=%u wait=%u axis=%u acc=%02x/%02x gyro=%02x/%02x internal=%02x int1map=%02x power=%02x\n",
      ok,p5[1],p5[0],unsigned(p5[4]|(p5[5]<<8)),unsigned(p5[6]|(p5[7]<<8)),unsigned(p5[14]|(p5[15]<<8)),
      unsigned(p6[0]|(p6[1]<<8)),unsigned(p6[4]|(p6[5]<<8)),r[0],r[1],r[2],r[3],status,map,power);
  }else if(!strncmp(command,"tap axis ",9)&&strlen(command)==10&&command[9]>='x'&&command[9]<='z'){
    axis=command[9]-'x';taps.context=false;if(ready&&!suspended)configureTap(tapRequested);
  }else if(strcmp(command,"tap")&&strcmp(command,"tap status")){
    Serial.println("[tap-help] tap status | on | auto | off | reset | axis x/y/z | sens 0..15 | internal | user | dump | sleepcheck");
  }
#endif
  imuReport();return true;
}
