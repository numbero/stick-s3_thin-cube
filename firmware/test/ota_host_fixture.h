#pragma once
// Minimal deterministic port for compiling the unchanged ota.cpp on a CPU.
// Crypto emits a controlled digest: actual SHA vectors are covered separately
// by test_phone_update_page.js. This fixture tests worker decisions/side effects.
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <vector>

using esp_err_t=int;
using esp_ota_handle_t=unsigned;
using httpd_handle_t=void*;
constexpr int ESP_OK=0,ESP_FAIL=-1,HTTPD_RESP_USE_STRLEN=-1;
constexpr int HTTPD_SOCK_ERR_TIMEOUT=-2,HTTP_GET=0,HTTP_POST=1,WL_CONNECTED=3;
struct httpd_req_t {
  size_t content_len=0,cursor=0,maxChunk=SIZE_MAX,disconnectAt=SIZE_MAX;
  std::vector<uint8_t> body;
  std::map<std::string,std::string> headers;
  std::string responseStatus="200 OK",response;
  unsigned receiveCalls=0,timeoutReads=0,receiveMs=1;
  std::function<void(httpd_req_t&)> beforeReceive;
};
struct httpd_config_t {
  int max_uri_handlers=0,stack_size=0,max_open_sockets=0;
  bool lru_purge_enable=false;
  int recv_wait_timeout=0,send_wait_timeout=0;
};
#define HTTPD_DEFAULT_CONFIG() httpd_config_t{}
struct httpd_uri_t {const char* uri;int method;esp_err_t (*handler)(httpd_req_t*);};
#include "ota.h"

enum class TransportMode {WiFi,BLE};
enum esp_ota_img_states_t {ESP_OTA_IMG_VALID,ESP_OTA_IMG_PENDING_VERIFY,ESP_OTA_IMG_INVALID};
struct esp_partition_t {const char* label;uint32_t address,size;};
inline const esp_partition_t runningPartition={"ota_0",0x10000,2097152};
inline const esp_partition_t sparePartition={"ota_1",0x210000,2097152};

struct OtaHost {
  uint32_t clock=100000;
  std::string runningVersion="v0.4.64";
  bool paired=true,maintenance=false,maintenanceReady=true,maintenanceAuthorized=true;
  bool maintenancePending=false,setup=false,provisioning=false,calibration=false,sleep=false;
  bool switching=false,connected=false,localHealthy=true,imuHealthy=true,displayReady=true,haveSpare=true;
  bool failErase=false,failEnd=false,failSetBoot=false,failMarkValid=false,failGetState=false;
  unsigned failWriteAt=0,erases=0,writeCalls=0,ends=0,aborts=0,commits=0;
  unsigned restarts=0,markValidCalls=0,rollbackCalls=0,touches=0,servers=0;
  bool handleOpen=false;
  esp_ota_img_states_t imageState=ESP_OTA_IMG_VALID;
  const esp_partition_t* bootPartition=&runningPartition;
  std::vector<uint8_t> flash,hashed;
  std::vector<std::string> effects;
  std::array<uint8_t,32> hash{};
  std::function<void()> onShaFinish,onEnd,onWrite;
  OtaHost(){for(size_t i=0;i<hash.size();++i)hash[i]=uint8_t(i*7+3);}
};
inline OtaHost host;
inline std::mutex admissionMutex;
inline uint32_t millis(){return host.clock;}
inline const char* firmwareVersion(){return host.runningVersion.c_str();}
inline uint32_t esp_random(){return 0xabcdef12;}
inline bool maintenanceActive(){return host.maintenance;}
inline bool maintenanceReady(){return host.maintenanceReady;}
inline bool maintenanceAuthorized(httpd_req_t*){return host.maintenanceAuthorized;}
inline bool maintenancePending(){return host.maintenancePending;}
inline void maintenanceTouch(){++host.touches;}
inline bool deviceAuthorized(httpd_req_t*){return host.paired;}
inline bool deviceBootHealthy(){return host.localHealthy;}
inline void deviceRegister(httpd_handle_t){}
inline bool imuHealthy(uint32_t){return host.imuHealthy;}
inline bool uiStartupReady(){return host.displayReady;}
inline bool connectionSetupActive(){return host.setup;}
inline bool provActive(){return host.provisioning;}
inline bool powerCalibrationOwnsUI(){return host.calibration;}
inline bool deepSleepPending(){return host.sleep;}
inline bool transportBusy(){return host.switching;}
inline TransportMode transportMode(){return TransportMode::WiFi;}
inline std::mutex& transportAdmission(){return admissionMutex;}
inline void uiLog(const char*,...){}
struct HostIP {std::string toString() const{return "192.168.4.1";}};
struct HostWiFi {int status() const{return host.connected?WL_CONNECTED:0;}HostIP localIP() const{return {};}};
inline HostWiFi WiFi;
struct HostMDNS {void end(){}};
inline HostMDNS MDNS;
struct HostESP {void restart(){++host.restarts;}};
inline HostESP ESP;
struct HostSerial {void println(const char*){}void printf(const char*,...) {}};
inline HostSerial Serial;

inline esp_err_t httpd_resp_set_status(httpd_req_t* request,const char* value){request->responseStatus=value;return ESP_OK;}
inline esp_err_t httpd_resp_set_type(httpd_req_t*,const char*){return ESP_OK;}
inline esp_err_t httpd_resp_set_hdr(httpd_req_t*,const char*,const char*){return ESP_OK;}
inline esp_err_t httpd_resp_send(httpd_req_t* request,const char* value,int length){request->response.assign(value,length==HTTPD_RESP_USE_STRLEN?std::strlen(value):size_t(length));return ESP_OK;}
inline int httpd_req_get_hdr_value_str(httpd_req_t* request,const char* key,char* out,size_t capacity){
  const auto found=request->headers.find(key);
  if(found==request->headers.end()||found->second.size()>=capacity)return ESP_FAIL;
  std::memcpy(out,found->second.c_str(),found->second.size()+1);return ESP_OK;
}
inline int httpd_req_recv(httpd_req_t* request,char* out,size_t count){
  ++request->receiveCalls;host.clock+=request->receiveMs;
  if(request->beforeReceive)request->beforeReceive(*request);
  if(request->timeoutReads){--request->timeoutReads;return HTTPD_SOCK_ERR_TIMEOUT;}
  if(request->cursor>=request->disconnectAt||request->cursor>=request->body.size())return ESP_FAIL;
  count=std::min({count,request->body.size()-request->cursor,request->maxChunk,request->disconnectAt-request->cursor});
  std::memcpy(out,request->body.data()+request->cursor,count);request->cursor+=count;return int(count);
}
inline esp_err_t httpd_start(httpd_handle_t* handle,httpd_config_t*){++host.servers;*handle=&host;return ESP_OK;}
inline esp_err_t httpd_stop(httpd_handle_t){return ESP_OK;}
inline esp_err_t httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t*){return ESP_OK;}
inline esp_err_t httpd_sess_trigger_close(httpd_handle_t,int){return ESP_OK;}

// The real begin handler still reads JSON from its HTTP request and checks
// every manifest field. This flat-object port only parses the three test fields.
struct cJSON {
  enum Kind {Object,Number,String} kind=Object;
  double valuedouble=0;int valueint=0;
  char* valuestring=nullptr;
  std::string storage;
  std::map<std::string,cJSON*> fields;
};
inline cJSON* cJSON_Parse(const char* input){
  std::string text(input);if(text.empty()||text.front()!='{'||text.back()!='}')return nullptr;
  auto root=new cJSON;
  std::smatch match;
  for(const char* key:{"sha256","ver"}){
    if(std::regex_search(text,match,std::regex(std::string("\"")+key+"\"\\s*:\\s*\"([^\"]*)\""))){
      auto item=new cJSON;item->kind=cJSON::String;item->storage=match[1];item->valuestring=item->storage.data();root->fields[key]=item;
    }
  }
  if(std::regex_search(text,match,std::regex("\"size\"\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)"))){
    auto item=new cJSON;item->kind=cJSON::Number;item->valuedouble=std::stod(match[1]);item->valueint=int(item->valuedouble);root->fields["size"]=item;
  }
  return root;
}
inline cJSON* cJSON_GetObjectItemCaseSensitive(cJSON* value,const char* key){
  if(!value)return nullptr;const auto found=value->fields.find(key);return found==value->fields.end()?nullptr:found->second;
}
inline bool cJSON_IsNumber(cJSON* value){return value&&value->kind==cJSON::Number;}
inline bool cJSON_IsString(cJSON* value){return value&&value->kind==cJSON::String;}
inline void cJSON_Delete(cJSON* value){if(!value)return;for(const auto& field:value->fields)delete field.second;delete value;}

struct mbedtls_sha256_context {};
inline void mbedtls_sha256_init(mbedtls_sha256_context*){host.hashed.clear();}
inline int mbedtls_sha256_starts_ret(mbedtls_sha256_context*,int){return 0;}
inline int mbedtls_sha256_update_ret(mbedtls_sha256_context*,const unsigned char* bytes,size_t count){host.hashed.insert(host.hashed.end(),bytes,bytes+count);return 0;}
inline int mbedtls_sha256_finish_ret(mbedtls_sha256_context*,unsigned char* result){if(host.onShaFinish)host.onShaFinish();std::copy(host.hash.begin(),host.hash.end(),result);return 0;}
inline void mbedtls_sha256_free(mbedtls_sha256_context*){}

inline const esp_partition_t* esp_ota_get_running_partition(){return &runningPartition;}
inline const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*){return host.haveSpare?&sparePartition:nullptr;}
inline esp_err_t esp_ota_get_state_partition(const esp_partition_t* partition,esp_ota_img_states_t* state){assert(partition==&runningPartition);if(host.failGetState)return ESP_FAIL;*state=host.imageState;return ESP_OK;}
inline esp_err_t esp_ota_begin(const esp_partition_t* partition,size_t bytes,esp_ota_handle_t* handle){
  assert(partition==&sparePartition&&bytes<=sparePartition.size&&!host.handleOpen);
  ++host.erases;host.effects.push_back("erase spare");
  if(host.failErase)return ESP_FAIL;
  host.handleOpen=true;*handle=1;return ESP_OK;
}
inline esp_err_t esp_ota_write(esp_ota_handle_t handle,const void* bytes,size_t count){
  assert(handle==1&&host.handleOpen);++host.writeCalls;host.effects.push_back("write spare");
  if(host.onWrite)host.onWrite();
  if(host.failWriteAt==host.writeCalls)return ESP_FAIL;
  const auto data=static_cast<const uint8_t*>(bytes);host.flash.insert(host.flash.end(),data,data+count);return ESP_OK;
}
inline esp_err_t esp_ota_abort(esp_ota_handle_t handle){assert(handle==1&&host.handleOpen);++host.aborts;host.handleOpen=false;host.effects.push_back("abort");return ESP_OK;}
inline esp_err_t esp_ota_end(esp_ota_handle_t handle){
  assert(handle==1&&host.handleOpen);++host.ends;host.effects.push_back("verify");if(host.onEnd)host.onEnd();
  // IDF consumes this handle on both success and failure.
  host.handleOpen=false;return host.failEnd?ESP_FAIL:ESP_OK;
}
inline esp_err_t esp_ota_set_boot_partition(const esp_partition_t* partition){
  assert(partition==&sparePartition&&!host.handleOpen&&host.ends==1&&!host.failEnd);
  ++host.commits;host.effects.push_back("set boot spare");
  if(host.failSetBoot)return ESP_FAIL;
  host.bootPartition=partition;return ESP_OK;
}
inline esp_err_t esp_ota_mark_app_valid_cancel_rollback(){
  ++host.markValidCalls;if(host.failMarkValid)return ESP_FAIL;host.imageState=ESP_OTA_IMG_VALID;return ESP_OK;
}
inline esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(){++host.rollbackCalls;return ESP_OK;}
