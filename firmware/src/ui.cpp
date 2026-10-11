#include "ui.h"
#include "theme.h"
#include "pet.h"
#include "motion.h"
#include "wifi_prov.h"
#include "ota.h"
#include "device.h"
#include "display_motion.h"
#include "landscape_games.h"
#include "audio_input.h"
#include "sound_effects.h"
#include "ui_timing.h"
#include "idle_power.h"
#include "power_status.h"
#include "button_gestures.h"
#include "transport.h"
#include "connection_hint.h"
#include "ble_link.h"
#include "ui_status.h"
#include "clawd_stories.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <stdarg.h>
#include <string.h>
#include <algorithm>

namespace {
void goWake(const char* why);
LGFX_Sprite canvas(&M5.Display);
LGFX_Sprite rotatedCanvas(&M5.Display);
RotationTransition rotation;
uint8_t displayRotation = 0;
void* canvasMemory=nullptr;
void* rotationMemory=nullptr;
LandscapeGames games;
Exploration& exploration=games.exploration;
StarCatch& catchStars=games.catchStars;
LandscapeButton gameButton;
bool landscapeVisible=false;
IdlePowerPolicy idlePower;
bool standby=false,screenOff=false,codecOff=false;
bool wakeAConsumed=false;
ClawdStory::Policy stories;
ClawdStory::Id storyDemoRequest=ClawdStory::Id::None;
uint8_t storyDemoNext=0;
uint32_t storyDemoRequestedAt=0,storyQuietAt=0,storyPokeAt=0,storyPortraitAt=0;
bool storyButton=false,storyMotion=false,storyStrongMotion=false,storyPoked=false,storyPortrait=false;
uint32_t codecAttemptAt=0;
ButtonGestures gestures;
char transportHint[48]={};
uint32_t transportHintUntil=0;
bool s_debug=false;
uint32_t rotationFrames = 0;
struct FrameTiming {
  uint32_t count=0,totalUs=0,maxUs=0,rotatingCount=0,rotatingMaxUs=0;
} frameTiming;
struct FrameTimer {
  uint32_t started=micros();
  bool rotating=rotation.active;
  ~FrameTimer(){
    const uint32_t elapsed=micros()-started;
    frameTiming.count++;frameTiming.totalUs+=elapsed;
    frameTiming.maxUs=std::max(frameTiming.maxUs,elapsed);
    if(rotating){frameTiming.rotatingCount++;frameTiming.rotatingMaxUs=std::max(frameTiming.rotatingMaxUs,elapsed);}
  }
};

int W = 135, H = 240;
constexpr int PAD = 4;
constexpr size_t FrameBytes=135*240*2+2;

uint8_t canvasRotation(uint32_t ms){
  const bool halfway=!rotation.active||uint32_t(ms-rotation.started)>=rotation.duration/2;
  return halfway?rotation.target:displayRotation;
}
void prepareCanvas(uint32_t ms){
  // Reflow halfway through the turn while the image is small. Both buffers
  // reuse their original internal RAM allocations, including odd rotations.
  const bool landscape=canvasRotation(ms)&1;
  const int width=landscape?240:135,height=landscape?135:240;
  if(W!=width){
    W=width;H=height;canvas.setBuffer(canvasMemory,W,H,16);
    canvas.setPivot((W-1)*.5f,(H-1)*.5f);
    Serial.printf("[layout] %s %dx%d\n",landscape?"landscape":"portrait",W,H);
  }
}

void present(uint32_t ms){
  const bool wasActive=rotation.active;
  const uint8_t previousRotation=displayRotation;
  const float angle=rotation.step(ms);
  if(rotation.active && rotatedCanvas.getBuffer()){
    rotatedCanvas.fillSprite(C::screen);
    const float relative=angle-displayRotation*90;
    const int outW=rotatedCanvas.width(),outH=rotatedCanvas.height();
    const float zoom=RotationTransition::fit(relative,W,H,outW,outH);
    canvas.pushRotateZoom(&rotatedCanvas,(outW-1)*.5f,(outH-1)*.5f,relative,zoom,zoom);
    rotatedCanvas.pushSprite(0,0);
    rotationFrames++;
  } else {
    if(displayRotation!=rotation.target){
      displayRotation=rotation.target;
      M5.Display.setRotation(displayRotation);
      if(rotationMemory)rotatedCanvas.setBuffer(rotationMemory,W,H,16);
    }
    canvas.pushSprite(0,0);
  }
  if(wasActive && !rotation.active){
    if((previousRotation&1)&&rotation.target==0&&!s_debug){storyPortrait=true;storyPortraitAt=ms;}
    Serial.printf("[rotation] done target %d elapsed %ums frames %u\n",rotation.target,unsigned(millis()-rotation.started),unsigned(rotationFrames));
  }
}

/* 逐字符字宽表：设备上 textWidth() 不可信，开机把 95 个可打印字符各画一次、
   扫像素量真实宽度，之后所有摆放/截断/右对齐都按这张表算。 */
uint8_t s_w[95];
float   s_wscale = 1.0f;   /* 整体比例校正：字宽表按墨迹量，比真实步进之和偏大 */
int s_glyphH = 13;

/* 日志：只存消息（时间戳占 8 字符，135px 宽放不下），完整版同时打到串口 */
struct LogLine { char m[24]; uint8_t k; };
LogLine logs[16];
int  logCount = 0;
int  logHead  = 0;

char echo[32] = "ready";
uint32_t lastBlink = 0;

// UI only consumes the shared filtered estimate; I2C sampling lives in powerTick.
bool s_chgShown = false;
int s_lvlShown = -1;

void setFontNow(){
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextSize(1);
  canvas.setTextWrap(false,false);
  canvas.setTextDatum(textdatum_t::top_left);
}
/* 字符串真实宽度：字宽表之和 × 比例校正 */
int strW(const char* s){
  int w = 0;
  for (const char* p = s; *p; ++p){
    const int c = (unsigned char)*p - 32;
    w += (c >= 0 && c < 95) ? s_w[c] : s_w['?' - 32];
  }
  return (int)(w * s_wscale + 0.5f);
}

const char* stateName(PetState st){
  switch (st){
    case PET_CURIOUS: return "curious";
    case PET_EXCITED: return "excited";
    case PET_UPSET:   return "upset";
    case PET_CHARGING:return "charging";
    case PET_WAKE:   return "awake";
    case PET_DIZZY:   return "dizzy";
    case PET_SLEEP:   return "sleep";
    default:          return "idle";
  }
}
const char* thought(){
  switch (petState()){
    case PET_CURIOUS: return "investigate";
    case PET_EXCITED: return "challenge on";
    case PET_UPSET:   return "hey, focus";
    case PET_CHARGING:return "energy sync";
    case PET_DIZZY:   return "recalibrating";
    case PET_SLEEP:   return "standby";
    case PET_WAKE:    return "ready to explore";
    default:          return "what's next?";
  }
}

void put(const char* s, int x, int y, uint16_t col){
  setFontNow();
  canvas.setTextColor(col);
  canvas.drawString(s, x, y);
}
/* 超宽就按真实字宽从尾部砍，砍到放得下再加 .. */
void putClip(const char* s, int x, int y, uint16_t col){
  const int maxW = (W - PAD) - x;
  if (strW(s) <= maxW){ put(s, x, y, col); return; }
  char buf[32];
  snprintf(buf, sizeof(buf), "%s", s);
  int n = (int)strlen(buf);
  while (n > 1 && strW(buf) > maxW - strW("..")) buf[--n] = 0;
  snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "..");
  put(buf, x, y, col);
}
void rule(int y){ canvas.fillRect(PAD, y, W - PAD * 2, 1, C::dim2); }
/* 10 段分段条：宽度自适应，右对齐到 W-PAD */
void segBar(int x, int y, int segs, uint16_t on){
  const int avail = (W - PAD) - x;
  const int gap = 2;
  int segW = (avail - 9 * gap) / 10;
  if (segW < 2) segW = 2;
  for (int i = 0; i < 10; i++) canvas.fillRect(x + i * (segW + gap), y, segW, 5, i < segs ? on : C::dim2);
}
int  rowPitch(){ int p = s_glyphH + 2; if (p < 12) p = 12; if (p > 18) p = 18; return p; }
void putCenter(const char* s, int y, uint16_t col){ put(s, (W - strW(s)) / 2, y, col); }

constexpr int BATTERY_W = 46;  // 34px 电池主体 + 电极 + 充电闪电
constexpr int BATTERY_H = 14;
void drawBattery(int x, int y){
  constexpr int bodyW = 34, inset = 2, innerW = bodyW - inset * 2;
  const bool known = s_lvlShown >= 0;
  const int level = std::max(0, std::min(100, s_lvlShown));
  const int fillW = known ? (innerW * level + 99) / 100 : 0;
  canvas.drawRoundRect(x, y, bodyW, BATTERY_H, 2, C::ink);
  canvas.fillRect(x + bodyW + 1, y + 4, 2, 6, C::ink);
  if(fillW)canvas.fillRect(x + inset, y + inset, fillW, BATTERY_H - inset * 2, C::ok);

  // Font0 的 6×8 固定字格可容纳 100%，不依赖主字体的字宽标定。
  char label[5];
  if(known)snprintf(label, sizeof(label), "%d%%", level);
  else snprintf(label, sizeof(label), "--%%");
  const int tx = x + (bodyW - int(strlen(label)) * 6) / 2;
  const int ty = y + (BATTERY_H - 8) / 2;
  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);
  canvas.setTextWrap(false, false);
  canvas.setTextDatum(textdatum_t::top_left);
  canvas.setTextColor(C::ink);
  canvas.drawString(label, tx, ty);
  if(fillW){
    // 绿色区域用深色字，空白区域用亮色字，跨过填充边界仍清晰。
    canvas.setClipRect(x + inset, y + inset, fillW, BATTERY_H - inset * 2);
    canvas.setTextColor(C::screen);
    canvas.drawString(label, tx, ty);
    canvas.clearClipRect();
  }
  if(s_chgShown){
    const int bx = x + 40, by = y + 3;
    canvas.fillTriangle(bx + 3, by, bx, by + 5, bx + 3, by + 5, C::ok);
    canvas.fillTriangle(bx + 2, by + 4, bx + 5, by + 4, bx + 2, by + 9, C::ok);
  }
  setFontNow();
}
void pixelText(const char* s,int x,int y,uint16_t color,int size=1){
  canvas.setFont(&fonts::Font0);canvas.setTextSize(size);canvas.setTextWrap(false,false);
  canvas.setTextDatum(textdatum_t::top_left);canvas.setTextColor(color);canvas.drawString(s,x,y);
}
void drawStatusIcons(int x,int y,uint32_t ms){
  const bool bluetooth=transportMode()==TransportMode::BLE;
  const bool linked=bluetooth?bleConnected():otaConnected();
  const bool joining=bluetooth?bleConnecting():WiFi.status()==WL_IDLE_STATUS;
  UIStatus::drawIcon(canvas,bluetooth?UIStatus::Icon::Bluetooth:UIStatus::Icon::WiFi,x,y,
                    linked?C::ok:joining?C::ink:C::offline,!linked&&!joining);
  const bool mac=deviceControlConnected(ms);
  UIStatus::drawIcon(canvas,UIStatus::Icon::Mac,x+UIStatus::SlotPitch,y,mac?C::ok:C::offline,!mac);
  const auto voice=UIStatus::voiceIcon(deviceVoiceStatus(ms));
  const int vx=x+2*UIStatus::SlotPitch;
  if(voice==UIStatus::VoiceIcon::Wave)UIStatus::drawIcon(canvas,UIStatus::Icon::Wave,vx,y,C::ok);
  else if(voice==UIStatus::VoiceIcon::Muted)UIStatus::drawIcon(canvas,UIStatus::Icon::Muted,vx,y,C::err);
  else if(voice==UIStatus::VoiceIcon::Warning)UIStatus::drawIcon(canvas,UIStatus::Icon::Warning,vx,y,C::warn);
}
const char* connectionCaption(uint32_t ms){
  const auto state=deviceVoiceStatus(ms);
  if(state.muted)return "mic muted";
  if(state.streaming)return deviceHeld()?"key held":"voice on";
  if(!state.connected)return "waiting for Mac";
  if(!state.allowed)return "voice off";
  if(state.sleeping)return "sleep";
  if(!state.calibrated)return "calibration needed";
  if(!state.audioReady)return "audio not ready";
  if(!state.captureReady)return "mic not ready";
  return !strcmp(deviceVoiceBlock(),"rearm")?"flip again to talk":"Mac online";
}
}

void uiBegin(){
  idlePower.activity(millis());
  stories.begin(millis(),esp_random());storyQuietAt=millis();
  Serial.printf("[idle] sleep %us off %us battery-only\n",unsigned(IdlePowerPolicy::SleepMs/1000),unsigned(IdlePowerPolicy::OffMs/1000));
  canvas.setPsram(false);        /* 内部 RAM：PSRAM 上整屏 DMA 推屏会有缓存一致性抖动 */
  canvas.setColorDepth(16);
  canvasMemory=heap_caps_malloc(FrameBytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
  if(canvasMemory)canvas.setBuffer(canvasMemory,W,H,16);
  canvas.setPivot((W-1)*.5f,(H-1)*.5f);
  rotatedCanvas.setPsram(false);
  rotatedCanvas.setColorDepth(16);
  rotationMemory=heap_caps_malloc(FrameBytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
  if(rotationMemory)rotatedCanvas.setBuffer(rotationMemory,W,H,16);
  Serial.printf("[rotation] buffer %s (%d bytes)\n",rotatedCanvas.getBuffer()?"ready":"unavailable; direct rotation",W*H*2);
  canvas.fillSprite(C::screen);
  uiCalibrate();
}
bool uiHealthy(){return canvas.getBuffer()!=nullptr;}
void uiSetOrientation(uint8_t requested,uint32_t ms){
  const bool critical=provActive()||otaActive()||devicePairWindow()||transportBusy()||gestures.switchReady||gestures.comboReady||s_debug;
  if(critical)games.cancel();
  else if(games.active())return;
  if(critical)requested=requested==2?2:0;
  if(!rotation.request(requested,ms))return;
  idlePower.activity(ms);
  rotationFrames=0;
  Serial.printf("[rotation] begin target %u duration %ums\n",requested,unsigned(rotation.duration));
  if(!rotatedCanvas.getBuffer()||critical)rotation.active=false;
}
bool uiRotationActive(){return rotation.active;}
void uiReportTiming(){
  Serial.printf("[render] frames %u mean %uus max %uus rotation_frames %u rotation_max %uus\n",unsigned(frameTiming.count),unsigned(frameTiming.count?frameTiming.totalUs/frameTiming.count:0),unsigned(frameTiming.maxUs),unsigned(frameTiming.rotatingCount),unsigned(frameTiming.rotatingMaxUs));
  frameTiming=FrameTiming{};
  Serial.printf("[idle] age %us standby %d screen_off %d bright %u codec_off %d\n",unsigned(idlePower.elapsed(millis())/1000),standby,screenOff,unsigned(M5.Display.getBrightness()),codecOff);
  Serial.printf("[story] active %s demo %d awake %us next %us budget %u\n",ClawdStory::name(stories.current()),stories.isDemo(),unsigned(stories.awakeMs()/1000),unsigned(stories.opportunityAt()/1000),stories.usedBudget());
}

// Local USB-only idle screenshot, for checking the exact pixels sent to the LCD.
void uiUsbDebug(){
  static char command[16];static unsigned length=0;
  for(unsigned n=0;n<32&&Serial.available();n++){
    const char c=Serial.read();
    if(c=='\r')continue;
    if(c!='\n'){if(length<sizeof(command)-1)command[length++]=c;continue;}
    command[length]=0;length=0;
    if(!strcmp(command,"debug")){if(!games.active()&&!provActive()&&!otaActive()&&!devicePairWindow()&&!deviceStreaming()&&!transportBusy())s_debug=!s_debug;continue;}
    const auto requested=!strncmp(command,"story ",6)?ClawdStory::demoCommand(command+6):ClawdStory::Id::None;
    if(ClawdStory::valid(requested)){
      if(!games.active()&&!provActive()&&!otaActive()&&!devicePairWindow()&&!deviceStreaming()&&!transportBusy()){
        s_debug=true;idlePower.activity(millis());goWake("story demo");storyDemoRequest=requested;storyDemoRequestedAt=millis();
        Serial.printf("[story] preview requested %s\n",ClawdStory::name(storyDemoRequest));
      }else Serial.println("[story] preview unavailable: device busy");
      continue;
    }
    if(!strcmp(command,"story stop")){storyDemoRequest=ClawdStory::Id::None;stories.cancel(esp_random());s_debug=false;continue;}
    if(strcmp(command,"screen"))continue;
    if(otaActive()||devicePairWindow()||transportBusy()||deviceStreaming()||rotation.active||!uiHealthy()){Serial.println("TCFB unavailable: idle stable screen only");continue;}
    // Serial frame export can take longer than the local capture queue. Do not
    // export in a live session; pause only local capture until the next tick.
    audioTick(false);
    Serial.printf("\nTCFB %d %d RGB565LE %d\n",W,H,W*H*2);
    uint8_t row[240*2];
    for(int y=0;y<H;y++){
      for(int x=0;x<W;x++){const uint16_t pixel=canvas.readPixel(x,y);row[x*2]=pixel&255;row[x*2+1]=pixel>>8;}
      Serial.write(row,W*2);
    }
    Serial.println("\nTCFB-END");
  }
}

/* 标定：逐字符画一遍、扫像素量真实宽度；再用 Mg 量字形高 */
void uiCalibrate(){
  constexpr int BOX_W = 40, BOX_H = 22;
  for (int c = 0; c < 95; c++){
    char ch[2] = { (char)(32 + c), 0 };
    canvas.fillRect(0, 0, BOX_W, BOX_H, C::screen);
    setFontNow();
    canvas.setTextColor(C::ink);
    canvas.drawString(ch, 0, 3);

    int maxX = -1;
    for (int y = 0; y < BOX_H; y++)
      for (int x = 0; x < BOX_W; x++)
        if (canvas.readPixel(x, y) != C::screen && x > maxX) maxX = x;

    /* 空格没有墨迹，给一个常规值 */
    s_w[c] = (ch[0] == ' ') ? 5 : (uint8_t)(maxX < 0 ? 5 : maxX + 2);
  }

  /* 字形高：用带升部与降部的 Mg 量 */
  canvas.fillRect(0, 0, BOX_W, BOX_H, C::screen);
  setFontNow();
  canvas.setTextColor(C::ink);
  canvas.drawString("Mg", 0, 3);
  int maxY = 0;
  for (int y = 0; y < BOX_H; y++)
    for (int x = 0; x < BOX_W; x++)
      if (canvas.readPixel(x, y) != C::screen && y > maxY) maxY = y;
  s_glyphH = maxY + 1;
  if (s_glyphH < 8) s_glyphH = 8;

  /* 比例校正：字宽表按「墨迹 + 内边距」量，字符步进里还有侧边距，
     直接用会系统性偏大（状态栏右对齐就会压到左边文字）。用一条代表性
     字符串量真实宽度，与表之和求比值，之后所有宽度都乘它。 */
  const char* ref = "BAT 97% up 00:42";
  int tableSum = 0;
  for (const char* p = ref; *p; ++p) tableSum += s_w[(unsigned char)*p - 32];
  canvas.fillRect(0, 0, W, BOX_H, C::screen);
  setFontNow();
  canvas.setTextColor(C::ink);
  canvas.drawString(ref, 0, 3);
  int refMaxX = -1;
  for (int y = 0; y < BOX_H; y++)
    for (int x = 0; x < W; x++)
      if (canvas.readPixel(x, y) != C::screen && x > refMaxX) refMaxX = x;
  if (tableSum > 0 && refMaxX > 0){
    s_wscale = (float)(refMaxX + 1) / (float)tableSum;
    if (s_wscale < 0.6f) s_wscale = 0.6f;
    if (s_wscale > 1.2f) s_wscale = 1.2f;
  }

  canvas.fillRect(0, 0, W, BOX_H, C::screen);
}

void uiDumpMetrics(){
  Serial.printf("[ui] display %dx%d rot=%d | fontHeight %d textWidth(M) %d\n",
                (int)M5.Display.width(), (int)M5.Display.height(),
                (int)M5.Display.getRotation(), canvas.fontHeight(), canvas.textWidth("M"));
  Serial.printf("[ui] glyphH %d wscale %.3f | strW BAT %d up %d ENERGY %d\n",
                s_glyphH, s_wscale, strW("BAT 97%"), strW("up 00:42"), strW("ENERGY"));
}

void uiLog(const char* fmt, ...){
  char buf[24];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  /* 屏幕只留短消息；带时间戳的完整版照常打到串口 */
  const uint32_t s = millis() / 1000;
  Serial.printf("[%02u:%02u] %s\n", (unsigned)(s / 60), (unsigned)(s % 60), buf);

  LogLine& l = logs[logHead];
  logHead = (logHead + 1) % 16;
  if (logCount < 16) logCount++;
  snprintf(l.m, sizeof(l.m), "%s", buf);
  l.k = strstr(buf, "ok") ? 1 : 0;
}

void uiPoke(){ petPoke();soundRequest(SoundCue::Poke); }

namespace {
uint32_t s_stateAt = 0;          /* 当前状态进入时刻，用于状态自然回落 */
constexpr uint8_t BRIGHT_ASLEEP = 12;

/* 统一的换状态入口：记录时刻 + 串口留痕（调试用） */
void setPet(PetState st, const char* why){
  if (petState() == st) return;
  petSetState(st);
  s_stateAt = millis();
  if(st==PET_WAKE)soundRequest(SoundCue::Wake);
  else if(st==PET_SLEEP)soundRequest(SoundCue::Sleep);
  else if(st==PET_DIZZY)soundRequest(SoundCue::Dizzy);
  else if(st==PET_CURIOUS&&!strcmp(why,"sound"))soundRequest(SoundCue::Curious);
  Serial.printf("[pet] -> %s (%s)\n", stateName(st), why);
}
void goSleep(const char* why){
  if (petState() == PET_SLEEP) return;
  setPet(PET_SLEEP, why);
  M5.Display.setBrightness(BRIGHT_ASLEEP);
  snprintf(echo, sizeof(echo), "sleep");
  uiLog("sleep: %s", why);
}
void goWake(const char* why){
  standby=false;
  if (petState() != PET_SLEEP) return;
  idlePower.activity(millis());
  M5.Display.setBrightness(deviceBrightness());
  setPet(PET_WAKE, why);
  snprintf(echo, sizeof(echo), "wake");
  uiLog("wake: %s", why);
}

/* 配网期间：屏幕交给 PROV 页，小爪只摆姿态，不吃主界面的按键/睡眠逻辑 */
void tickProv(){
  const ProvStage st = provStage();
  petSetState(st == PROV_OK ? PET_EXCITED : st == PROV_FAIL ? PET_UPSET : PET_CURIOUS);
  if (st == PROV_FAIL && M5.BtnA.wasClicked()) provRetry();
}

/* OTA 期间：A 确认 / B 取消，失败后 B 回主界面；不睡不切页 */
void tickOta(){
  const OtaStage st = otaStage();
  petSetState(st == OTA_DONE ? PET_EXCITED : st == OTA_FAIL ? PET_UPSET : PET_IDLE);
  if (st == OTA_CONFIRM){
    if (M5.BtnA.wasClicked()) otaConfirm();
    if (M5.BtnB.wasClicked()) otaCancel();
  } else if (st == OTA_READY || st == OTA_RECV){
    if (M5.BtnB.wasClicked()) otaCancel();
  } else if (st == OTA_FAIL){
    if (M5.BtnB.wasClicked()) otaDismiss();
  }
}
}

bool uiButtonAForVoice(){return M5.BtnA.isPressed()&&!wakeAConsumed&&!gestures.suppressA&&!games.active()&&!gameButton.blocked;}
bool uiLandscapeActive(){return games.active();}

void uiWakeForVoice(){ idlePower.activity(millis());goWake("Mac voice start"); }

void uiTick(uint32_t ms){
  if(deviceStreaming())s_debug=false;
  s_lvlShown = powerBatteryLevel(ms);
  s_chgShown = powerCharging();
  // Keep sensor acquisition and diagnostics alive while setup/OTA own the UI.
  const MotionEv ev = motionPoll(ms);
  if(!M5.BtnA.isPressed()&&!M5.BtnA.wasClicked())wakeAConsumed=false;
  if(petState()==PET_SLEEP&&M5.BtnA.isPressed())wakeAConsumed=true;
  const bool button=M5.BtnA.isPressed()||M5.BtnB.isPressed()||M5.BtnA.wasClicked()||M5.BtnB.wasClicked()||M5.BtnPWR.wasClicked()||M5.BtnPWR.wasHold();
  storyButton=button;storyMotion=motionActivityDetected()||ev!=MEV_NONE;
  storyStrongMotion=ev==MEV_SHAKE||ev==MEV_FACE_DOWN;
  if(button||motionActivityDetected()){
    idlePower.activity(ms);
    standby=false;
    if(petState()==PET_SLEEP&&(button||devicePose()!=DevicePose::FaceDown))goWake(button?"button":"motion");
  }
  const bool special=provActive()||otaActive()||devicePairWindow()||transportBusy()||gestures.switchReady||gestures.comboReady||s_debug;
  if(special)games.cancel();

  /* 配网 / OTA 期间接管屏幕与按键，暂停小爪的日常循环 */
  if (provActive()){ gameButton.update(ms,M5.BtnA.isPressed(),M5.BtnA.wasClicked(),false,false,true);gestures.update(ms,M5.BtnA.isPressed(),M5.BtnB.isPressed(),false,false,false);tickProv(); return; }
  if (otaActive()){ gameButton.update(ms,M5.BtnA.isPressed(),M5.BtnA.wasClicked(),false,false,true);gestures.update(ms,M5.BtnA.isPressed(),M5.BtnB.isPressed(),false,false,false);tickOta(); return; }

  const auto gesture=gestures.update(ms,M5.BtnA.isPressed(),M5.BtnB.isPressed(),M5.BtnB.wasClicked(),!transportBusy(),!deviceStreaming());
  // A alone in idle mode opens diagnostics; B/combo and live shortcuts retain priority.
  static bool debugHeld=false,debugBlocked=false;
  if(!M5.BtnA.isPressed()&&!M5.BtnA.wasClicked()){debugHeld=false;debugBlocked=false;}
  const bool deck=(deviceDisplayRotation()&1)&&!devicePairWindow();
  const auto gameAction=gameButton.update(ms,M5.BtnA.isPressed(),M5.BtnA.wasClicked(),games.active(),
    deck&&!uiRotationActive()&&!special&&!debugHeld,gestures.suppressA||devicePairWindow()||transportBusy());
  if(M5.BtnA.isPressed()&&(gestures.suppressA||deviceStreaming()||devicePairWindow()||transportBusy()))debugBlocked=true;
  if(M5.BtnA.isPressed()&&M5.BtnA.pressedFor(1200)&&!debugHeld&&!debugBlocked&&!games.active()&&!gameButton.blocked){
    debugHeld=true;s_debug=!s_debug;goWake("diagnostics");idlePower.activity(ms);
  }
  if(gesture==ButtonGestures::Action::Mute){deviceToggleMute();uiLog(deviceMuted()?"mic muted":"mic unmuted");}
  if(gesture==ButtonGestures::Action::Pair){deviceOpenPairing();goWake("pairing");uiLog("pair via %s",transportName(transportMode()));}
  if(gesture==ButtonGestures::Action::Switch){
    char id[37];snprintf(id,sizeof(id),"button-%08x-%08x",unsigned(esp_random()),unsigned(ms));
    const auto target=transportMode()==TransportMode::BLE?TransportMode::WiFi:TransportMode::BLE;
    const char* error=requestTransportSwitch(target,"button",id);
    if(error){snprintf(transportHint,sizeof(transportHint),"switch: %s",error);transportHintUntil=ms+3000;uiLog("%s",transportHint);}
    goWake("transport");
  }
  if(devicePairWindow()||transportBusy()||gestures.switchReady||gestures.comboReady)games.cancel();
  if(deck&&!landscapeVisible)goWake("landscape");
  if(!deck)games.cancel();
  landscapeVisible=deck;
  if(deck&&!special){
    const MotionDiag input=motionDiag();
    const float tilt=LandscapeTilt::tiltDegrees(input.ax,input.ay,deviceDisplayRotation());
    const bool catching=LandscapeGames::scene(deviceDisplayRotation())==LandscapeGames::Scene::Catch;
    if(gameAction==LandscapeButton::Action::Exit){
      games.cancel();idlePower.activity(ms);uiLog(catching?"catch cancelled":"explore cancelled");
    }
    if(gameAction==LandscapeButton::Action::Enter&&!wakeAConsumed&&!debugHeld&&!s_debug){
      if(petState()==PET_SLEEP)goWake("game A");
      else if(games.begin(deviceDisplayRotation(),tilt,ms)){
        idlePower.activity(ms);uiLog(catching?"catch start":"explore start");
        if(catching)Serial.printf("[catch] begin rot %u base %.1f round %u duration %ums\n",deviceDisplayRotation(),tilt,catchStars.rounds,unsigned(StarCatch::RoundMs));
        else Serial.printf("[explore] begin rot %u base %.1f target %.1f\n",deviceDisplayRotation(),tilt,exploration.targetX());
      }
    }
    const auto event=games.update(deviceDisplayRotation(),tilt,petState()!=PET_SLEEP,ms);
    if(event.explore==Exploration::Event::Locked){
      idlePower.activity(ms);petPoke();soundRequest(SoundCue::Scan);s_stateAt=ms;uiLog("target locked");
    }else if(event.explore==Exploration::Event::Found){
      idlePower.activity(ms);petBlinkNow();soundRequest(SoundCue::Discovery);uiLog("discovery %u",exploration.discoveries);
    }
    if(event.catchStars.caught){
      idlePower.activity(ms);petPoke();s_stateAt=ms;soundRequest(SoundCue::Poke);
      Serial.printf("[catch] caught %u combo %u x %.1f\n",catchStars.score,catchStars.combo,catchStars.hitX);
    }
    if(event.catchStars.missed){petBlinkNow();Serial.printf("[catch] missed score %u combo %u\n",catchStars.score,catchStars.combo);}
    if(event.catchStars.finished){
      idlePower.activity(ms);petBlinkNow();soundRequest(SoundCue::Discovery);
      uiLog("catch done %u",catchStars.score);Serial.printf("[catch] finished score %u best %u elapsed %ums\n",catchStars.score,catchStars.bestCombo,unsigned(ms-catchStars.started));
    }
  }

  /* ---- 姿态输入 ---- */
  if (ev == MEV_SHAKE && !games.active()){
    if (petState() != PET_SLEEP){
      setPet(PET_DIZZY, "shake");
      snprintf(echo, sizeof(echo), "shaken");
      uiLog("shaken");
    }
  } else if (ev == MEV_PICKUP){
    if (petState() == PET_SLEEP) goWake("picked up");
    else if (petState() == PET_IDLE) setPet(PET_CURIOUS, "moved");
  } else if (ev == MEV_FACE_DOWN && !games.active() && !button){
    goSleep("face down");
  }

  /* ---- 按键 ---- */
  // Motion wakes a new inversion above; a stationary inverted device must not
  // undo automatic standby on every tick when voice input is disabled.
  if (M5.BtnA.wasClicked() && !wakeAConsumed && !gestures.suppressA && !gameButton.blocked && !debugHeld && !s_debug && !deviceStreaming() && !deck){
    idlePower.activity(ms);
    if (petState() == PET_SLEEP) goWake("btn.A");
    else { petPoke();soundRequest(SoundCue::Poke); s_stateAt = ms; storyPoked=true;storyPokeAt=ms;snprintf(echo, sizeof(echo), "poke"); uiLog("poke"); }
  }
  if(s_debug&&M5.BtnA.wasClicked()&&!debugHeld&&!gestures.suppressA&&!deviceStreaming()&&!transportBusy()){
    storyDemoRequest=ClawdStory::Id(storyDemoNext);storyDemoRequestedAt=ms;
    storyDemoNext=uint8_t(ClawdStory::demoNext(storyDemoRequest));
  }

  /* ---- 状态自然回落 ---- */
  const uint32_t inState = petStateElapsed(ms,s_stateAt);
  switch (petState()){
    case PET_CURIOUS: if (inState > 3000) setPet(PET_IDLE, "timeout"); break;
    case PET_EXCITED: if (inState > 2600) setPet(PET_IDLE, "timeout"); break;
    case PET_UPSET:   if (inState > 3000) setPet(PET_IDLE, "timeout"); break;
    case PET_WAKE:    if (inState > 3200) setPet(PET_IDLE, "timeout"); break;
    case PET_DIZZY:   if (inState > 3500) setPet(PET_IDLE, "recovered"); break;
    default: break;
  }
  if (petState() == PET_EXCITED && inState > 2600) setPet(PET_IDLE, "timeout");

  /* ---- Clawd 的视觉睡眠先出现；息屏待机由独立的 5 分钟期限控制 ---- */
  if (!games.active() && petState() != PET_SLEEP && petIdleSleepDue(ms,idlePower.lastActivity,motionStillSec(ms),deviceStreaming(),deviceInverted(),devicePairWindow())) goSleep("still 30s");

  /* BtnA 长按：切到诊断页（IMU 数据与事件计数），再长按回主界面 */
  if (petState()==PET_IDLE && s_chgShown && !deviceStreaming())setPet(PET_CHARGING,"charging");
  if (petState()==PET_CHARGING && !s_chgShown)setPet(PET_IDLE,"charged");
  static uint32_t lastSound=0;
  if (deviceLocalSound() && deviceLevel()>.55f && ms-lastSound>3000 && petState()==PET_IDLE){lastSound=ms;setPet(PET_CURIOUS,"sound");}

  if (petState() == PET_IDLE && ms - lastBlink > 3200){ petBlinkNow(); lastBlink = ms; }
}

bool uiScreenOff(){return screenOff;}
void uiStoriesTick(uint32_t ms){
  static bool known=false,lastCharging=false,lastConnected=false,lastTakeover=false;
  static int lastPowerSource=-1;
  static PetState lastState=PET_IDLE;
  const bool connected=deviceControlConnected(ms);
  const int powerSource=powerInputSource(ms);
  const bool changing=known&&(lastCharging!=s_chgShown||lastState!=petState());
  const bool connectionChanged=known&&lastConnected!=connected;
  const bool disconnected=known&&lastConnected&&!connected;
  const bool busy=provActive()||otaActive()||transportBusy()||devicePairWindow()||games.active()||
    deviceStreaming()||deviceHeld()||uiRotationActive()||gestures.switchReady||gestures.comboReady||
    (transportHintUntil&&int32_t(transportHintUntil-ms)>0);
  const bool takeover=busy||s_debug;
  if(!known||changing||connectionChanged||storyButton||storyMotion||takeover||lastTakeover!=takeover)storyQuietAt=ms;
  ClawdStory::Inputs in;
  const bool portrait=W==135&&displayRotation==0&&rotation.target==0&&!rotation.active;
  in.awake=petState()!=PET_SLEEP&&!screenOff;
  in.home=portrait&&!s_debug&&(petState()==PET_IDLE||petState()==PET_CHARGING);
  in.takeover=takeover;
  in.motionHealthy=motionActivityHealthy(ms);
  in.interrupt=storyButton||storyMotion||changing||disconnected;
  // A diagnostic clip survives charger termination pulses at full battery;
  // unplugging/changing input or losing trustworthy power data still cancels.
  in.manualInterrupt=storyButton||storyStrongMotion||disconnected||(known&&lastPowerSource!=powerSource);
  in.manualSafe=s_debug&&portrait&&in.awake&&uiHealthy()&&!busy&&!deviceInverted()&&in.motionHealthy&&powerSource>=0;
  in.battery=powerBatteryLevel(ms);in.charging=s_chgShown;
  in.quietMs=petStateElapsed(ms,storyQuietAt);
  in.stableMs=uint32_t(std::max(0.f,motionStillSec(ms))*1000);
  in.sleepRemainingMs=30000-std::min(30000u,idlePower.elapsed(ms));
  in.preferRepair=storyPoked&&petStateElapsed(ms,storyPokeAt)<90000;
  in.preferGravity=storyPortrait&&petStateElapsed(ms,storyPortraitAt)<90000;
  if(!in.awake||takeover){storyPoked=false;storyPortrait=false;}
  const auto previous=stories.current();const bool wasDemo=stories.isDemo();
  const auto event=stories.update(ms,in,esp_random());
  if(event==ClawdStory::Event::Started)Serial.printf("[story] start %s auto\n",ClawdStory::name(stories.current()));
  else if(event!=ClawdStory::Event::None)Serial.printf("[story] %s %s %s\n",event==ClawdStory::Event::Finished?"finish":"cancel",ClawdStory::name(previous),wasDemo?"demo":"auto");
  if(ClawdStory::valid(storyDemoRequest)){
    if(petStateElapsed(ms,storyDemoRequestedAt)>2000||busy||!s_debug){Serial.println("[story] preview rejected");storyDemoRequest=ClawdStory::Id::None;}
    else if(in.manualSafe&&stories.startDemo(storyDemoRequest,ms,in,esp_random())){
      Serial.printf("[story] start %s demo\n",ClawdStory::name(storyDemoRequest));storyDemoRequest=ClawdStory::Id::None;
    }
  }
  known=true;lastCharging=s_chgShown;lastConnected=connected;lastState=petState();lastTakeover=takeover;lastPowerSource=powerSource;
}
void uiPowerTick(uint32_t ms,bool bootVerified){
  const bool busy=provActive()||otaActive()||transportBusy()||devicePairWindow()||
    games.active()||deviceStreaming()||s_debug||uiRotationActive()||gestures.switchReady||gestures.comboReady;
  // Broken/stale IMU input cannot be interpreted as prolonged inactivity.
  const bool blocked=busy||!motionActivityHealthy(ms);
  const auto action=idlePower.update(ms,blocked,standby,powerAutoOffAllowed(ms));
  if(blocked)standby=false;
  if(bootVerified&&!soundPlaying()&&action==IdlePowerPolicy::Action::Sleep){
    goSleep("idle standby 5m");standby=true;
    Serial.printf("[idle] standby after %us inactive\n",unsigned(idlePower.elapsed(ms)/1000));
  }
  if(bootVerified&&!soundPlaying()&&action==IdlePowerPolicy::Action::PowerOff){
    // OTA begin shares this admission lock. Recheck after acquiring it so an
    // upload arriving at the deadline cannot race a physical power cut.
    std::unique_lock<std::mutex> admission(transportAdmission(),std::try_to_lock);
    if(admission.owns_lock()&&!otaActive()&&!provActive()&&!transportBusy()&&powerConfirmAutoOff()){
      Serial.printf("[idle] power-off after %us inactive\n",unsigned(idlePower.elapsed(ms)/1000));
      deviceStop();soundCancel();audioTick(false);M5.Speaker.end();
      M5.Display.setBrightness(0);
      for(unsigned attempt=0;attempt<3;attempt++){
        if(M5.Power.M5pm1.powerOff())break;
        delay(10);
      }
      // If I2C/power-off fails or power remains on, stay responsive and grant
      // a fresh timeout rather than entering deep sleep with no wake source.
      idlePower.activity(millis());
      Serial.println("[idle] power-off returned; remaining in standby");
    }
  }
  const bool settled=petState()==PET_SLEEP&&!busy&&petStateElapsed(ms,s_stateAt)>=1200;
  const bool blank=standby&&settled;
  const uint8_t brightness=blank?0:petState()==PET_SLEEP&&!busy?BRIGHT_ASLEEP:deviceBrightness();
  if(M5.Display.getBrightness()!=brightness)M5.Display.setBrightness(brightness);
  if(blank!=screenOff){screenOff=blank;Serial.printf("[idle] screen %s inactive %us\n",screenOff?"off":"on",unsigned(idlePower.elapsed(ms)/1000));}
  if(!settled||soundPlaying())codecOff=false;
  else if(!codecOff&&uint32_t(ms-codecAttemptAt)>=1000){
    codecAttemptAt=ms;codecOff=audioPowerDown();
    Serial.printf("[idle] codec power-down %s\n",codecOff?"ok":"retry");
  }
}

/* 诊断页：IMU 链路是否通、原始三轴、静止判定、事件计数 */
void renderDebug(uint32_t ms){
  const MotionDiag g = motionDiag();
  canvas.fillSprite(C::screen);
  pixelText("DIAGNOSTICS",PAD,4,C::warn);rule(16);
  int y=22;
  char buf[40];
  auto line=[&](const char* s,uint16_t color=C::ink){char visible[22];snprintf(visible,sizeof(visible),"%.21s",s);pixelText(visible,PAD,y,color);y+=12;};
  snprintf(buf,sizeof(buf),"link %s / Mac %s",transportName(transportMode()),deviceControlConnected(ms)?"on":"off");line(buf);
  if(transportMode()==TransportMode::BLE){snprintf(buf,sizeof(buf),"MTU %u drops %u",bleMTU(),unsigned(bleAudioDropped()));line(buf);}
  else{snprintf(buf,sizeof(buf),"IP %s",WiFi.localIP().toString().c_str());line(buf);}
  snprintf(buf,sizeof(buf),"imu %s upd %u",g.imuEnabled?"on":"OFF",unsigned(g.updates));line(buf);
  snprintf(buf,sizeof(buf),"a %.2f %.2f %.2f",g.ax,g.ay,g.az);line(buf);
  snprintf(buf,sizeof(buf),"gRef %.2f dev %.2f",g.gRef,g.dev);line(buf);
  snprintf(buf,sizeof(buf),"still %.1fs home %s",motionStillSec(ms),g.home?"set":"wait");line(buf);
  snprintf(buf,sizeof(buf),"up%u sh%u dn%u",unsigned(g.pickups),unsigned(g.shakes),unsigned(g.downs));line(buf);
  line(connectionCaption(ms),C::dim);
  rule(y);y+=6;
  const int from=logCount>3?logCount-3:0;
  for(int i=from;i<logCount;i++){
    const int oldest=(logHead-logCount+16)%16;
    snprintf(buf,sizeof(buf),"%.21s",logs[(oldest+i)%16].m);line(buf,i==logCount-1?C::glow:C::dim);
  }
  snprintf(buf,sizeof(buf),"A:story %u/%u",unsigned(storyDemoNext+1),ClawdStory::Count);pixelText(buf,PAD,H-24,C::ink);
  pixelText("hold A: back",PAD,H-12,C::dim);
  present(ms);
}

/* Fixed 135x240 setup layout. Critical strings are complete, never ellipsized. */
void renderProv(uint32_t ms){
  canvas.fillSprite(C::screen);
  canvas.setTextWrap(false,false);
  canvas.setTextDatum(textdatum_t::top_left);
  int rightmost=0,bottommost=0;
  auto line=[&](const char* text,int y,const lgfx::IFont* font,float size,uint16_t color,bool centered=true){
    canvas.setFont(font);canvas.setTextSize(size);
    int width=canvas.textWidth(text);
    if(width>W-2*PAD){canvas.setTextSize(size*float(W-2*PAD)/width);width=canvas.textWidth(text);}
    const int x=centered?(W-width)/2:PAD;
    rightmost=std::max(rightmost,x+width);bottommost=std::max(bottommost,y+int(canvas.fontHeight()));
    canvas.setTextColor(color);canvas.drawString(text,x,y);
  };
  line("WIFI SETUP",5,&fonts::Font2,1,C::ink);
  rule(27);
  line("1 CONNECT HOTSPOT",36,&fonts::Font0,1,C::dim);
  line(provApSsid(),50,&fonts::Font0,1,C::ink,false);
  line("PASSWORD",69,&fonts::Font0,1,C::dim);
  line(provApPass(),82,&fonts::Font2,1.5f,C::glow);
  rule(115);

  const ProvStage st = provStage();
  petSetScale(.8f);petDraw(&canvas,124,ms);
  if (st == PROV_WAIT || st == PROV_VERIFY){
    const int phase=(ms/400)%3;
    for(int i=0;i<phase;i++){
      const int x=104+i*5;
      canvas.drawLine(x,133-i*3,x+2,135-i*3,C::dim);
      canvas.drawLine(x+2,135-i*3,x+2,139+i*3,C::dim);
      canvas.drawLine(x+2,139+i*3,x,141+i*3,C::dim);
    }
  }
  line("2 OPEN BROWSER",180,&fonts::Font0,1,C::dim);
  line("192.168.4.1",194,&fonts::Font2,1,C::ink);
  char status[24];
  switch (st){
    case PROV_AP: snprintf(status,sizeof(status),"Starting hotspot");break;
    case PROV_VERIFY:snprintf(status,sizeof(status),"Connecting %d/%d",provTries(),PROV_MAX_TRIES);break;
    case PROV_OK:snprintf(status,sizeof(status),"Saved. Restarting");break;
    case PROV_FAIL:snprintf(status,sizeof(status),"Failed. Retry: A");break;
    default:snprintf(status,sizeof(status),"Waiting for Wi-Fi");break;
  }
  line(status,223,&fonts::Font0,1,st==PROV_FAIL?C::err:C::dim);
  static bool reported=false;
  if(!reported){Serial.printf("[prov-ui] text bounds right=%d bottom=%d viewport=%dx%d\n",rightmost,bottommost,W,H);reported=true;}
  setFontNow();
  present(ms);
}

/* OTA forces the portrait viewport. Fixed Font0 cells and an anchored footer
   keep every stage inside 135x240, independent of the calibrated UI font. */
void renderOta(uint32_t ms){
  canvas.fillSprite(C::screen);
  constexpr int margin=8;
  auto line=[&](const char* text,int y,uint16_t color,int size=1){
    const int maxChars=(W-2*margin)/(6*size);
    char label[40];snprintf(label,sizeof(label),"%s",text);
    int length=strlen(label);
    if(length>maxChars){length=maxChars;label[length]=0;memcpy(label+length-3,"...",3);}
    pixelText(label,(W-length*6*size)/2,y,color,size);
  };
  line("FIRMWARE",8,C::ink,2);
  const char* version=otaTargetVer();
  line(version[0]?version:"-",39,C::dim);
  canvas.drawFastHLine(margin,55,W-2*margin,C::dim2);
  const OtaStage st = otaStage();
  const int progress=std::max(0,std::min(100,otaPct()));
  const char* name="CONFIRM",*hint="Confirm on device",*footer="A: YES   B: NO";
  uint16_t color=C::glow;
  switch (st){
    case OTA_READY: name="WAITING";hint="Waiting for Mac";footer="B: CANCEL";break;
    case OTA_RECV: name="RECEIVING";hint="Keep power on";footer="B: CANCEL";break;
    case OTA_VERIFY: name="VERIFYING";hint="Checking image";footer="PLEASE WAIT";break;
    case OTA_DONE: name="COMPLETE";hint="Restarting...";footer="UPDATE COMPLETE";color=C::ok;break;
    case OTA_FAIL: name="FAILED";hint="Try again on Mac";footer="B: BACK";color=C::err;break;
    default: break;
  }
  line(name,73,color,2);
  if(st==OTA_FAIL){
    // Show the whole bounded error, including long underscored reasons.
    char error[40];snprintf(error,sizeof(error),"%s",otaLastErr());
    if(!error[0])snprintf(error,sizeof(error),"Upload failed");
    for(char* p=error;*p;p++)if(*p=='_')*p=' ';
    const int columns=(W-2*margin)/6;
    const int length=strlen(error);
    for(int offset=0,row=0;offset<length;offset+=columns,row++){
      char part[40];const int count=std::min(columns,length-offset);
      memcpy(part,error+offset,count);part[count]=0;line(part,108+row*14,C::err);
    }
  }else{
    char value[8];
    if(st==OTA_CONFIRM){
      snprintf(value,sizeof(value),"%ds",std::max(0,std::min(30,otaConfirmLeftSec())));
      line("Time remaining",154,C::dim);
    }else{
      snprintf(value,sizeof(value),"%d%%",progress);
      canvas.drawRect(margin,153,W-2*margin,10,C::dim2);
      const int fill=(W-2*margin-4)*progress/100;
      if(fill)canvas.fillRect(margin+2,155,fill,6,color);
    }
    line(value,106,color,4);
  }
  line(hint,183,C::dim);
  canvas.drawFastHLine(margin,H-32,W-2*margin,C::dim2);
  line(footer,H-18,st==OTA_DONE?C::ok:C::ink);
  setFontNow();
  present(ms);
}

// A landscape scene, rather than a rotated portrait dashboard. Everything in
// the play field is local fiction; battery and status icons remain real state.
void renderExploration(uint32_t ms){
  canvas.fillSprite(C::screen);
  auto text=[&](const char* s,int x,int y,uint16_t color){
    canvas.setFont(&fonts::Font0);canvas.setTextSize(1);canvas.setTextWrap(false,false);
    canvas.setTextDatum(textdatum_t::top_left);canvas.setTextColor(color);canvas.drawString(s,x,y);
  };
  text("CLAWD // DECK",8,6,C::ink);
  drawStatusIcons(W-8-BATTERY_W-6-UIStatus::Width,4,ms);
  drawBattery(W - 8 - BATTERY_W, 3);
  canvas.drawFastHLine(8,19,224,C::dim2);
  canvas.drawFastVLine(119,26,88,C::dim2);
  const bool asleep=petState()==PET_SLEEP;
  text(stateName(petState()),10,26,C::dim);
  for(int i=0;i<5;i++)canvas.fillRect(15+i*20,111,8,1,C::dim2);
  petSetScale(1.35f);petDraw(&canvas,38,ms,deviceStreaming()?2:10,exploration.offset);

  text(asleep?"REST / A:WAKE":exploration.scanning?"LOCKED / SCAN":exploration.active?"AIM / ROT LOCK":exploration.revealed?"DISCOVERY":"A:EXPLORE",130,26,exploration.scanning?C::ok:C::glow);
  for(int x=136;x<232;x+=12)for(int y=43;y<100;y+=12)canvas.drawPixel(x,y,C::dim2);
  const float progress=exploration.progress(ms);
  const int px=lroundf(exploration.probeX());
  const int tx=lroundf(exploration.targetX());
  const float sound=!asleep&&deviceLocalSound()?deviceLevel():0;
  const int py=Exploration::AimY+(asleep||exploration.active?0:lroundf(sinf(ms/390.f)*2-sound*3));
  canvas.setClipRect(129,39,104,60);
  if(exploration.active){
    const int ty=Exploration::AimY;
    canvas.drawCircle(tx,ty,4,exploration.near?C::glow:C::dim);
    canvas.drawFastHLine(tx-7,ty,3,C::dim);canvas.drawFastHLine(tx+5,ty,3,C::dim);
    if(exploration.scanning)canvas.drawRect(tx-10,ty-10,21,21,C::ok);
  }
  if(!asleep&&(!exploration.revealed||exploration.active)){
    // Suspended probe: two detached particles keep a clear gap to the pet.
    canvas.fillRect(px-15,py+5,2,2,C::lite);
    canvas.fillRect(px-12,py+15,2,2,C::lite);
    canvas.drawLine(px,py-8,px+8,py,C::ink);canvas.drawLine(px+8,py,px,py+8,C::ink);
    canvas.drawLine(px,py+8,px-8,py,C::ink);canvas.drawLine(px-8,py,px,py-8,C::ink);
    canvas.fillRect(px-2,py-2,5,5,exploration.scanning?C::ok:C::glow);
    if(exploration.scanning){
      const int radius=10+int(progress*23);
      canvas.drawCircle(px,py,radius,C::dim);
    }
  }
  const char* names[]={"CHIP","CRYSTAL","ORBIT","SIGNAL"};
  if(exploration.revealed){
    const int x=tx,y=Exploration::AimY;
    switch(exploration.artifact()){
      case 0:
        canvas.drawRect(x-7,y-7,14,14,C::ok);canvas.fillRect(x-3,y-3,6,6,C::dim);
        for(int i=-4;i<=4;i+=4){canvas.drawFastVLine(x+i,y-10,3,C::ok);canvas.drawFastVLine(x+i,y+7,3,C::ok);}break;
      case 1:
        canvas.drawLine(x,y-10,x+7,y,C::lite);canvas.drawLine(x+7,y,x,y+9,C::lite);
        canvas.drawLine(x,y+9,x-7,y,C::lite);canvas.drawLine(x-7,y,x,y-10,C::lite);canvas.drawFastVLine(x,y-7,14,C::body);break;
      case 2:
        canvas.drawCircle(x,y,6,C::ink);canvas.drawEllipse(x,y,13,4,C::lite);canvas.fillRect(x+9,y-5,3,3,C::ok);break;
      default:
        for(int i=0;i<4;i++)canvas.fillRect(x-10+i*6,y+7-i*5,3,3+i*5,C::ok);break;
    }
  }
  canvas.clearClipRect();
  if(exploration.scanning){canvas.fillRect(130,111,100,2,C::dim2);canvas.fillRect(130,111,int(progress*100),2,C::ok);}
  text(asleep?"probe parked":exploration.scanning?"scanning...":exploration.active?(exploration.near?"hold to lock":"tilt to aim"):exploration.revealed?names[exploration.artifact()]:"A to start",130,101,C::dim);
  canvas.drawFastHLine(8,118,224,C::dim2);
  text(asleep?"A:WAKE":exploration.active?"TILT:AIM HOLD A:EXIT":"A:EXPLORE",8,124,C::ink);
  char found[18];snprintf(found,sizeof(found),"FOUND %02u",exploration.discoveries%100);
  text(found,183,124,C::dim);
  present(ms);
}

void drawCatchStar(int x,int y,uint16_t color){
  canvas.fillRect(x-1,y-5,3,11,color);canvas.fillRect(x-5,y-1,11,3,color);
  canvas.fillRect(x-3,y-3,7,7,color);canvas.fillRect(x-1,y-1,3,3,C::screen);
}
void renderStarCatch(uint32_t ms){
  canvas.fillSprite(C::screen);
  auto text=[&](const char* s,int x,int y,uint16_t color=C::ink){pixelText(s,x,y,color);};
  text("CLAWD // CATCH",8,6);
  drawStatusIcons(W-8-BATTERY_W-6-UIStatus::Width,4,ms);
  drawBattery(W-8-BATTERY_W,3);
  canvas.drawFastHLine(8,19,224,C::dim2);canvas.drawFastVLine(75,26,86,C::dim2);
  const bool asleep=petState()==PET_SLEEP;
  text(asleep?"REST":catchStars.active?"FOCUS":"READY",8,26,C::dim);
  canvas.setClipRect(1,38,73,63);
  petSetScale(deviceStreaming()?.74f:.87f);petDraw(&canvas,47,ms,deviceStreaming()?2:8,catchStars.offset);
  canvas.clearClipRect();
  char label[24];
  if(catchStars.combo>1){snprintf(label,sizeof(label),"X%u",catchStars.combo);text(label,8,102,C::ok);}
  else text("CLAWD",8,102,C::dim);
  snprintf(label,sizeof(label),"STARS %02u",catchStars.score);text(label,84,26);
  snprintf(label,sizeof(label),"%02uS",unsigned((catchStars.leftMs(ms)+999)/1000));text(label,208,26,catchStars.active?C::glow:C::dim);
  for(int x=88;x<232;x+=12)for(int y=43;y<116;y+=12)canvas.drawPixel(x,y,C::dim2);
  canvas.setClipRect(82,38,151,79);
  const bool recent=catchStars.feedback!=StarCatch::Feedback::None&&uint32_t(ms-catchStars.feedbackAt)<500;
  if(catchStars.active){
    for(const auto& star:catchStars.stars)if(star.active)drawCatchStar(lroundf(star.x),lroundf(star.y),C::glow);
    const int x=lroundf(catchStars.trayX()),half=StarCatch::TrayWidth/2;
    const uint16_t color=recent&&catchStars.feedback==StarCatch::Feedback::Caught?C::ok:C::ink;
    canvas.fillRect(x-half,104,StarCatch::TrayWidth,3,color);
    canvas.fillRect(x-half,99,2,5,C::ink);canvas.fillRect(x+half-2,99,2,5,C::ink);
    if(recent)text(catchStars.feedback==StarCatch::Feedback::Caught?"+1":"MISS",lroundf(catchStars.hitX)-6,87,color);
  }else{
    drawCatchStar(131,55,C::glow);drawCatchStar(196,66,C::dim);
    if(asleep){text("A:WAKE",130,89);text("PROBE PARKED",118,103,C::dim);}
    else if(catchStars.finished){
      text("ROUND CLEAR",116,80,C::ok);
      snprintf(label,sizeof(label),"BEST X%u",catchStars.bestCombo);text(label,124,93,C::dim);text("A:AGAIN",130,106);
    }else{text("A:START",128,90);text("TILT:CATCH",121,104,C::dim);}
    canvas.fillRect(145,113,StarCatch::TrayWidth,2,C::ink);
  }
  canvas.clearClipRect();
  // Detached particles, never arms or a solid tether to the tray.
  if(catchStars.active){canvas.fillRect(69,73,2,2,C::lite);canvas.fillRect(79,83,2,2,C::lite);}
  canvas.drawFastHLine(8,118,224,C::dim2);
  text(asleep?"A:WAKE":catchStars.active?"TILT:MOVE HOLD A:EXIT":"A:START",8,124);
  text(catchStars.active?"LOCK":"20S",208,124,C::dim);
  present(ms);
}

void uiDraw(uint32_t ms){
  FrameTimer frameTimer;
  prepareCanvas(ms);
  if (provActive()){ renderProv(ms); return; }
  if (otaActive()){ renderOta(ms); return; }
  if(gestures.switchReady||gestures.comboReady||transportBusy()||(transportHintUntil&&int32_t(transportHintUntil-ms)>0)){
    canvas.fillSprite(C::screen);const bool shortLayout=H==135;putCenter("ThinCube",shortLayout?6:12,C::ink);
    const bool switching=transportBusy();
    putCenter(switching?"SWITCHING":gestures.comboReady?"RE-PAIR":"CONNECTION",shortLayout?35:55,C::glow);
    putCenter(gestures.comboReady?"release A + B":gestures.switchReady?"release B":transportHint[0]?transportHint:"reconnecting...",shortLayout?68:95,C::ink);
    const auto shown=connectionHintMode(transportMode(),gestures.switchReady&&!gestures.comboReady,switching,transportRecord().mode);
    putCenter(connectionHintLabel(shown),shortLayout?100:130,C::ok);present(ms);return;
  }
  if(devicePairWindow()){
    canvas.fillSprite(C::screen);const int pitch=rowPitch();const bool shortLayout=H==135;
    putCenter("ThinCube",shortLayout?4:8,C::ink);rule((shortLayout?4:8)+pitch+4);
    putCenter("PAIR PIN",shortLayout?31:45,C::dim);putCenter(devicePairPin(),shortLayout?54:45+pitch+8,C::glow);
    if(shortLayout){
      putCenter(connectionHintLabel(transportMode()),80,C::ok);putCenter("open ThinCube on Mac",106,C::ink);
    }else{
      putCenter("open ThinCube",100,C::ink);putCenter("on your Mac",100+pitch,C::ink);
      putCenter(connectionHintLabel(transportMode()),100+2*pitch+4,C::ok);
      putCenter("A+B hold: pair",H-2*pitch,C::dim);
    }
    present(ms);return;
  }
  if (s_debug&&!stories.isDemo()){ renderDebug(ms); return; }
  if(W==240){
    if(LandscapeGames::scene(canvasRotation(ms))==LandscapeGames::Scene::Catch)renderStarCatch(ms);
    else renderExploration(ms);
    return;
  }
  canvas.fillSprite(C::screen);

  drawBattery(PAD,3);
  drawStatusIcons(W-PAD-UIStatus::Width,4,ms);
  rule(20);
  const bool live=deviceStreaming();
  const bool asleep = (petState() == PET_SLEEP);
  pixelText(live?"VOICE":"CLAWD",PAD,27,C::ink,2);
  pixelText(stateName(petState()),PAD,48,asleep?C::dim:petState()==PET_UPSET?C::err:C::dim);
  if(stories.playing()){
    clawdStoryDraw(canvas,stories.current(),stories.elapsed(ms));
    if(stories.isDemo()){char label[16];snprintf(label,sizeof(label),"DEMO %u/%u",unsigned(stories.current())+1,ClawdStory::Count);pixelText(label,W-PAD-int(strlen(label))*6,48,C::dim);}
  }else{petSetScale(1.55f);petDraw(&canvas,77,ms);}
  if(live){
    // The large meter shows microphone level; the small status wave is static.
    const float level=std::max(0.f,std::min(1.f,deviceLevel()));
    for(int i=0;i<15;i++){
      const int shape=3+((i*7+3)%11);
      const int h=2+int(shape*level*1.6f);
      canvas.fillRect(9+i*8,183-h/2,3,h,C::ink);
    }
  }else{
    const char* s=thought();pixelText(s,(W-int(strlen(s))*6)/2,176,C::ink);
    pixelText("NOISE",PAD,194,C::ink);segBar(43,197,int(deviceLevel()*10),C::ink);
  }
  const char* caption=connectionCaption(ms);
  pixelText(caption,PAD,live?201:209,UIStatus::voiceIcon(deviceVoiceStatus(ms))==UIStatus::VoiceIcon::Warning?C::warn:deviceMuted()?C::err:C::ok);
  rule(224);
  pixelText(live?"A:KEY":asleep?"A:WAKE":"A:PLAY",PAD,230,C::ink);
  pixelText("B:MUTE",W-PAD-36,230,C::ink);
  present(ms);
}
