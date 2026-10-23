#include "transport.h"
#include "wifi_prov.h"
#include "ota.h"
#include "maintenance.h"
#include "device.h"
#include "audio_input.h"
#include "sound_effects.h"
#include "deep_sleep.h"
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <Arduino.h>
#include <atomic>
namespace {
TransportRecord record;
uint32_t rebootAt=0;
bool storageOK=true;
bool hasChoice=false;
std::mutex admission;
std::atomic<bool> busyPublished{false};
bool save(const TransportRecord& value){
  Preferences p;if(!p.begin("tcmode",false))return false;
  const bool ok=p.putBytes("transaction",&value,sizeof(value))==sizeof(value);p.end();return ok;
}
}
void transportBegin(){
  Preferences p;if(p.begin("tcmode",true)){
    if(p.getBytesLength("transaction")==sizeof(record))hasChoice=p.getBytes("transaction",&record,sizeof(record))==sizeof(record)&&record.valid();p.end();
  }
  if(!record.valid())record=TransportRecord{};
  const auto before=record.phase;record.boot();
  if(record.phase!=before)storageOK=save(record);
}
void transportStarted(){busyPublished=record.busy();}
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
  const bool leaveSetup=provCanLeave(provStage())&&target==TransportMode::BLE&&!strcmp(source,"button");
  if((provActive()&&!leaveSetup)||otaActive()||maintenanceActive()||maintenancePending()||transportBusy()||deepSleepPending())return "busy";
  if(!transportBootVerified())return "boot_unverified";
  if(target==transportMode())return "already_active";
  // Wi-Fi without saved credentials starts provisioning after reboot.
  auto next=record;if(!next.begin(target,id,source))return "invalid_request";
  if(!save(next))return "storage_failed";
  if(leaveSetup)provLeave();
  record=next;busyPublished=true;devicePause();soundCancel();audioTick(false);
  rebootAt=millis()+700;return nullptr;
}

bool transportHasChoice(){return connectionChoiceSaved(hasChoice,record);}
bool transportSetMode(TransportMode mode){
  TransportRecord next;next.mode=next.previous=mode;strcpy(next.source,"setup");
  if(!save(next))return false;record=next;hasChoice=true;return true;
}
bool transportScheduleRestart(TransportMode mode,const char* source,const char* id){
  if(!id||!id[0]||strlen(id)>36||!source||strlen(source)>8)return false;
  auto next=record;next.previous=transportMode();next.mode=mode;next.phase=SwitchPhase::Pending;
  strcpy(next.request,id);strcpy(next.source,source);
  if(!save(next))return false;
  record=next;hasChoice=true;busyPublished=true;devicePause();soundCancel();audioTick(false);
  rebootAt=millis()+700;return true;
}
bool transportCommit(const char* id){
  auto next=record;if(!next.commit(id))return false;
  if(record.phase==SwitchPhase::Committed)return true;
  if(!save(next))return false;record=next;busyPublished=false;return true;
}
void transportTick(uint32_t now){
  if(rebootAt&&int32_t(now-rebootAt)>=0){Serial.printf("[transport] reboot %s %s\n",transportName(record.mode),switchName(record.phase));ESP.restart();}
}

void transportProvisioningOverride(){
  // An intentional boot-time B hold remains the physical Wi-Fi recovery path.
  record=TransportRecord{};storageOK=save(record);
}
