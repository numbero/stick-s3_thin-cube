#include "ota.h"
#include "device.h"
#include "transport.h"
#include "ui.h"
#include "power_status.h"
#include "deep_sleep.h"
#include "wifi_prov.h"
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

namespace {
std::atomic<OtaStage> stage{OTA_IDLE};
std::atomic<bool> cancel{false};
std::atomic<uint32_t> written{0};
std::mutex guard;
char token[33]{}, version[32]{}, error[40]{}, digest[65]{};
uint32_t size=0, since=0, doneAt=0, batteryAt=0;
int battery=-1;
httpd_handle_t server=nullptr;
bool started=false;

esp_err_t reply(httpd_req_t* r,const char* value,const char* status="200 OK") {
  httpd_resp_set_status(r,status);httpd_resp_set_type(r,"application/json");
  httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_send(r,value,HTTPD_RESP_USE_STRLEN);
}
void fail(const char* why) {
  std::lock_guard<std::mutex> lock(guard);snprintf(error,sizeof(error),"%s",why);stage=OTA_FAIL;
}
esp_err_t status(httpd_req_t* r) {
  if(!deviceAuthorized(r))return reply(r,"{\"error\":\"pairing_required\"}","401 Unauthorized");
  std::lock_guard<std::mutex> lock(guard);char b[160];
  snprintf(b,sizeof(b),"{\"stage\":\"%s\",\"pct\":%d,\"err\":\"%s\"}",otaStageName(stage),size?int(uint64_t(written)*100/size):0,error);
  return reply(r,b);
}
esp_err_t begin(httpd_req_t* r) {
  if(!deviceAuthorized(r))return reply(r,"{\"error\":\"pairing_required\"}","401 Unauthorized");
  if(r->content_len<2||r->content_len>512)return reply(r,"{\"error\":\"bad_request\"}","400 Bad Request");
  char b[513];size_t n=0;
  while(n<r->content_len){int got=httpd_req_recv(r,b+n,r->content_len-n);if(got<=0)return ESP_FAIL;n+=got;}b[n]=0;
  cJSON* j=cJSON_Parse(b);auto s=cJSON_GetObjectItemCaseSensitive(j,"size");auto h=cJSON_GetObjectItemCaseSensitive(j,"sha256");auto v=cJSON_GetObjectItemCaseSensitive(j,"ver");
  bool valid=cJSON_IsNumber(s)&&s->valuedouble>=352&&s->valuedouble<=2097152&&s->valuedouble==s->valueint&&cJSON_IsString(h)&&strlen(h->valuestring)==64&&cJSON_IsString(v)&&strlen(v->valuestring)<sizeof(version);
  if(valid)for(int i=0;i<64;i++)if(!isxdigit(h->valuestring[i]))valid=false;
  if(!valid){cJSON_Delete(j);return reply(r,"{\"error\":\"invalid_manifest\"}","400 Bad Request");}
  std::lock_guard<std::mutex> admission(transportAdmission());
  std::lock_guard<std::mutex> lock(guard);
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
  written=0;error[0]=0;cancel=false;since=millis();stage=OTA_CONFIRM;
  char response[100];snprintf(response,sizeof(response),"{\"ok\":true,\"token\":\"%s\"}",token);return reply(r,response);
}
esp_err_t upload(httpd_req_t* r) {
  if(!deviceAuthorized(r))return reply(r,"{\"error\":\"pairing_required\"}","401 Unauthorized");
  char supplied[33]{};uint32_t expectedSize;char expectedDigest[65];
  {
    std::lock_guard<std::mutex> lock(guard);
    if(httpd_req_get_hdr_value_str(r,"X-ThinCube-OTA",supplied,sizeof(supplied))!=ESP_OK||!token[0]||strcmp(supplied,token))return reply(r,"{\"error\":\"token\"}","401 Unauthorized");
    if(stage!=OTA_READY)return reply(r,"{\"error\":\"confirm_on_device\"}","409 Conflict");
    if(r->content_len!=size)return reply(r,"{\"error\":\"exact_length_required\"}","400 Bad Request");
    expectedSize=size;strcpy(expectedDigest,digest);stage=OTA_RECV;
  }
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
      const auto* header=reinterpret_cast<const esp_image_header_t*>(chunk);
      const auto* desc=reinterpret_cast<const esp_app_desc_t*>(chunk+sizeof(esp_image_header_t)+sizeof(esp_image_segment_header_t));
      const char identity[32]="ThinCube-StickS3-v1";
      if(count<352||header->magic!=ESP_IMAGE_HEADER_MAGIC||header->chip_id!=9||desc->magic_word!=ESP_APP_DESC_MAGIC_WORD||memcmp(chunk+288,identity,32)){reason="wrong_device_image";break;}
      if(!slot||esp_ota_begin(slot,expectedSize,&handle)!=ESP_OK){reason="erase_failed";break;}
      first=false;
    }
    if(esp_ota_write(handle,chunk,count)!=ESP_OK){reason="write_failed";break;}
    mbedtls_sha256_update_ret(&sha,chunk,count);remaining-=count;written+=count;
  }
  unsigned char hash[32];mbedtls_sha256_finish_ret(&sha,hash);mbedtls_sha256_free(&sha);
  char hex[65];for(int i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",hash[i]);
  if(!reason&&strcmp(hex,expectedDigest))reason="sha256_mismatch";
  if(!reason&&cancel)reason="cancelled";
  if(reason){if(handle)esp_ota_abort(handle);fail(reason);return reply(r,"{\"ok\":false}","400 Bad Request");}
  stage=OTA_VERIFY;
  esp_err_t result=esp_ota_end(handle);handle=0;
  if(result!=ESP_OK){fail("image_verification_failed");return reply(r,"{\"ok\":false}","400 Bad Request");}
  if(esp_ota_set_boot_partition(slot)!=ESP_OK){fail("set_boot_failed");return reply(r,"{\"ok\":false}","500 Internal Server Error");}
  {std::lock_guard<std::mutex> lock(guard);doneAt=millis();stage=OTA_DONE;token[0]=0;}
  return reply(r,"{\"ok\":true}");
}
bool startServer() {
  httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.max_uri_handlers=16;cfg.stack_size=12288;
  cfg.max_open_sockets=5;cfg.lru_purge_enable=true;cfg.recv_wait_timeout=5;cfg.send_wait_timeout=5;
  if(httpd_start(&server,&cfg)!=ESP_OK)return false;
  const httpd_uri_t routes[]={
    {.uri="/ota/begin",.method=HTTP_POST,.handler=begin},
    {.uri="/ota/status",.method=HTTP_GET,.handler=status},
    {.uri="/ota",.method=HTTP_POST,.handler=upload},
  };
  for(auto& route:routes)if(httpd_register_uri_handler(server,&route)!=ESP_OK){httpd_stop(server);server=nullptr;return false;}
  deviceRegister(server);return true;
}
}

const char* otaStageName(OtaStage s){switch(s){case OTA_CONFIRM:return "confirm";case OTA_READY:return "ready";case OTA_RECV:return "receiving";case OTA_VERIFY:return "verifying";case OTA_DONE:return "done";case OTA_FAIL:return "failed";default:return "idle";}}
bool otaActive(){return stage!=OTA_IDLE;}
bool otaConnected(){return WiFi.status()==WL_CONNECTED;}
OtaStage otaStage(){return stage;}
const char* otaTargetVer(){static char b[32];std::lock_guard<std::mutex> lock(guard);strcpy(b,version);return b;}
const char* otaLastErr(){static char b[40];std::lock_guard<std::mutex> lock(guard);strcpy(b,error);return b;}
int otaPct(){std::lock_guard<std::mutex> lock(guard);return size?std::min(100,int(uint64_t(written)*100/size)):0;}
void otaConfirm(){std::lock_guard<std::mutex> lock(guard);if(stage==OTA_CONFIRM){since=millis();stage=OTA_READY;}}
void otaCancel(){cancel=true;if(stage==OTA_CONFIRM||stage==OTA_READY)fail("cancelled");}
void otaDismiss(){std::lock_guard<std::mutex> lock(guard);if(stage==OTA_FAIL){stage=OTA_IDLE;error[0]=token[0]=0;}}
int otaConfirmLeftSec(){std::lock_guard<std::mutex> lock(guard);return stage==OTA_CONFIRM?std::max(0,30-int((millis()-since)/1000)):0;}
void otaSetBattery(int level){std::lock_guard<std::mutex> lock(guard);battery=level;batteryAt=millis();}
bool otaMarkValid(){
  if(!deviceHealthy()||!M5.Imu.isEnabled()||millis()<60000)return false;
  esp_ota_img_states_t s;
  if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&s)==ESP_OK&&s==ESP_OTA_IMG_PENDING_VERIFY)return esp_ota_mark_app_valid_cancel_rollback()==ESP_OK;
  return true;
}
void otaStopServer(){if(server){httpd_stop(server);server=nullptr;}started=false;MDNS.end();}
void otaRollbackUnhealthy(){
  esp_ota_img_states_t state;
  if(millis()>90000&&esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY)
    esp_ota_mark_app_invalid_rollback_and_reboot();
}
void otaTick(uint32_t ms){
  if(!started&&transportMode()==TransportMode::WiFi&&!provActive()&&WiFi.status()==WL_CONNECTED){started=startServer();if(started)uiLog("ip %s",WiFi.localIP().toString().c_str());}
  static OtaStage prev=OTA_IDLE;const OtaStage now=stage;
  if(now!=prev){uiLog("ota %s",otaStageName(now));prev=now;}
  bool timeout=false,reboot=false;
  {std::lock_guard<std::mutex> lock(guard);timeout=(stage==OTA_CONFIRM||stage==OTA_READY)&&ms-since>30000;reboot=stage==OTA_DONE&&ms-doneAt>2000;}
  if(timeout)fail("timeout");
  if(reboot)ESP.restart();
}
