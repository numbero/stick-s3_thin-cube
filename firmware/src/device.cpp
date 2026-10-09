#include "device.h"
#include "voice_state.h"
#include "control_lease.h"
#include "pose_voice.h"
#include "display_motion.h"
#include "audio_input.h"
#include "sound_effects.h"
#include "ota.h"
#include "ui.h"
#include "transport.h"
#include "ble_link.h"
#include "ima_adpcm.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <cJSON.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <mbedtls/md.h>
#include <lwip/sockets.h>
#include "peer_address.h"
#include <algorithm>
#include <atomic>
extern const char* firmwareVersion();

namespace {
struct Command {
  char op[24] = {}, ssid[33] = {}, pass[65] = {}, target[5] = {}, requestId[37] = {};
  int fd = -1, port = 0, flags = 0, brightness = 128, soundVolume = 35;
  uint32_t ack = 0, epoch = 0, address = 0;
  float gain = 1;
  bool allowed = false, muted = false, local = true, ready = false, soundMuted = false;
};
struct Edge { uint32_t seq; bool down; };
QueueHandle_t commands;
SemaphoreHandle_t publication;
VoiceState voice;
ControlLease controlLease;
PoseDetector pose;
DisplayOrientation displayOrientation;
WiFiUDP udp;
char name[24], pin[7], key[33] = {}, published[1600] = "{}";
bool bleSnapshotDirty=true;
std::atomic<uint32_t> pairingUntil{0};
uint32_t lastPairTry = 0, epoch = 0, edgeSeq = 0, seq = 0, clockSample = 0;
int pairTries = 0, owner = -1, outputPort = 0;
IPAddress outputIP;
float gain = 1;
uint8_t brightness = 128, nonce[16] = {};
Edge edges[32];
unsigned edgeRead = 0, edgeCount = 0;
bool lastHeld = false, lastStreaming = false, networkPending = false, healthy = false;
PoseVoiceController poseVoice;
uint32_t calibrationSeq = 0;
char calibrationOp[24] = {};
bool calibrationOK = false;
bool runtimeRegistered = false;
char nextSSID[33] = {}, nextPass[65] = {};
bool bleAuthenticated=false;
uint32_t bleSeenGeneration=0, challengeUntil=0;
char challenge[33]={}, switchError[40]={}, rejectedRequest[37]={};


void hexRandom(char* out, size_t bytes) {
  for (size_t i=0;i<bytes;i++) { uint8_t b=esp_random(); snprintf(out+i*2,3,"%02x",b); }
}
void persist() {
  Preferences p;
  if (!p.begin("thincube", false)) return;
  p.putBool("allowed",voice.allowed); p.putBool("muted",voice.muted);
  p.putBool("local",voice.localSound); p.putFloat("gain",gain); p.putUChar("bright",brightness);
  p.putUChar("sndVol",soundVolume());p.putBool("sndMute",soundMuted());
  p.putInt("upright",pose.uprightSign); p.putInt("faceup",pose.faceUpSign); p.end();
}
esp_err_t json(httpd_req_t* r, const char* data, const char* status="200 OK") {
  httpd_resp_set_status(r,status); httpd_resp_set_type(r,"application/json");
  httpd_resp_set_hdr(r,"Cache-Control","no-store");
  return httpd_resp_send(r,data,HTTPD_RESP_USE_STRLEN);
}
cJSON* body(httpd_req_t* r) {
  if (r->content_len < 2 || r->content_len > 768) return nullptr;
  char b[769]; size_t n=0;
  while (n<r->content_len) {
    int got=httpd_req_recv(r,b+n,r->content_len-n);
    if (got<=0) return nullptr;
    n+=got;
  }
  b[n]=0; return cJSON_ParseWithLength(b,n);
}
bool enqueue(const Command& c) { return xQueueSend(commands,&c,0)==pdTRUE; }
bool auth(httpd_req_t* r) {
  if (deviceAuthorized(r)) return true;
  json(r,"{\"error\":\"pairing_required\"}","401 Unauthorized"); return false;
}
esp_err_t info(httpd_req_t* r) {
  char b[256];
  xSemaphoreTake(publication,portMAX_DELAY);bool paired=key[0];xSemaphoreGive(publication);
  snprintf(b,sizeof(b),"{\"name\":\"%s\",\"version\":\"%s\",\"protocol\":1,\"transport\":\"wifi\",\"transportSwitch\":true,\"paired\":%s}",name,firmwareVersion(),paired?"true":"false");
  return json(r,b);
}
esp_err_t pair(httpd_req_t* r) {
  cJSON* j=body(r); const cJSON* p=cJSON_GetObjectItemCaseSensitive(j,"pin");
  xSemaphoreTake(publication,portMAX_DELAY);
  const uint32_t now=millis();
  const bool ok=devicePairWindow() && pairTries<10 && uint32_t(now-lastPairTry)>1000 && cJSON_IsString(p) && !strcmp(p->valuestring,pin);
  lastPairTry=now; pairTries++;
  if (!ok) { xSemaphoreGive(publication); cJSON_Delete(j); return json(r,"{\"error\":\"invalid_pin_or_window\"}","403 Forbidden"); }
  hexRandom(key,16);
  Preferences prefs;bool saved=false;
  if(prefs.begin("thincube",false)){saved=prefs.putString("key",key)==32;prefs.end();}
  if(!saved){key[0]=0;xSemaphoreGive(publication);cJSON_Delete(j);return json(r,"{\"error\":\"key_storage_failed\"}","500 Internal Server Error");}
  pairingUntil=0;
  char b[128]; snprintf(b,sizeof(b),"{\"key\":\"%s\",\"name\":\"%s\"}",key,name);
  xSemaphoreGive(publication); cJSON_Delete(j); return json(r,b);
}
esp_err_t status(httpd_req_t* r) {
  if(!auth(r)) return ESP_OK;
  char b[sizeof(published)]; xSemaphoreTake(publication,portMAX_DELAY); strcpy(b,published); xSemaphoreGive(publication);
  return json(r,b);
}
esp_err_t settings(httpd_req_t* r) {
  if(!auth(r)) return ESP_OK;
  cJSON* j=body(r); if(!cJSON_IsObject(j)){cJSON_Delete(j);return json(r,"{\"error\":\"bad_json\"}","400 Bad Request");}
  Command c; strcpy(c.op,"settings"); bool valid=true;
  auto flag=[&](const char* n,int bit,bool& v){auto x=cJSON_GetObjectItemCaseSensitive(j,n);if(x){if(!cJSON_IsBool(x))valid=false;else{c.flags|=bit;v=cJSON_IsTrue(x);}}};
  flag("allowed",1,c.allowed); flag("muted",2,c.muted); flag("localSound",4,c.local); flag("soundMuted",32,c.soundMuted);
  auto b=cJSON_GetObjectItemCaseSensitive(j,"brightness");
  if(b){if(!cJSON_IsNumber(b)||b->valuedouble<8||b->valuedouble>255)valid=false;else{c.flags|=8;c.brightness=b->valueint;}}
  auto g=cJSON_GetObjectItemCaseSensitive(j,"gain");
  if(g){if(!cJSON_IsNumber(g)||g->valuedouble<.1||g->valuedouble>4)valid=false;else{c.flags|=16;c.gain=g->valuedouble;}}
  auto volume=cJSON_GetObjectItemCaseSensitive(j,"soundVolume");
  if(volume){if(!cJSON_IsNumber(volume)||!isfinite(volume->valuedouble)||volume->valuedouble<0||volume->valuedouble>100||volume->valuedouble!=volume->valueint)valid=false;else{c.flags|=64;c.soundVolume=volume->valueint;}}
  cJSON_Delete(j);
  if(!valid)return json(r,"{\"error\":\"invalid_settings\"}","400 Bad Request");
  return enqueue(c)?json(r,"{\"accepted\":true}"):json(r,"{\"error\":\"busy\"}","503 Service Unavailable");
}
esp_err_t wifi(httpd_req_t* r) {
  if(!auth(r))return ESP_OK;
  cJSON* j=body(r);auto s=cJSON_GetObjectItemCaseSensitive(j,"ssid");auto p=cJSON_GetObjectItemCaseSensitive(j,"password");
  if(!cJSON_IsString(s)||!cJSON_IsString(p)||strlen(s->valuestring)<1||strlen(s->valuestring)>32||strlen(p->valuestring)>63||(strlen(p->valuestring)>0&&strlen(p->valuestring)<8)){
    cJSON_Delete(j);return json(r,"{\"error\":\"invalid_wifi\"}","400 Bad Request");
  }
  Command c;strcpy(c.op,"wifi");strcpy(c.ssid,s->valuestring);strcpy(c.pass,p->valuestring);cJSON_Delete(j);
  return enqueue(c)?json(r,"{\"accepted\":true}"):json(r,"{\"error\":\"busy\"}","503 Service Unavailable");
}
esp_err_t ws(httpd_req_t* r) {
  if(r->method==HTTP_GET) {
    if(!auth(r))return ESP_FAIL;
    Command c; strcpy(c.op,"connect");c.fd=httpd_req_to_sockfd(r);
    sockaddr_storage peer{};socklen_t len=sizeof(peer);
    if(getpeername(c.fd,(sockaddr*)&peer,&len)!=0)return ESP_FAIL;
    if(!peerIPv4(peer,len,c.address))return ESP_FAIL;
    return enqueue(c)?ESP_OK:ESP_FAIL;
  }
  httpd_ws_frame_t f{}; f.type=HTTPD_WS_TYPE_TEXT;
  if(httpd_ws_recv_frame(r,&f,0)!=ESP_OK || f.len>768)return ESP_FAIL;
  if(f.type==HTTPD_WS_TYPE_CLOSE)return ESP_FAIL;
  // Consume both JSON frame representations; leaving a binary payload unread
  // desynchronizes the next WebSocket header and tears down the control lease.
  if(f.type!=HTTPD_WS_TYPE_TEXT&&f.type!=HTTPD_WS_TYPE_BINARY)return ESP_FAIL;
  char b[769];f.payload=(uint8_t*)b;
  if(httpd_ws_recv_frame(r,&f,768)!=ESP_OK)return ESP_FAIL;
  b[f.len]=0;
  cJSON* j=cJSON_Parse(b);auto op=cJSON_GetObjectItemCaseSensitive(j,"op");
  Command c;c.fd=httpd_req_to_sockfd(r);
  if(!cJSON_IsString(op)||strlen(op->valuestring)>=sizeof(c.op)){cJSON_Delete(j);return ESP_FAIL;}
  strcpy(c.op,op->valuestring);
  auto number=[&](const char* n){auto v=cJSON_GetObjectItemCaseSensitive(j,n);return cJSON_IsNumber(v)?v->valuedouble:0;};
  c.epoch=uint32_t(number("epoch"));c.ack=uint32_t(number("ack"));c.port=int(number("port"));
  c.ready=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"ready"));
  auto target=cJSON_GetObjectItemCaseSensitive(j,"target"),id=cJSON_GetObjectItemCaseSensitive(j,"requestId");
  if(cJSON_IsString(target)&&strlen(target->valuestring)<sizeof(c.target))strcpy(c.target,target->valuestring);
  if(cJSON_IsString(id)&&strlen(id->valuestring)<sizeof(c.requestId))strcpy(c.requestId,id->valuestring);
  cJSON_Delete(j);
  if(!enqueue(c))return ESP_FAIL;
  char response[sizeof(published)];xSemaphoreTake(publication,portMAX_DELAY);strcpy(response,published);xSemaphoreGive(publication);
  httpd_ws_frame_t out{};out.type=HTTPD_WS_TYPE_TEXT;out.payload=(uint8_t*)response;out.len=strlen(response);
  return httpd_ws_send_frame(r,&out);
}
void edge(bool down) {
  if(edgeCount==32) { voice.stop();edgeRead=edgeCount=0;down=false; }
  edges[(edgeRead+edgeCount)%32]={++edgeSeq,down};edgeCount++;
}
const char* voiceBlock() {
  if(!voice.allowed)return "disabled";
  if(voice.muted)return "muted";
  if(voice.busy)return "busy";
  if(!voice.peer)return "disconnected";
  if(!pose.uprightSign)return "calibration";
  if(voice.sleeping)return "sleeping";
  if(!voice.captureReady)return "microphone";
  if(voice.streaming)return "";
  return voice.inverted ? "rearm" : "invert";
}
void publish() {
  cJSON* j=cJSON_CreateObject();
  cJSON_AddNumberToObject(j,"epoch",epoch);
  cJSON_AddStringToObject(j,"deviceId",name);
  cJSON_AddStringToObject(j,"transport",transportName(transportMode()));
  cJSON_AddBoolToObject(j,"transportSwitch",true);
  cJSON_AddNumberToObject(j,"audioRate",transportMode()==TransportMode::BLE?16000:48000);
  cJSON_AddStringToObject(j,"audioCodec",transportMode()==TransportMode::BLE?"ima_adpcm":"pcm_s16le");
  cJSON_AddBoolToObject(j,"transportAudioReady",transportMode()!=TransportMode::BLE||bleAudioReady());
  if(transportMode()==TransportMode::BLE){cJSON_AddNumberToObject(j,"bleMtu",bleMTU());cJSON_AddNumberToObject(j,"bleAudioDropped",bleAudioDropped());}
  const auto& transaction=transportRecord();auto tr=cJSON_AddObjectToObject(j,"transportTransaction");
  cJSON_AddStringToObject(tr,"requestId",transaction.request);cJSON_AddStringToObject(tr,"source",transaction.source);
  const auto requested=transaction.phase==SwitchPhase::RolledBack?(transaction.previous==TransportMode::BLE?TransportMode::WiFi:TransportMode::BLE):transaction.mode;
  cJSON_AddStringToObject(tr,"target",transportName(requested));cJSON_AddStringToObject(tr,"previous",transportName(transaction.previous));
  cJSON_AddStringToObject(tr,"phase",switchName(transaction.phase));
  cJSON_AddStringToObject(j,"transportError",switchError);cJSON_AddStringToObject(j,"rejectedRequest",rejectedRequest);
  cJSON_AddBoolToObject(j,"allowed",voice.allowed);cJSON_AddBoolToObject(j,"muted",voice.muted);
  cJSON_AddBoolToObject(j,"localSound",voice.localSound);cJSON_AddBoolToObject(j,"streaming",voice.streaming);
  cJSON_AddBoolToObject(j,"manual",voice.manual);cJSON_AddBoolToObject(j,"held",voice.held);
  cJSON_AddBoolToObject(j,"micReady",voice.captureReady);cJSON_AddBoolToObject(j,"inverted",voice.inverted);
  cJSON_AddBoolToObject(j,"calibrated",pose.uprightSign!=0);cJSON_AddBoolToObject(j,"faceCalibrated",pose.faceUpSign!=0);
  cJSON_AddStringToObject(j,"pose",poseName(pose.stable));
  cJSON_AddStringToObject(j,"voiceBlock",voiceBlock());
  if(calibrationSeq){
    auto report=cJSON_AddObjectToObject(j,"calibration");
    cJSON_AddNumberToObject(report,"seq",calibrationSeq);
    cJSON_AddStringToObject(report,"op",calibrationOp);
    cJSON_AddBoolToObject(report,"ok",calibrationOK);
  }
  cJSON_AddNumberToObject(j,"gain",gain);cJSON_AddNumberToObject(j,"brightness",brightness);cJSON_AddNumberToObject(j,"level",audioLevel());
  cJSON_AddNumberToObject(j,"soundVolume",soundVolume());cJSON_AddBoolToObject(j,"soundMuted",soundMuted());
  cJSON_AddBoolToObject(j,"soundPlaying",soundPlaying());cJSON_AddNumberToObject(j,"soundCount",soundCount());
  cJSON_AddNumberToObject(j,"audioFrames",audioFrameCount());
  cJSON_AddNumberToObject(j,"audioDropped",audioDropCount());
  cJSON_AddStringToObject(j,"ota",otaStageName(otaStage()));
  esp_ota_img_states_t bootState;
  const bool verified=esp_ota_get_state_partition(esp_ota_get_running_partition(),&bootState)!=ESP_OK||bootState!=ESP_OTA_IMG_PENDING_VERIFY;
  cJSON_AddBoolToObject(j,"bootVerified",verified);
  char cookie[33];for(int i=0;i<16;i++)snprintf(cookie+i*2,3,"%02x",nonce[i]);cJSON_AddStringToObject(j,"session",cookie);
  if(edgeCount){auto e=cJSON_AddObjectToObject(j,"edge");cJSON_AddNumberToObject(e,"seq",edges[edgeRead].seq);cJSON_AddBoolToObject(e,"down",edges[edgeRead].down);}
  static char b[sizeof(published)]; if(cJSON_PrintPreallocated(j,b,sizeof(b),false)){
    xSemaphoreTake(publication,portMAX_DELAY);strcpy(published,b);xSemaphoreGive(publication);
  }
  cJSON_Delete(j);
}
void bleResponse(cJSON* request,cJSON* result){
  auto rid=cJSON_GetObjectItemCaseSensitive(request,"rid");
  if(cJSON_IsNumber(rid))cJSON_AddNumberToObject(result,"rid",rid->valuedouble);
  static char text[2048];if(cJSON_PrintPreallocated(result,text,sizeof(text),false))bleSendControl(text);cJSON_Delete(result);
}
void bleRequest(const char* text){
  cJSON* j=cJSON_Parse(text);auto op=cJSON_GetObjectItemCaseSensitive(j,"op");
  if(!cJSON_IsObject(j)||!cJSON_IsString(op)){cJSON_Delete(j);return;}
  const char* operation=op->valuestring;cJSON* result=cJSON_CreateObject();
  const char* error=nullptr;
  if(!strcmp(operation,"info")){
    cJSON_AddStringToObject(result,"name",name);cJSON_AddStringToObject(result,"version",firmwareVersion());
    cJSON_AddNumberToObject(result,"protocol",1);cJSON_AddBoolToObject(result,"paired",key[0]);
    cJSON_AddStringToObject(result,"transport","ble");cJSON_AddBoolToObject(result,"transportSwitch",true);
  }else if(!strcmp(operation,"challenge")){
    hexRandom(challenge,16);challengeUntil=millis()+10000;cJSON_AddStringToObject(result,"nonce",challenge);
  }else if(!strcmp(operation,"pair")){
    auto p=cJSON_GetObjectItemCaseSensitive(j,"pin");const uint32_t now=millis();
    bool ok=devicePairWindow()&&pairTries<10&&uint32_t(now-lastPairTry)>1000&&cJSON_IsString(p)&&!strcmp(pin,p->valuestring);
    lastPairTry=now;pairTries++;
    if(!ok)error="invalid_pin_or_window";
    else{
      xSemaphoreTake(publication,portMAX_DELAY);hexRandom(key,16);
      Preferences prefs;bool saved=false;if(prefs.begin("thincube",false)){saved=prefs.putString("key",key)==32;prefs.end();}
      if(!saved){key[0]=0;error="key_storage_failed";}else{pairingUntil=0;cJSON_AddStringToObject(result,"key",key);cJSON_AddStringToObject(result,"name",name);}
      xSemaphoreGive(publication);
    }
  }else if(!strcmp(operation,"auth")){
    auto proof=cJSON_GetObjectItemCaseSensitive(j,"proof");uint8_t binaryKey[16]={},digest[32]={};char message[128],hex[65]={};
    for(int i=0;i<16;i++){char h[3]={key[i*2],key[i*2+1],0};binaryKey[i]=strtoul(h,nullptr,16);}
    snprintf(message,sizeof(message),"ThinCube-BLE-v1|%s|%s",name,challenge);
    const auto* md=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_hmac(md,binaryKey,16,(const uint8_t*)message,strlen(message),digest);
    for(int i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",digest[i]);
    unsigned diff=0;if(cJSON_IsString(proof)&&strlen(proof->valuestring)==64)for(int i=0;i<64;i++)diff|=uint8_t(hex[i])^uint8_t(proof->valuestring[i]);else diff=1;
    const bool valid=key[0]&&challenge[0]&&int32_t(challengeUntil-millis())>0&&!diff;
    challenge[0]=0;memset(binaryKey,0,sizeof(binaryKey));memset(digest,0,sizeof(digest));
    if(!valid)error="pairing_required";
    else{bleAuthenticated=true;Command c;strcpy(c.op,"connect");c.fd=-2;if(!enqueue(c)){bleAuthenticated=false;error="busy";}else cJSON_AddBoolToObject(result,"authenticated",true);}
  }else if(!bleAuthenticated)error="pairing_required";
  else if(!strcmp(operation,"status")){cJSON_Delete(result);result=cJSON_Parse(published);if(!result)result=cJSON_CreateObject();}
  else{
    Command c;c.fd=-2;bool valid=true;snprintf(c.op,sizeof(c.op),"%s",operation);
    auto number=[&](const char* n){auto v=cJSON_GetObjectItemCaseSensitive(j,n);return cJSON_IsNumber(v)?v->valuedouble:0;};
    c.epoch=uint32_t(number("epoch"));c.ack=uint32_t(number("ack"));c.ready=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"ready"));
    if(!strcmp(operation,"settings")){
      auto flag=[&](const char* n,int bit,bool& v){auto x=cJSON_GetObjectItemCaseSensitive(j,n);if(x){if(!cJSON_IsBool(x))valid=false;else{c.flags|=bit;v=cJSON_IsTrue(x);}}};
      flag("allowed",1,c.allowed);flag("muted",2,c.muted);flag("localSound",4,c.local);flag("soundMuted",32,c.soundMuted);
      auto numeric=[&](const char* n,int bit,double low,double high,double& value){auto x=cJSON_GetObjectItemCaseSensitive(j,n);if(x){if(!cJSON_IsNumber(x)||!isfinite(x->valuedouble)||x->valuedouble<low||x->valuedouble>high)valid=false;else{c.flags|=bit;value=x->valuedouble;}}};
      double bright=c.brightness,volume=c.soundVolume,g=c.gain;numeric("brightness",8,8,255,bright);numeric("gain",16,.1,4,g);numeric("soundVolume",64,0,100,volume);
      if(bright!=int(bright)||volume!=int(volume))valid=false;c.brightness=bright;c.gain=g;c.soundVolume=volume;
    }else if(!strcmp(operation,"transport_switch")||!strcmp(operation,"transport_commit")){
      auto id=cJSON_GetObjectItemCaseSensitive(j,"requestId"),t=cJSON_GetObjectItemCaseSensitive(j,"target");
      if(!cJSON_IsString(id)||!id->valuestring[0]||strlen(id->valuestring)>36)valid=false;else strcpy(c.requestId,id->valuestring);
      if(cJSON_IsString(t)&&strlen(t->valuestring)<sizeof(c.target))strcpy(c.target,t->valuestring);
    }else if(strcmp(operation,"heartbeat")&&strcmp(operation,"start")&&strcmp(operation,"stop")&&strcmp(operation,"sound_preview")&&strcmp(operation,"calibrate_inverted")&&strcmp(operation,"calibrate_upright")&&strcmp(operation,"calibrate_faceup"))valid=false;
    if(!valid)error="invalid_command";else if(!enqueue(c))error="busy";else cJSON_AddBoolToObject(result,"accepted",true);
  }
  if(error)cJSON_AddStringToObject(result,"error",error);
  // Fire-and-forget heartbeats already have the encrypted ATT write response.
  // Only correlated exchanges need an additional application acknowledgement.
  if(!strcmp(operation,"heartbeat")&&!cJSON_GetObjectItemCaseSensitive(j,"rid")&&!error)cJSON_Delete(result);
  else bleResponse(j,result);
  cJSON_Delete(j);
}

}

void deviceBegin() {
  commands=xQueueCreate(12,sizeof(Command));publication=xSemaphoreCreateMutex();
  snprintf(name,sizeof(name),"ThinCube-%06X",unsigned(ESP.getEfuseMac()>>24)&0xffffff);
  Preferences p;if(p.begin("thincube",true)){
    voice.allowed=p.getBool("allowed",false);voice.muted=p.getBool("muted",false);voice.localSound=p.getBool("local",true);
    soundConfigure(p.getUChar("sndVol",35),p.getBool("sndMute",false));
    gain=p.getFloat("gain",1);brightness=p.getUChar("bright",128);pose.uprightSign=p.getInt("upright",0);pose.faceUpSign=p.getInt("faceup",0);
    snprintf(key,sizeof(key),"%s",p.getString("key","").c_str());p.end();
  }
  snprintf(pin,sizeof(pin),"%06u",unsigned(esp_random()%1000000));
  audioBegin(transportMode()==TransportMode::BLE);if(!key[0])deviceOpenPairing();
  healthy=commands&&publication&&M5.Mic.isEnabled();
}
void deviceRegister(httpd_handle_t server) {
  const httpd_uri_t routes[]={
    {.uri="/api/info",.method=HTTP_GET,.handler=info},
    {.uri="/api/pair",.method=HTTP_POST,.handler=pair},
    {.uri="/api/status",.method=HTTP_GET,.handler=status},
    {.uri="/api/settings",.method=HTTP_POST,.handler=settings},
    {.uri="/api/wifi",.method=HTTP_POST,.handler=wifi},
    {.uri="/control",.method=HTTP_GET,.handler=ws,.is_websocket=true},
  };
  for(auto& route:routes)if(httpd_register_uri_handler(server,&route)!=ESP_OK)healthy=false;
  String host=String(name);host.toLowerCase();
  if(MDNS.begin(host.c_str())){MDNS.setInstanceName(name);MDNS.addService("thincube","tcp",80);MDNS.addServiceTxt("thincube","tcp","protocol","1");}
  else healthy=false;
  runtimeRegistered=healthy;
}
bool deviceAuthorized(httpd_req_t* req) {
  char supplied[33]{};
  if(httpd_req_get_hdr_value_str(req,"X-ThinCube-Key",supplied,sizeof(supplied))!=ESP_OK)return false;
  xSemaphoreTake(publication,portMAX_DELAY);unsigned diff=0;
  for(int i=0;i<32;i++)diff|=uint8_t(supplied[i])^uint8_t(key[i]);
  const bool ok=key[0]&&strlen(supplied)==32&&!diff;xSemaphoreGive(publication);return ok;
}
void deviceOpenPairing() {
  voice.stop();voice.setPeer(false,millis());controlLease.reset();owner=-1;edgeRead=edgeCount=0;
  xSemaphoreTake(publication,portMAX_DELAY);
  key[0]=0;snprintf(pin,sizeof(pin),"%06u",unsigned(esp_random()%1000000));pairTries=0;pairingUntil=millis()+120000;
  Preferences p;if(p.begin("thincube",false)){p.remove("key");p.end();}
  xSemaphoreGive(publication);
  bleAuthenticated=false;bleForgetBonds(strtoul(pin,nullptr,10));
}
bool devicePairWindow(){const uint32_t until=pairingUntil.load();return until && int32_t(until-millis())>0;}
const char* devicePairPin(){return pin;}
const char* deviceName(){return name;}
void deviceStop(){voice.stop();}
void deviceToggleMute(){voice.muted=!voice.muted;if(voice.muted)voice.stop();persist();}
bool deviceStreaming(){return voice.streaming;}
bool deviceHeld(){return voice.held;}
bool deviceMuted(){return voice.muted;}
bool deviceLocalSound(){return voice.localSound&&!voice.muted;}
bool deviceInverted(){return voice.inverted;}
uint8_t deviceDisplayRotation(){return displayOrientation.rotation;}
bool deviceHealthy(){return healthy&&(transportMode()==TransportMode::BLE?bleHealthy():runtimeRegistered)&&(audioDelivered()||voice.muted||(!voice.localSound&&!voice.streaming));}
bool deviceControlConnected(uint32_t ms){
  const bool link=transportMode()==TransportMode::BLE?(bleConnected()&&bleAuthenticated):WiFi.status()==WL_CONNECTED;
  return link&&!transportBusy()&&controlLease.connected(ms);
}
UIStatus::VoiceInputs deviceVoiceStatus(uint32_t ms){
  return {voice.muted,voice.streaming,voice.allowed,deviceControlConnected(ms),voice.peer,voice.captureReady,pose.uprightSign!=0,voice.sleeping};
}
const char* deviceVoiceBlock(){return voiceBlock();}
float deviceLevel(){return voice.muted||soundSuppressListening(millis())?0:audioLevel();}
uint8_t deviceBrightness(){return brightness;}
DevicePose devicePose(){return pose.stable;}
void deviceMotion(float x,float y,float z,float gyro,uint32_t ms){
  pose.update(x,y,z,gyro,ms);
  const uint8_t previous=displayOrientation.rotation;
  displayOrientation.update(x,y,z,gyro,pose.uprightSign,ms,uiLandscapeActive());
  if(displayOrientation.rotation!=previous)Serial.printf("[orientation] target %u confirmed %ums\n",displayOrientation.rotation,unsigned(displayOrientation.confirmedAfterMs));
}
bool deviceTakeNetwork(char* ssid,char* pass){if(!networkPending)return false;strcpy(ssid,nextSSID);strcpy(pass,nextPass);networkPending=false;return true;}
void deviceTick(uint32_t ms,bool sleeping,bool busy) {
  if(transportMode()==TransportMode::BLE){
    if(bleSeenGeneration!=bleGeneration()){bleSeenGeneration=bleGeneration();bleAuthenticated=false;challenge[0]=0;voice.setPeer(false,ms);controlLease.reset();owner=-1;}
    static char message[769];for(int i=0;i<4&&bleTake(message,sizeof(message));i++)bleRequest(message);
  }
  voice.sleeping=sleeping;voice.busy=busy||networkPending||transportBusy();
  Command c;
  while(xQueueReceive(commands,&c,0)==pdTRUE) {
    if(!strcmp(c.op,"connect")){
      voice.setPeer(false,ms);controlLease.reset();owner=c.fd;outputIP=IPAddress(c.address);epoch=esp_random();edgeRead=edgeCount=0;lastHeld=false;
      Serial.printf("[control] IPv4 audio peer %s\n",outputIP.toString().c_str());
    } else if(!strcmp(c.op,"settings")) {
      bleSnapshotDirty=true;
      if(c.flags&1)voice.allowed=c.allowed;if(c.flags&2)voice.muted=c.muted;if(c.flags&4)voice.localSound=c.local;
      if(c.flags&8)brightness=c.brightness;if(c.flags&16)gain=c.gain;
      if(c.flags&(32|64))soundConfigure(c.flags&64?c.soundVolume:soundVolume(),c.flags&32?c.soundMuted:soundMuted());
      if(!voice.allowed||voice.muted)voice.stop();persist();
    } else if(!strcmp(c.op,"wifi")) {
      if(busy||transportBusy())continue;voice.stop();strcpy(nextSSID,c.ssid);strcpy(nextPass,c.pass);networkPending=true;
    } else if(c.fd==owner&&c.epoch==epoch) {
      if(!strcmp(c.op,"heartbeat")){
        controlLease.heartbeat(ms);
        if(transportMode()==TransportMode::BLE){voice.setPeer(c.ready&&bleAuthenticated&&bleAudioReady(),ms);}
        else if(c.port>=1024&&c.port<=65535){outputPort=c.port;voice.setPeer(c.ready,ms);}
        if(edgeCount&&c.ack==edges[edgeRead].seq){edgeRead=(edgeRead+1)%32;edgeCount--;}
      } else if(!strcmp(c.op,"transport_switch")){
        bleSnapshotDirty=true;
        const char* error;
        if(networkPending)error="busy";
        else if(strcmp(c.target,"wifi")&&strcmp(c.target,"ble"))error="invalid_target";
        else error=requestTransportSwitch(!strcmp(c.target,"ble")?TransportMode::BLE:TransportMode::WiFi,"mac",c.requestId);
        snprintf(switchError,sizeof(switchError),"%s",error?error:"");snprintf(rejectedRequest,sizeof(rejectedRequest),"%s",error?c.requestId:"");
      } else if(!strcmp(c.op,"transport_commit")){
        bleSnapshotDirty=true;
        if(!transportCommit(c.requestId)){strcpy(switchError,"commit_rejected");strcpy(rejectedRequest,c.requestId);}
        else{switchError[0]=rejectedRequest[0]=0;}
      } else if(!strcmp(c.op,"start")) {
        // An explicit start from the Mac is also a wake action. A sleeping
        // companion must not silently ignore the user's meeting controls.
        if(voice.allowed&&!voice.muted&&!voice.busy&&voice.peer){
          uiWakeForVoice();voice.sleeping=false;
          soundCancel();audioTick(true);voice.captureReady=audioReady();
          voice.startManual(M5.BtnA.isPressed());
        }
      }
      else if(!strcmp(c.op,"stop"))voice.stop();
      else if(!strcmp(c.op,"sound_preview"))soundRequest(SoundCue::Preview);
      else if(!strcmp(c.op,"calibrate_inverted")||!strcmp(c.op,"calibrate_upright")||!strcmp(c.op,"calibrate_faceup")){
        bleSnapshotDirty=true;
        auto a=M5.Imu.getImuData().accel;
        bool calibrated;
        if(!strcmp(c.op,"calibrate_inverted"))calibrated=pose.calibrateInverted(a.x,a.y,a.z);
        else if(!strcmp(c.op,"calibrate_upright"))calibrated=pose.calibrateUpright(a.x,a.y,a.z);
        else calibrated=pose.calibrateFaceUp(a.x,a.y,a.z);
        if(calibrated){
          poseVoice.calibrated(voice,pose.stable);persist();
        }
        calibrationSeq++;strcpy(calibrationOp,c.op);calibrationOK=calibrated;
        Serial.printf("[pose] %s %s a %.2f %.2f %.2f upright %d faceup %d\n",c.op,calibrated?"ok":"rejected",a.x,a.y,a.z,pose.uprightSign,pose.faceUpSign);
      }
    }
  }
  if(transportMode()==TransportMode::BLE?(!bleConnected()||!bleAuthenticated):WiFi.status()!=WL_CONNECTED){voice.setPeer(false,ms);controlLease.reset();}
  voice.busy=busy||networkPending||transportBusy();
  voice.tick(ms);
  // Prepare capture after waking, before consuming the inverted gesture. If
  // Mic startup needs a retry, VoiceState retains only this fresh gesture.
  // Reserve audio as soon as an inverted gesture is forming, before pose
  // confirmation. A manual start cancels effects above before starting capture.
  const bool voiceReserved=voice.streaming||voice.pendingGesture||
    (!uiLandscapeActive()&&voice.voiceEnabled()&&(pose.stable==DevicePose::Inverted||pose.candidate==DevicePose::Inverted||displayOrientation.rotation==2));
  soundTick(millis(),voiceReserved||voice.busy||networkPending||devicePairWindow());
  audioTick(voice.captureWanted()&&!soundPlaying());voice.captureReady=audioReady();
  poseVoice.update(voice,pose.stable,uiButtonAForVoice(),uiLandscapeActive(),pose.candidate==pose.stable);
  voice.button(uiButtonAForVoice() && !(displayOrientation.rotation&1));
  voice.tick(ms);
  static DevicePose loggedPose=DevicePose::Other;
  if(pose.stable!=loggedPose){loggedPose=pose.stable;Serial.printf("[pose] %s confirmed %ums live %d block %s\n",poseName(loggedPose),unsigned(ms-pose.since),voice.streaming,voiceBlock());}
  static unsigned bleBlock=0;static uint8_t blePacket[196];static int imaIndex=32;
  if(voice.streaming!=lastStreaming){
    bleBlock=0;
    Serial.printf("[voice] %s pose %s manual %d ready %d reason %s\n",voice.streaming?"start":"stop",poseName(pose.stable),voice.manual,voice.captureReady,voiceBlock());
    if(voice.streaming){esp_fill_random(nonce,sizeof(nonce));seq=clockSample=0;}
  }
  if(voice.held!=lastHeld){edge(voice.held);lastHeld=voice.held;}
  lastStreaming=voice.streaming;
  static int16_t pcm[480];
  // A slow network must not keep an independently refilled capture queue
  // nonempty forever and starve buttons, control leases, or OTA cancellation.
  for(unsigned drained=0;drained<4&&audioTake(pcm);++drained) {
    if(!voice.streaming)continue;
    const unsigned count=transportMode()==TransportMode::BLE?160:480;
    static uint8_t packet[988];memcpy(packet,count==160?"TCB2":"TCM1",4);memcpy(packet+4,nonce,16);
    const uint32_t nseq=htonl(seq++),nclock=htonl(clockSample);clockSample+=count;
    memcpy(packet+20,&nseq,4);memcpy(packet+24,&nclock,4);
    for(unsigned i=0;i<count;i++){int32_t s=lroundf(pcm[i]*gain);s=std::max(-32768,std::min(32767,int(s)));pcm[i]=s;packet[28+i*2]=s&255;packet[29+i*2]=(s>>8)&255;}
    if(count==160){
      if(!bleBlock){memcpy(blePacket,"TCB3",4);memcpy(blePacket+4,packet+4,24);}
      ThinCubeIMA::encode160(pcm,blePacket+28+bleBlock*84,imaIndex);
      if(++bleBlock==2){bleSendAudio(blePacket,sizeof(blePacket));bleBlock=0;}
    }
    else{udp.beginPacket(outputIP,outputPort);udp.write(packet,sizeof(packet));udp.endPacket();}
  }
  static uint32_t lastPublish=0;
  static SwitchPhase lastTransportPhase=SwitchPhase::Stable;
  if(lastTransportPhase!=transportRecord().phase){lastTransportPhase=transportRecord().phase;bleSnapshotDirty=true;}
  if(ms-lastPublish>=40||(bleAuthenticated&&bleSnapshotDirty)){publish();lastPublish=ms;}
  static uint32_t lastBleStatus=0;
  if(bleAuthenticated){
    unsigned flags=unsigned(voice.allowed)|(unsigned(voice.muted)<<1)|(unsigned(voice.localSound)<<2)|
      (unsigned(voice.streaming)<<3)|(unsigned(voice.manual)<<4)|(unsigned(voice.held)<<5)|
      (unsigned(voice.captureReady)<<6)|(unsigned(voice.inverted)<<7);
    const uint32_t currentEdge=edgeCount?edges[edgeRead].seq:0;
    static unsigned lastFlags=~0u;static uint32_t lastEdge=~0u;
    if(ms-lastBleStatus>=200||flags!=lastFlags||currentEdge!=lastEdge){
    char cookie[33];for(int i=0;i<16;i++)snprintf(cookie+i*2,3,"%02x",nonce[i]);
    static char live[384];char edgeJSON[64];
    if(edgeCount)snprintf(edgeJSON,sizeof(edgeJSON),"{\"seq\":%u,\"down\":%s}",unsigned(edges[edgeRead].seq),edges[edgeRead].down?"true":"false");
    else strcpy(edgeJSON,"null");
    snprintf(live,sizeof(live),"{\"s\":[%u,%u,\"%s\",%.4f,\"%s\",\"%s\",%u],\"edge\":%s}",unsigned(epoch),flags,cookie,audioLevel(),poseName(pose.stable),voiceBlock(),unsigned(bleAudioDropped()),edgeJSON);
    bleSendStatus(live);lastBleStatus=ms;lastFlags=flags;lastEdge=currentEdge;
    }
  }
  static uint32_t lastBleSnapshot=0;
  if(bleAuthenticated&&(bleSnapshotDirty||(!voice.streaming&&ms-lastBleSnapshot>=5000))){bleSendSnapshot(published);lastBleSnapshot=ms;bleSnapshotDirty=false;}
  if(!voice.streaming&&transportMode()==TransportMode::BLE)bleClearAudio();
}
