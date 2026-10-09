/* ThinCube v0.3 · Clawd / Wi-Fi microphone / paired Mac control / A/B OTA.
   当前流程与通信协议见 README.md 与 docs/thincube-protocol-v1.md。 */
#include <M5Unified.h>
#include <WiFi.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "ui.h"
#include "pet.h"
#include "motion.h"
#include "wifi_prov.h"
#include "ota.h"
#include "device.h"
#include "audio_input.h"
#include "power_status.h"
#include "transport.h"
#include "ble_link.h"
#include <esp_heap_caps.h>

#ifndef CLAWD_FW_VER
#define CLAWD_FW_VER "dev"
#endif

static void reportBootInfo(){
  Serial.printf("\n=== clawd %s ===\n", CLAWD_FW_VER);
  Serial.printf("chip      : %s rev%d, %d cores @ %u MHz\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), ESP.getCpuFreqMHz());
  Serial.printf("flash     : %u KB\n", (unsigned)(ESP.getFlashChipSize() / 1024));
  Serial.printf("psram     : %u KB (free %u KB)\n",
                (unsigned)(ESP.getPsramSize() / 1024), (unsigned)(ESP.getFreePsram() / 1024));
  Serial.printf("mac       : %s\n", WiFi.macAddress().c_str());

  const esp_partition_t* run  = esp_ota_get_running_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  Serial.printf("running   : %s @ 0x%06x (%u KB)\n", run->label,
                (unsigned)run->address, (unsigned)(run->size / 1024));
  Serial.printf("next ota  : %s @ 0x%06x (%u KB)\n", next ? next->label : "(none)",
                next ? (unsigned)next->address : 0, next ? (unsigned)(next->size / 1024) : 0);

  esp_ota_img_states_t st;
  if (esp_ota_get_state_partition(run, &st) == ESP_OK){
    const char* s = "unknown";
    switch (st){
      case ESP_OTA_IMG_NEW:            s = "new"; break;
      case ESP_OTA_IMG_PENDING_VERIFY: s = "pending_verify"; break;
      case ESP_OTA_IMG_VALID:          s = "valid"; break;
      case ESP_OTA_IMG_INVALID:        s = "invalid"; break;
      case ESP_OTA_IMG_ABORTED:        s = "aborted"; break;
      case ESP_OTA_IMG_UNDEFINED:      s = "undefined"; break;
    }
    Serial.printf("ota state : %s\n", s);
  }
  Serial.printf("free heap : %u KB\n\n", (unsigned)(ESP.getFreeHeap() / 1024));
}

// USB chip reset can interrupt a peripheral read while the slave holds SDA.
// Release that transaction before M5GFX's board-detection pull-up test (NXP
// UM10204 bus-clear procedure); both pins stay open-drain throughout recovery.
bool recoverPeripheralBus(){
  constexpr int sda=47,scl=48;
  pinMode(sda,INPUT_PULLUP);pinMode(scl,INPUT_PULLUP);delay(5);
  if(digitalRead(sda)||!digitalRead(scl))return false;
  digitalWrite(scl,HIGH);pinMode(scl,OUTPUT_OPEN_DRAIN);
  for(int pulse=0;pulse<9;pulse++){
    digitalWrite(scl,LOW);delayMicroseconds(10);digitalWrite(scl,HIGH);
    const uint32_t start=micros();while(!digitalRead(scl)&&uint32_t(micros()-start)<1000)delayMicroseconds(5);
    if(!digitalRead(scl))break;delayMicroseconds(10);
  }
  digitalWrite(scl,LOW);digitalWrite(sda,LOW);pinMode(sda,OUTPUT_OPEN_DRAIN);delayMicroseconds(10);
  digitalWrite(scl,HIGH);delayMicroseconds(10);digitalWrite(sda,HIGH);delayMicroseconds(10);
  pinMode(sda,INPUT_PULLUP);pinMode(scl,INPUT_PULLUP);
  return digitalRead(sda)&&digitalRead(scl);
}
void setup(){
  const bool busRecovered=recoverPeripheralBus();
  auto cfg = M5.config();
  cfg.fallback_board=m5::board_t::board_M5StickS3;
  M5.begin(cfg);
  if(!M5.Display.width()){delay(100);M5.Display.init();}
  Serial.begin(115200);
  delay(200);
  Serial.printf("[boot] peripheral bus recovered %d board %u LCD %dx%d\n",busRecovered,unsigned(M5.getBoard()),M5.Display.width(),M5.Display.height());

  M5.Display.setRotation(0);       // 竖屏 135×240
  M5.Display.setBrightness(128);   // 屏幕常亮太费电，先给 50%
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSans9pt7b);

  /* IMU 显式启用一次，并在串口报告状态（不启用则姿态输入整条链路都不工作） */
  if (!M5.Imu.isEnabled()) M5.Imu.begin();
  Serial.printf("imu       : %s\n", M5.Imu.isEnabled() ? "enabled" : "NOT AVAILABLE");

  reportBootInfo();

  uiBegin();
  transportBegin();
  M5.update();
  const bool bootProvisioning=M5.BtnB.isPressed();
  if(bootProvisioning)transportProvisioningOverride();
  deviceBegin();
  M5.Display.setBrightness(deviceBrightness());
  motionBegin();
  uiLog("boot ok");
  uiLog("lcd 135x240");
  uiLog("clawd %s up", CLAWD_FW_VER);
  uiDumpMetrics();          /* 排版按实测度量算，串口留一份 */

  /* 网络：NVS 有凭据直接连 STA；没凭据、或开机就按住 B → 进配网（见 docs/wifi-prov-ota.md §2.1） */
  M5.update();
  static char ssid[33], pass[65];
  const bool haveCreds = provLoadCreds(ssid, sizeof(ssid), pass, sizeof(pass));
  if (transportMode()==TransportMode::BLE){
    if(!bleBegin(deviceName(),strtoul(devicePairPin(),nullptr,10)))uiLog("BLE init failed");
  } else if (!haveCreds || bootProvisioning){
    provBegin();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    Serial.printf("[wifi] joining %s\n", ssid);
    uiLog("wifi join");
  }
  transportStarted(millis());
}

void loop(){
  M5.update();
  const uint32_t ms = millis();
  powerTick(ms);
  if(transportMode()==TransportMode::BLE)bleTick(ms);
  transportTick(ms);
  provTick();               /* 配网状态机（未进配网时是 no-op） */
  otaTick(ms);              /* 联网后起 OTA httpd + 确认超时 / 重启 */
  uiTick(ms);

  /* 稳定运行 60 秒才认账，放弃回滚窗口（docs/wifi-prov-ota.md §3.4） */
  static bool validMarked = false;
  if (!validMarked && ms > 60000 && uiHealthy() && deviceHealthy() && motionDiag().updates>0){ validMarked=otaMarkValid(); }
  if (!validMarked) otaRollbackUnhealthy();
  deviceTick(ms,petState()==PET_SLEEP,provActive()||otaActive());
  uiSetOrientation(deviceDisplayRotation(),ms);
  static uint8_t appliedBrightness=0;
  if(petState()!=PET_SLEEP&&appliedBrightness!=deviceBrightness()){appliedBrightness=deviceBrightness();M5.Display.setBrightness(appliedBrightness);}
  char nextSSID[33],nextPass[65];
  if(!otaActive()&&deviceTakeNetwork(nextSSID,nextPass)){deviceStop();audioTick(false);otaStopServer();provBeginWithCredentials(nextSSID,nextPass);memset(nextPass,0,sizeof(nextPass));}

  /* 心跳：每 5 秒一行，不用接屏幕也能判断固件是否活着、跑在哪个槽 */
  static uint32_t lastBeat = 0;
  if (ms - lastBeat > 5000){
    lastBeat = ms;
    uiReportTiming();
    Serial.printf("[transport] mode %s phase %s BLE mtu %u drops %u internal %u largest %u min %u DMA %u loopStack %u\n",
      transportName(transportMode()),switchName(transportRecord().phase),bleMTU(),unsigned(bleAudioDropped()),
      unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
      unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_free_size(MALLOC_CAP_DMA)),unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    static const esp_partition_t* run = esp_ota_get_running_partition();
    static bool homePrinted = false;
    const int bat = powerBatteryLevel(ms);
    powerReport(ms);
    Serial.printf("[audio] ready %d frames %u dropped %u level %.3f live %d muted %d health %d\n",audioReady(),unsigned(audioFrameCount()),unsigned(audioDropCount()),deviceLevel(),deviceStreaming(),deviceMuted(),deviceHealthy());
    Serial.printf("[hb] up %us bat %d%%%s heap %uKB run %s mood %d still %.1fs\n",
                  (unsigned)(ms / 1000), bat,
                  powerCharging() ? " chg" : "",
                  (unsigned)(ESP.getFreeHeap() / 1024), run->label,
                  (int)petState(), motionStillSec(ms));
    if (motionHomeKnown() && !homePrinted){ homePrinted = true; motionPrintHome(); }
    const MotionDiag g = motionDiag();
    Serial.printf("[imu] en %d upd %u/%u a %.2f %.2f %.2f mag %.2f gRef %.2f dev %.2f still %d home %d up%u sh%u dn%u\n",
                  g.imuEnabled, (unsigned)g.updates, (unsigned)g.reads,
                  g.ax, g.ay, g.az, g.mag, g.gRef, g.dev, g.still, g.home,
                  (unsigned)g.pickups, (unsigned)g.shakes, (unsigned)g.downs);
  }

  static uint32_t last = 0;
  const uint32_t drawMs=millis();
  if (drawMs - last < (uiRotationActive()?20u:33u)) { delay(1); return; }
  last = drawMs;
  uiDraw(drawMs);
  uiUsbDebug();
  delay(1);
}
