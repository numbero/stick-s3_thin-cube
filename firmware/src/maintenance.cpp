#include "maintenance.h"
#include "maintenance_policy.h"
#include "phone_update_page.h"
#include "ota.h"
#include "device.h"
#include "transport.h"
#include "wifi_prov.h"
#include "connection_setup.h"
#include "deep_sleep.h"
#include "power_status.h"
#include "power_save.h"
#include "audio_input.h"
#include "sound_effects.h"
#include "ui.h"
#include "theme.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <esp_system.h>
#include <cJSON.h>
#include <atomic>
#include <mutex>

#ifndef CLAWD_FW_VER
#define CLAWD_FW_VER "dev"
#endif

namespace {
std::atomic<bool> active{false},ready{false},pending{false},leaving{false};
std::atomic<uint32_t> touchedAt{0},rebootAt{0};
std::atomic<int> battery{-1};
std::atomic<const char*> failure{""};
bool bootRequested=false,bootBlocked=false,bootPrepared=false,pauseApplied=false;
char apSSID[32]{},code[7]{},sessionToken[33]{};
MaintenanceRequestRecord acceptedRequest;
MaintenanceSessionAttempts attempts;
std::mutex sessionGuard;
DNSServer dns;
httpd_handle_t server=nullptr;

bool protectedOperation(){const auto s=otaStage();return s!=OTA_IDLE&&s!=OTA_FAIL;}
// The capture runs before uiBegin, so draw directly without allocating another
// framebuffer. Whole-pixel Font0 cells match the later maintenance page.
void bootLine(const char* text,int y,uint16_t color,int scale=1){
  M5.Display.setFont(&fonts::Font0);M5.Display.setTextSize(scale);
  M5.Display.setTextWrap(false,false);M5.Display.setTextDatum(textdatum_t::top_left);
  // Opaque whole cells let a changed countdown digit replace its predecessor
  // without clearing and repainting a large region of the display.
  M5.Display.setTextColor(color,C::screen);
  M5.Display.drawString(text,(135-int(strlen(text))*6*scale)/2,y);
}
void bootFrame(){
  M5.Display.clearClipRect();M5.Display.fillScreen(C::screen);
  bootLine("PHONE UPDATE",11,C::ink);
  M5.Display.fillRect(10,30,115,1,C::dim2);
}
void bootHoldPrompt(bool ready){
  M5.Display.startWrite();
  if(ready){
    // Clear just the old labels. The digit/A shares one opaque 36x48 cell.
    M5.Display.fillRect(31,50,72,16,C::screen);
    M5.Display.fillRect(46,146,42,8,C::screen);
    M5.Display.fillRect(25,173,84,8,C::screen);
    bootLine("READY",50,C::ink,2);
    bootLine("A",88,C::glow,6);
    bootLine("RELEASE A",155,C::glow,2);
    bootLine("to open hotspot",181,C::dim);
  }else{
    bootLine("HOLD A",50,C::ink,2);
    bootLine("2",88,C::glow,6);
    bootLine("SECONDS",146,C::dim);
    bootLine("Keep holding A",173,C::ink);
  }
  M5.Display.endWrite();
}
bool sameSecret(const char* a,const char* b,size_t length){
  uint8_t difference=0;for(size_t i=0;i<length;++i)difference|=uint8_t(a[i])^uint8_t(b[i]);return difference==0;
}
esp_err_t response(httpd_req_t* request,const char* body,const char* status="200 OK",const char* type="application/json"){
  httpd_resp_set_status(request,status);httpd_resp_set_type(request,type);
  httpd_resp_set_hdr(request,"Cache-Control","no-store");
  httpd_resp_set_hdr(request,"X-Content-Type-Options","nosniff");
  return httpd_resp_send(request,body,HTTPD_RESP_USE_STRLEN);
}
esp_err_t page(httpd_req_t* request){
  httpd_resp_set_hdr(request,"Content-Security-Policy","default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
  return response(request,phoneUpdatePage,"200 OK","text/html; charset=utf-8");
}
esp_err_t redirect(httpd_req_t* request){
  httpd_resp_set_hdr(request,"Location","http://192.168.4.1/");return response(request,"","302 Found","text/plain");
}
esp_err_t info(httpd_req_t* request){
  cJSON* value=cJSON_CreateObject();
  cJSON_AddBoolToObject(value,"active",active.load());cJSON_AddBoolToObject(value,"ready",ready.load());
  cJSON_AddStringToObject(value,"version",CLAWD_FW_VER);cJSON_AddStringToObject(value,"device","ThinCube-StickS3-v1");
  cJSON_AddStringToObject(value,"name",deviceName());
  cJSON_AddStringToObject(value,"project","ThinCube-StickS3-v1");
  cJSON_AddNumberToObject(value,"level",battery.load());cJSON_AddNumberToObject(value,"maxSize",2097152);
  cJSON_AddStringToObject(value,"stage",otaStageName(otaStage()));
  const char* maintenanceError=failure.load();
  cJSON_AddStringToObject(value,"err",maintenanceError[0]?maintenanceError:otaLastErr());
  cJSON_AddNumberToObject(value,"idleTimeoutSec",MaintenanceIdlePolicy::TimeoutMs/1000);
  char* body=cJSON_PrintUnformatted(value);cJSON_Delete(value);
  if(!body)return response(request,"{\"error\":\"memory\"}","503 Service Unavailable");
  const auto result=response(request,body);cJSON_free(body);return result;
}
esp_err_t session(httpd_req_t* request){
  if(!ready.load()||leaving.load())return response(request,"{\"error\":\"not_ready\"}","409 Conflict");
  if(request->content_len<2||request->content_len>128)return response(request,"{\"error\":\"bad_request\"}","400 Bad Request");
  // Requiring JSON also prevents a cross-origin HTML form from guessing codes.
  char contentType[64]{};
  if(httpd_req_get_hdr_value_str(request,"Content-Type",contentType,sizeof(contentType))!=ESP_OK||strncmp(contentType,"application/json",16))
    return response(request,"{\"error\":\"json_required\"}","415 Unsupported Media Type");
  char body[129];size_t read=0;
  while(read<request->content_len){const int got=httpd_req_recv(request,body+read,request->content_len-read);if(got<=0)return ESP_FAIL;read+=got;}body[read]=0;
  cJSON* json=cJSON_Parse(body);const auto supplied=cJSON_GetObjectItemCaseSensitive(json,"code");
  char candidate[7]{};const bool valid=cJSON_IsString(supplied)&&strlen(supplied->valuestring)==6;
  if(valid)memcpy(candidate,supplied->valuestring,6);cJSON_Delete(json);
  std::lock_guard<std::mutex> lock(sessionGuard);
  if(!maintenanceReady())return response(request,"{\"error\":\"not_ready\"}","409 Conflict");
  const uint32_t now=millis();
  if(!attempts.allowed(now))return response(request,"{\"error\":\"too_many_attempts\"}","429 Too Many Requests");
  if(!valid||!sameSecret(candidate,code,6)){attempts.rejected(now);return response(request,"{\"error\":\"invalid_code\"}","401 Unauthorized");}
  attempts.accepted();maintenanceTouch();
  // A stable RAM token permits retry or a page refresh without revoking an
  // existing upload. It disappears when this temporary mode ends.
  if(!sessionToken[0])for(int i=0;i<16;++i)snprintf(sessionToken+i*2,3,"%02x",uint8_t(esp_random()));
  char result[64];snprintf(result,sizeof(result),"{\"token\":\"%s\"}",sessionToken);return response(request,result);
}
void stopServices(){
  ready=false;
  // HTTP workers may hold the session/OTA/admission locks; never hold any of
  // them while waiting for httpd_stop to join the server task.
  if(server){httpd_stop(server);server=nullptr;}
  dns.stop();WiFi.softAPdisconnect(true);
}
bool startServices(){
  otaStopServer();WiFi.disconnect(false,false);WiFi.mode(WIFI_AP);
  if(!WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0))||!WiFi.softAP(apSSID)){
    failure="hotspot_failed";return false;
  }
  if(!dns.start(53,"*",IPAddress(192,168,4,1))){failure="dns_failed";return false;}
  httpd_config_t config=HTTPD_DEFAULT_CONFIG();config.max_uri_handlers=16;config.stack_size=12288;
  config.max_open_sockets=5;config.lru_purge_enable=true;config.recv_wait_timeout=5;config.send_wait_timeout=5;
  config.uri_match_fn=httpd_uri_match_wildcard;
  if(httpd_start(&server,&config)!=ESP_OK){failure="portal_failed";return false;}
  const httpd_uri_t routes[]={
    {.uri="/",.method=HTTP_GET,.handler=page},
    {.uri="/maintenance/info",.method=HTTP_GET,.handler=info},
    {.uri="/maintenance/session",.method=HTTP_POST,.handler=session},
    {.uri="/generate_204",.method=HTTP_GET,.handler=redirect},
    {.uri="/hotspot-detect.html",.method=HTTP_GET,.handler=redirect},
    {.uri="/connecttest.txt",.method=HTTP_GET,.handler=redirect},
    {.uri="/ncsi.txt",.method=HTTP_GET,.handler=redirect},
    {.uri="/success.txt",.method=HTTP_GET,.handler=redirect},
  };
  for(const auto& route:routes)if(httpd_register_uri_handler(server,&route)!=ESP_OK){failure="portal_routes_failed";return false;}
  if(!otaRegisterRoutes(server)){failure="ota_routes_failed";return false;}
  const httpd_uri_t fallback={.uri="/*",.method=HTTP_GET,.handler=redirect};
  if(httpd_register_uri_handler(server,&fallback)!=ESP_OK){failure="portal_routes_failed";return false;}
  return true;
}
void scheduleExit(bool idle){
  std::unique_lock<std::mutex> admission(transportAdmission(),std::try_to_lock);
  if(!admission.owns_lock())return;
  std::unique_lock<std::mutex> session(sessionGuard,std::try_to_lock);
  if(!session.owns_lock()||!active.load()||leaving.load()||protectedOperation())return;
  if(idle&&!MaintenanceIdlePolicy::expired(millis(),touchedAt.load(),false))return;
  leaving=true;rebootAt=millis()+300;
}
}

void maintenancePrepareBoot(){
  bootPrepared=true;
  struct Store {
    Preferences preferences;bool opened=false;
    Store(){opened=preferences.begin("tcmaint",true);}
    ~Store(){if(opened)preferences.end();}
    bool exists(){return opened&&preferences.isKey("request");}
    bool read(MaintenanceRequestRecord& value){return preferences.getBytesLength("request")==sizeof(value)&&preferences.getBytes("request",&value,sizeof(value))==sizeof(value);}
    bool clear(){
      preferences.end();opened=preferences.begin("tcmaint",false);
      return opened&&preferences.remove("request");
    }
  } store;
  const auto consumed=maintenanceConsumeRequest(store);
  bootRequested=consumed==MaintenanceConsume::Requested;
  if(consumed==MaintenanceConsume::StorageFailed){bootBlocked=true;failure="request_clear_failed";}
  // transportBegin completes legacy transactions. Snapshot before that write
  // so an interrupted switch always receives its ordinary recovery boot first.
  Preferences transport;
  if(transport.begin("tcmode",true)){
    if(transport.isKey("transaction")){
      TransportRecord record;
      const bool read=transport.getBytesLength("transaction")==sizeof(record)&&transport.getBytes("transaction",&record,sizeof(record))==sizeof(record);
      bootBlocked=bootBlocked||!read||!record.valid()||record.phase==SwitchPhase::Pending||record.phase==SwitchPhase::Trial;
    }
    transport.end();
  }
  Preferences network;
  if(network.begin("clawd",true)){
    if(network.isKey("wifiChange")){
      WiFiChange change;
      const bool read=network.getBytesLength("wifiChange")==sizeof(change)&&network.getBytes("wifiChange",&change,sizeof(change))==sizeof(change);
      bootBlocked=bootBlocked||!read||!change.valid()||change.phase==WiFiChange::Pending||change.rebootPending;
    }
    network.end();
  }
}

bool maintenanceCaptureBoot(bool aInitially,bool bInitially){
  if(!aInitially||bInitially)return false;
  MaintenanceBootGesture gesture(aInitially,bInitially,millis());
  bool drawn=false,releaseShown=false;
  int secondsShown=2,progressShown=0;
  for(;;){
    M5.update();const uint32_t now=millis();
    const auto action=gesture.update(now,M5.BtnA.isPressed(),M5.BtnB.isPressed());
    if(action==MaintenanceBootGesture::Action::Enter)return true;
    if(action==MaintenanceBootGesture::Action::Cancel)return false;
    if(!drawn){
      M5.Display.startWrite();bootFrame();bootLine("B: CANCEL",225,C::dim);
      M5.Display.fillRect(19,201,97,5,C::panel);M5.Display.endWrite();
      bootHoldPrompt(gesture.ready);drawn=true;releaseShown=gesture.ready;
    }else if(gesture.ready&&!releaseShown){
      bootHoldPrompt(true);releaseShown=true;
    }
    if(gesture.ready){
      if(progressShown<97){
        // A single small color transition marks readiness; it is never
        // repainted while waiting for release, including a stuck key.
        M5.Display.fillRect(19,201,97,5,C::glow);progressShown=97;
      }
    }else{
      const uint32_t elapsed=uint32_t(now-gesture.since);
      const int seconds=elapsed<1000?2:1;
      if(seconds!=secondsShown){bootLine("1",88,C::glow,6);secondsShown=seconds;}
      const int progress=int(elapsed*97/MaintenanceBootGesture::HoldMs);
      if(progress>progressShown){
        M5.Display.fillRect(19+progressShown,201,progress-progressShown,5,C::ink);
        progressShown=progress;
      }
    }
    delay(20);
  }
}

void maintenanceBegin(bool physicalRequested,bool blockedByBootTransaction){
  if(!bootPrepared)maintenancePrepareBoot();
  const bool requested=physicalRequested||bootRequested;bootRequested=false;
  if(!requested)return;
  if(bootBlocked||blockedByBootTransaction||connectionSetupResetPending()||connectionSetupActive()||provActive()||provChangeRecord().phase==WiFiChange::Pending||transportBusy()){
    Serial.println("[maintenance] request consumed; existing setup/network/transport transaction has priority");return;
  }
  if(!transportBootVerified()){
    Serial.println("[maintenance] request consumed; OTA trial must complete normal boot health checks first");
    if(physicalRequested){
      M5.Display.startWrite();bootFrame();
      bootLine("Checking firmware",86,C::glow);
      bootLine("Try again later",118,C::dim);M5.Display.endWrite();
      const uint32_t since=millis();while(uint32_t(millis()-since)<1500){M5.update();delay(10);}
    }
    return;
  }
  snprintf(apSSID,sizeof(apSSID),"ThinCube-Update-%06X",unsigned(ESP.getEfuseMac()>>24)&0xffffff);
  // The temporary hotspot is open. A fresh screen code and RAM session token
  // authorize OTA; no paired Mac key or saved Wi-Fi state is overwritten.
  snprintf(code,sizeof(code),"%06u",unsigned(esp_random()%1000000));
  active=true;touchedAt=millis();battery=powerBatteryLevel(millis());
  devicePause();soundCancel();audioTick(false);deepSleepCancel();powerSavePerformance();
  if(startServices()){ready=true;uiLog("phone update ready");}
  else{stopServices();uiLog("phone update: %s",failure.load());}
}

bool maintenanceActive(){return active.load();}
bool maintenanceReady(){return ready.load()&&!leaving.load();}
bool maintenancePending(){return pending.load()||leaving.load();}
const char* maintenanceRequest(const char* requestId){
  MaintenanceRequestRecord request;
  if(!requestId||!requestId[0]||strlen(requestId)>36)return "invalid_request";
  strcpy(request.request,requestId);if(!request.valid())return "invalid_request";
  std::unique_lock<std::mutex> admission(transportAdmission(),std::try_to_lock);
  if(!admission.owns_lock())return "busy";
  if(pending.load())return !strcmp(request.request,acceptedRequest.request)?nullptr:"busy";
  if(active.load()||leaving.load()||connectionSetupActive()||connectionSetupResetPending()||provActive()||provChangeRecord().phase==WiFiChange::Pending||otaActive()||transportBusy()||devicePairWindow()||deepSleepPending()||powerCalibrationOwnsUI())return "busy";
  if(!transportBootVerified())return "boot_unverified";
  Preferences preferences;if(!preferences.begin("tcmaint",false))return "storage_failed";
  const bool saved=preferences.putBytes("request",&request,sizeof(request))==sizeof(request);preferences.end();
  if(!saved)return "storage_failed";
  acceptedRequest=request;rebootAt=millis()+700;pending=true;return nullptr;
}
void maintenanceTouch(){touchedAt=millis();}
void maintenanceExit(){scheduleExit(false);}
void maintenanceTick(uint32_t now){
  if(pending.load()&&!pauseApplied){
    pauseApplied=true;devicePause();soundCancel();audioTick(false);deepSleepCancel();powerSavePerformance();uiLog("phone update restart");
  }
  if(active.load()){
    battery=powerBatteryLevel(now);if(ready.load())dns.processNextRequest();
    // Confirm/ready have their own short OTA timeout. Never reboot while an
    // HTTP worker owns a write/verification handle or is scheduling success.
    if(MaintenanceIdlePolicy::expired(now,touchedAt.load(),protectedOperation()))scheduleExit(true);
  }
  const uint32_t restart=rebootAt.load();
  if(restart&&int32_t(now-restart)>=0&&!protectedOperation()){
    if(active.load())stopServices();ESP.restart();
  }
}
bool maintenanceAuthorized(httpd_req_t* request){
  if(!maintenanceReady())return false;
  char supplied[33]{};
  if(httpd_req_get_hdr_value_str(request,"X-ThinCube-Maintenance",supplied,sizeof(supplied))!=ESP_OK||strlen(supplied)!=32)return false;
  std::lock_guard<std::mutex> lock(sessionGuard);
  return sessionToken[0]&&sameSecret(supplied,sessionToken,32);
}
const char* maintenanceApSsid(){return apSSID;}
const char* maintenanceCode(){return code;}
const char* maintenanceLastErr(){return failure.load();}
int maintenanceIdleLeftSec(uint32_t now){
  const int32_t age=int32_t(now-touchedAt.load());
  const uint32_t elapsed=age>0?uint32_t(age):0;
  return elapsed>=MaintenanceIdlePolicy::TimeoutMs?0:int((MaintenanceIdlePolicy::TimeoutMs-elapsed+999)/1000);
}
