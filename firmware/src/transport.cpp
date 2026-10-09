#include "transport.h"
#include "wifi_prov.h"
#include "ota.h"
#include "device.h"
#include "audio_input.h"
#include "sound_effects.h"
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <Arduino.h>
#include <atomic>
namespace {
TransportRecord record;
uint32_t started=0,rebootAt=0;
bool storageOK=true;
std::mutex admission;
std::atomic<bool> busyPublished{false};
bool save(const TransportRecord& value){
  Preferences p;if(!p.begin("tcmode",false))return false;
  const bool ok=p.putBytes("transaction",&value,sizeof(value))==sizeof(value);p.end();return ok;
}
}
void transportBegin(){
  Preferences p;if(p.begin("tcmode",true)){
    if(p.getBytesLength("transaction")==sizeof(record))p.getBytes("transaction",&record,sizeof(record));p.end();
  }
  if(!record.valid())record=TransportRecord{};
  const auto before=record.phase;record.boot();
  if(record.phase!=before&&!save(record)){
    record.rollback();storageOK=save(record);
  }
}
void transportStarted(uint32_t now){started=now;busyPublished=record.busy();}
std::mutex& transportAdmission(){return admission;}
TransportMode transportMode(){return record.phase==SwitchPhase::Pending?record.previous:record.mode;}
const TransportRecord& transportRecord(){return record;}
bool transportBusy(){return busyPublished.load();}
bool transportBootVerified(){esp_ota_img_states_t s;return esp_ota_get_state_partition(esp_ota_get_running_partition(),&s)!=ESP_OK||s!=ESP_OTA_IMG_PENDING_VERIFY;}
const char* requestTransportSwitch(TransportMode target,const char* source,const char* id){
  std::unique_lock<std::mutex> lock(admission,std::try_to_lock);
  if(!lock.owns_lock())return "busy";
  if(!id||!id[0]||strlen(id)>36)return "invalid_request";
  if(!strcmp(record.request,id))return record.mode==target&&record.phase!=SwitchPhase::RolledBack?nullptr:"request_conflict";
  if(!storageOK)return "storage_failed";
  if(provActive()||otaActive()||transportBusy())return "busy";
  if(!transportBootVerified())return "boot_unverified";
  if(target==transportMode())return "already_active";
  char ssid[33],pass[65];
  if(target==TransportMode::WiFi&&!provLoadCreds(ssid,sizeof(ssid),pass,sizeof(pass)))return "wifi_unconfigured";
  memset(pass,0,sizeof(pass));
  auto next=record;if(!next.begin(target,id,source))return "invalid_request";
  if(!save(next))return "storage_failed";
  record=next;busyPublished=true;deviceStop();soundCancel();audioTick(false);
  rebootAt=millis()+700;return nullptr;
}
bool transportCommit(const char* id){
  auto next=record;if(!next.commit(id))return false;
  if(record.phase==SwitchPhase::Committed)return true;
  if(!save(next))return false;record=next;busyPublished=false;return true;
}
void transportTick(uint32_t now){
  if(record.timedOut(now,started)){
    auto next=record;next.rollback();
    // A retained trial will also roll back at the next boot if the write fails.
    save(next);record=next;busyPublished=true;deviceStop();audioTick(false);rebootAt=now+300;
  }
  if(rebootAt&&int32_t(now-rebootAt)>=0){Serial.printf("[transport] reboot %s %s\n",transportName(record.mode),switchName(record.phase));ESP.restart();}
}

void transportProvisioningOverride(){
  // An intentional boot-time B hold remains the physical Wi-Fi recovery path.
  record=TransportRecord{};storageOK=save(record);
}
