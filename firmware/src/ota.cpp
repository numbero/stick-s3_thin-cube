#include "imu_service.h"
#include "ota.h"
#include "device.h"
#include "transport.h"
#include "ui.h"
#include "power_status.h"
#include "deep_sleep.h"
#include "wifi_prov.h"
#include "maintenance.h"
#include "connection_setup.h"
#include "ota_image_policy.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <M5Unified.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <mbedtls/sha256.h>
#include <cJSON.h>
#include <atomic>
#include <mutex>
#include <algorithm>

// Arduino 2.0.17 otherwise confirms PENDING_VERIFY inside initArduino(), before setup().
// Strong C linkage overrides its weak hook; our 60-second local self-test owns confirmation.
extern "C" bool verifyRollbackLater(){return true;}

namespace {
std::atomic<OtaStage> stage{OTA_IDLE};
std::atomic<bool> cancel{false};
std::atomic<uint32_t> written{0};
std::atomic<uint32_t> sessionGeneration{0};
std::mutex guard;
char token[33]{}, version[32]{}, error[40]{}, digest[65]{};
uint32_t size=0, since=0, doneAt=0, batteryAt=0;
int battery=-1;
httpd_handle_t server=nullptr;
bool started=false;
OtaBootNotice bootNotice=OtaBootNotice::None;

bool authorized(httpd_req_t* r){
  return maintenanceActive()?maintenanceReady()&&maintenanceAuthorized(r):deviceAuthorized(r);
}

esp_err_t reply(httpd_req_t* r,const char* value,const char* status="200 OK") {
  httpd_resp_set_status(r,status);httpd_resp_set_type(r,"application/json");
  httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_send(r,value,HTTPD_RESP_USE_STRLEN);
}
void fail(const char* why) {
  std::lock_guard<std::mutex> lock(guard);snprintf(error,sizeof(error),"%s",why);stage=OTA_FAIL;
}
esp_err_t status(httpd_req_t* r) {
  if(!authorized(r))return reply(r,"{\"error\":\"pairing_required\"}","401 Unauthorized");
  std::lock_guard<std::mutex> lock(guard);char b[160];
  snprintf(b,sizeof(b),"{\"stage\":\"%s\",\"pct\":%d,\"err\":\"%s\"}",otaStageName(stage),size?int(uint64_t(written)*100/size):0,error);
  return reply(r,b);
}
esp_err_t begin(httpd_req_t* r) {
  if(!authorized(r))return reply(r,"{\"error\":\"pairing_required\"}","401 Unauthorized");
  if(r->content_len<2||r->content_len>512)return reply(r,"{\"error\":\"bad_request\"}","400 Bad Request");
  char b[513];size_t n=0;
  while(n<r->content_len){int got=httpd_req_recv(r,b+n,r->content_len-n);if(got<=0)return ESP_FAIL;n+=got;}b[n]=0;
  cJSON* j=cJSON_Parse(b);auto s=cJSON_GetObjectItemCaseSensitive(j,"size");auto h=cJSON_GetObjectItemCaseSensitive(j,"sha256");auto v=cJSON_GetObjectItemCaseSensitive(j,"ver");
  bool valid=cJSON_IsNumber(s)&&s->valuedouble>=OtaImagePolicy::MinimumSize&&s->valuedouble<=OtaImagePolicy::MaximumSize&&s->valuedouble==s->valueint&&cJSON_IsString(h)&&strlen(h->valuestring)==64&&cJSON_IsString(v)&&OtaImagePolicy::versionValid(v->valuestring);
  if(valid)for(int i=0;i<64;i++)if(!isxdigit(static_cast<unsigned char>(h->valuestring[i])))valid=false;
  if(!valid){cJSON_Delete(j);return reply(r,"{\"error\":\"invalid_manifest\"}","400 Bad Request");}
  std::lock_guard<std::mutex> admission(transportAdmission());
  std::lock_guard<std::mutex> lock(guard);
  if(maintenancePending()||connectionSetupActive()||provActive()){cJSON_Delete(j);return reply(r,"{\"error\":\"busy\"}","409 Conflict");}
  if(powerCalibrationOwnsUI()){cJSON_Delete(j);return reply(r,"{\"error\":\"battery_calibration_active\"}","409 Conflict");}
  if(deepSleepPending()){cJSON_Delete(j);return reply(r,"{\"error\":\"sleep_pending\"}","409 Conflict");}
  if(transportBusy()){cJSON_Delete(j);return reply(r,"{\"error\":\"transport_switch_pending\"}","409 Conflict");}
  if(stage!=OTA_IDLE&&stage!=OTA_FAIL){cJSON_Delete(j);return reply(r,"{\"error\":\"busy\"}","409 Conflict");}
  if(battery<50||millis()-batteryAt>15000){cJSON_Delete(j);return reply(r,"{\"error\":\"battery_must_be_known_and_at_least_50\"}","409 Conflict");}
  auto slot=esp_ota_get_next_update_partition(nullptr);
  if(!slot||s->valueint>int(slot->size)){cJSON_Delete(j);return reply(r,"{\"error\":\"no_compatible_slot\"}","409 Conflict");}
  esp_ota_img_states_t state;
  if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY){cJSON_Delete(j);return reply(r,"{\"error\":\"boot_health_pending\"}","409 Conflict");}
  size=s->valueint;strcpy(digest,h->valuestring);strcpy(version,v->valuestring);cJSON_Delete(j);
  for(int i=0;i<64;i++)digest[i]=tolower(digest[i]);
  for(int i=0;i<16;i++)snprintf(token+i*2,3,"%02x",uint8_t(esp_random()));
  written=0;error[0]=0;cancel=false;since=millis();++sessionGeneration;stage=OTA_CONFIRM;
  if(maintenanceActive())maintenanceTouch();
  char response[100];snprintf(response,sizeof(response),"{\"ok\":true,\"token\":\"%s\"}",token);return reply(r,response);
}
esp_err_t upload(httpd_req_t* r) {
  if(!authorized(r))return reply(r,"{\"error\":\"pairing_required\"}","401 Unauthorized");
  char supplied[33]{};uint32_t expectedSize;char expectedDigest[65],expectedVersion[32];
  {
    std::lock_guard<std::mutex> lock(guard);
    if(httpd_req_get_hdr_value_str(r,"X-ThinCube-OTA",supplied,sizeof(supplied))!=ESP_OK||!token[0]||strcmp(supplied,token))return reply(r,"{\"error\":\"token\"}","401 Unauthorized");
    if(stage!=OTA_READY)return reply(r,"{\"error\":\"confirm_on_device\"}","409 Conflict");
    if(r->content_len!=size)return reply(r,"{\"error\":\"exact_length_required\"}","400 Bad Request");
    expectedSize=size;strcpy(expectedDigest,digest);strcpy(expectedVersion,version);stage=OTA_RECV;
  }
  if(maintenanceActive())maintenanceTouch();
  // This HTTP worker alone owns the OTA handle. Cancellation only sets a flag.
  esp_ota_handle_t handle=0;auto slot=esp_ota_get_next_update_partition(nullptr);
  mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts_ret(&sha,0);
  const char* reason=nullptr;uint8_t chunk[4096];size_t remaining=expectedSize;uint32_t start=millis(),lastData=start;
  bool first=true;
  while(remaining&&!reason) {
    if(cancel){reason="cancelled";break;}
    if(millis()-start>120000||millis()-lastData>10000){reason="upload_timeout";break;}
    size_t count=0,want=std::min(remaining,sizeof(chunk));
    while(count<want) {
      int got=httpd_req_recv(r,reinterpret_cast<char*>(chunk+count),want-count);
      if(got==HTTPD_SOCK_ERR_TIMEOUT){if(millis()-lastData>10000){reason="receive_timeout";break;}continue;}
      if(got<=0){reason="connection_lost";break;}
      count+=got;lastData=millis();
      if(cancel||millis()-start>120000){reason="cancelled_or_timeout";break;}
    }
    if(reason)break;
    if(first) {
      if(!OtaImagePolicy::prefixValid(chunk,count,expectedVersion)){reason="wrong_device_image";break;}
      if(!slot||esp_ota_begin(slot,expectedSize,&handle)!=ESP_OK){reason="erase_failed";break;}
      first=false;
    }
    if(esp_ota_write(handle,chunk,count)!=ESP_OK){reason="write_failed";break;}
    mbedtls_sha256_update_ret(&sha,chunk,count);remaining-=count;written+=count;
    if(maintenanceActive())maintenanceTouch();
  }
  unsigned char hash[32];mbedtls_sha256_finish_ret(&sha,hash);mbedtls_sha256_free(&sha);
  char hex[65];for(int i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",hash[i]);
  if(!reason&&strcmp(hex,expectedDigest))reason="sha256_mismatch";
  if(!reason){
    // Cancellation and the commit boundary share a lock: once VERIFY begins,
    // a button/HTTP cancellation must not promise an abort that cannot happen.
    std::lock_guard<std::mutex> lock(guard);
    if(cancel)reason="cancelled";else stage=OTA_VERIFY;
  }
  if(reason){if(handle)esp_ota_abort(handle);fail(reason);char response[96];snprintf(response,sizeof(response),"{\"ok\":false,\"error\":\"%s\"}",reason);return reply(r,response,"400 Bad Request");}
  esp_err_t result=esp_ota_end(handle);handle=0;
  if(result!=ESP_OK){fail("image_verification_failed");return reply(r,"{\"ok\":false}","400 Bad Request");}
  if(esp_ota_set_boot_partition(slot)!=ESP_OK){fail("set_boot_failed");return reply(r,"{\"ok\":false}","500 Internal Server Error");}
  {std::lock_guard<std::mutex> lock(guard);doneAt=millis();stage=OTA_DONE;token[0]=0;}
  return reply(r,"{\"ok\":true}");
}
esp_err_t cancelUpload(httpd_req_t* r){
  if(!authorized(r))return reply(r,"{\"error\":\"pairing_required\"}","401 Unauthorized");
  if(!otaCancel())return reply(r,"{\"error\":\"cannot_cancel\"}","409 Conflict");
  return reply(r,"{\"ok\":true}");
}
bool startServer() {
  httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.max_uri_handlers=16;cfg.stack_size=12288;
  cfg.max_open_sockets=5;cfg.lru_purge_enable=true;cfg.recv_wait_timeout=5;cfg.send_wait_timeout=5;
  if(httpd_start(&server,&cfg)!=ESP_OK)return false;
  if(!otaRegisterRoutes(server)){httpd_stop(server);server=nullptr;return false;}
  deviceRegister(server);return true;
}
}

bool otaRegisterRoutes(httpd_handle_t target){
  const httpd_uri_t routes[]={
    {.uri="/ota/begin",.method=HTTP_POST,.handler=begin},
    {.uri="/ota/status",.method=HTTP_GET,.handler=status},
    {.uri="/ota",.method=HTTP_POST,.handler=upload},
    {.uri="/ota/cancel",.method=HTTP_POST,.handler=cancelUpload},
  };
  for(auto& route:routes)if(httpd_register_uri_handler(target,&route)!=ESP_OK)return false;
  return true;
}

const char* otaStageName(OtaStage s){switch(s){case OTA_CONFIRM:return "confirm";case OTA_READY:return "ready";case OTA_RECV:return "receiving";case OTA_VERIFY:return "verifying";case OTA_DONE:return "done";case OTA_FAIL:return "failed";default:return "idle";}}
bool otaActive(){return stage!=OTA_IDLE;}
bool otaConnected(){return WiFi.status()==WL_CONNECTED;}
OtaStage otaStage(){return stage;}
const char* otaTargetVer(){static char b[32];std::lock_guard<std::mutex> lock(guard);strcpy(b,version);return b;}
const char* otaLastErr(){static char b[40];std::lock_guard<std::mutex> lock(guard);strcpy(b,error);return b;}
int otaPct(){std::lock_guard<std::mutex> lock(guard);return size?std::min(100,int(uint64_t(written)*100/size)):0;}
uint32_t otaSessionGeneration(){return sessionGeneration.load();}
void otaConfirm(uint32_t expectedGeneration){std::lock_guard<std::mutex> lock(guard);if(stage==OTA_CONFIRM&&sessionGeneration==expectedGeneration){since=millis();stage=OTA_READY;}}
bool otaCancel(){
  std::lock_guard<std::mutex> lock(guard);
  const OtaStage current=stage;
  if(current==OTA_VERIFY||current==OTA_DONE)return false;
  cancel=true;
  if(current==OTA_CONFIRM||current==OTA_READY){strcpy(error,"cancelled");token[0]=0;stage=OTA_FAIL;}
  return true;
}
void otaDismiss(){std::lock_guard<std::mutex> lock(guard);if(stage==OTA_FAIL){stage=OTA_IDLE;error[0]=token[0]=0;}}
int otaConfirmLeftSec(){std::lock_guard<std::mutex> lock(guard);return stage==OTA_CONFIRM?std::max(0,30-int((millis()-since)/1000)):0;}
void otaSetBattery(int level){std::lock_guard<std::mutex> lock(guard);battery=level;batteryAt=millis();}
void otaBootBegin(){
  esp_ota_img_states_t state;
  bootNotice=esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY
    ?OtaBootNotice::Checking:OtaBootNotice::None;
}
OtaBootNotice otaBootNotice(){return bootNotice;}
uint32_t otaBootElapsedSec(){return millis()/1000;}
void otaBootDismiss(){if(bootNotice==OtaBootNotice::Complete)bootNotice=OtaBootNotice::None;}
bool otaMarkValid(){
  if(!deviceBootHealthy()||!imuHealthy(millis())||millis()<60000)return false;
  esp_ota_img_states_t s;
  if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&s)!=ESP_OK)return false;
  if(s==ESP_OTA_IMG_PENDING_VERIFY){
    const bool ok=esp_ota_mark_app_valid_cancel_rollback()==ESP_OK;
    if(ok){
      // Only the successful IDF confirmation can announce completion. A
      // normal VALID boot or a failed mark must never imply an OTA succeeded.
      bootNotice=OtaBootNotice::Complete;
      Serial.println("[ota] local health passed; boot marked valid after 60s");
    }
    return ok;
  }
  return true;
}
void otaStopServer(){if(server){httpd_stop(server);server=nullptr;}started=false;MDNS.end();}
void otaCloseControlConnection(int fd){
  if(!server||fd<0)return;
  const auto result=httpd_sess_trigger_close(server,fd);
  Serial.printf("[control] close revoked Wi-Fi session fd %d result %d\n",fd,int(result));
}
void otaRollbackUnhealthy(){
  esp_ota_img_states_t state;
  if(millis()>90000&&esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY)
    esp_ota_mark_app_invalid_rollback_and_reboot();
}
void otaTick(uint32_t ms){
  if(!started&&!maintenanceActive()&&!maintenancePending()&&transportMode()==TransportMode::WiFi&&!provActive()&&WiFi.status()==WL_CONNECTED){started=startServer();if(started)uiLog("ip %s",WiFi.localIP().toString().c_str());}
  static OtaStage prev=OTA_IDLE;const OtaStage now=stage;
  if(now!=prev){uiLog("ota %s",otaStageName(now));prev=now;}
  bool reboot=false;
  {
    std::lock_guard<std::mutex> lock(guard);
    // The HTTP task can set since/doneAt after this loop captured ms. Signed
    // deltas avoid interpreting that newer timestamp as a 49-day timeout.
    if((stage==OTA_CONFIRM||stage==OTA_READY)&&int32_t(ms-since)>30000){
      strcpy(error,"timeout");token[0]=0;stage=OTA_FAIL;
    }
    reboot=stage==OTA_DONE&&int32_t(ms-doneAt)>2000;
  }
  if(reboot)ESP.restart();
}
