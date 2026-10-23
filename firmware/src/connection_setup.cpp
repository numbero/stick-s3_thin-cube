#include "connection_setup.h"
#include "transport.h"
#include "device.h"
#include "wifi_prov.h"
#include <Preferences.h>
#include <Arduino.h>
#include <nvs.h>
#include <vector>
#include <string>
#include <algorithm>
namespace {
SetupPage page=SetupPage::None;
SetupButtons buttons;
uint32_t rebootAt=0,resetAt=0;
const char* error="";
enum class ResetRequest { None, Network, Factory, Failed };
ResetRequest resetRequest(){
  nvs_handle_t handle;
  const auto opened=nvs_open("tcsetup",NVS_READONLY,&handle);
  if(opened==ESP_ERR_NVS_NOT_FOUND)return ResetRequest::None;
  if(opened!=ESP_OK)return ResetRequest::Failed;
  uint8_t factory=0,network=0;
  const auto f=nvs_get_u8(handle,"factory",&factory),n=nvs_get_u8(handle,"reset",&network);
  nvs_close(handle);
  if((f!=ESP_OK&&f!=ESP_ERR_NVS_NOT_FOUND)||(n!=ESP_OK&&n!=ESP_ERR_NVS_NOT_FOUND))return ResetRequest::Failed;
  return factory?ResetRequest::Factory:network?ResetRequest::Network:ResetRequest::None;
}
bool clearNamespace(const char* name){
  nvs_handle_t handle;if(nvs_open(name,NVS_READWRITE,&handle)!=ESP_OK)return false;
  const bool ok=nvs_erase_all(handle)==ESP_OK&&nvs_commit(handle)==ESP_OK;
  nvs_close(handle);return ok;
}
bool factoryMarker(){
  nvs_handle_t handle;if(nvs_open("tcsetup",NVS_READWRITE,&handle)!=ESP_OK)return false;
  const bool ok=nvs_set_u8(handle,"factory",1)==ESP_OK&&nvs_commit(handle)==ESP_OK;
  nvs_close(handle);return ok;
}
bool finishFactoryJournal(){
  // Erase any other setup keys before removing the recovery marker itself.
  std::vector<std::string> keys;
  auto it=nvs_entry_find("nvs","tcsetup",NVS_TYPE_ANY);
  while(it){
    nvs_entry_info_t info;nvs_entry_info(it,&info);
    if(strcmp(info.key,"factory"))keys.emplace_back(info.key);
    it=nvs_entry_next(it);
  }
  nvs_handle_t handle;if(nvs_open("tcsetup",NVS_READWRITE,&handle)!=ESP_OK)return false;
  bool ok=true;
  for(const auto& key:keys)if(nvs_erase_key(handle,key.c_str())!=ESP_OK){ok=false;break;}
  ok=ok&&nvs_commit(handle)==ESP_OK;
  ok=ok&&nvs_erase_key(handle,"factory")==ESP_OK&&nvs_commit(handle)==ESP_OK;
  nvs_close(handle);return ok;
}
bool resetMarker(bool value){
  Preferences p;if(!p.begin("tcsetup",false))return false;
  const bool ok=p.putBool("reset",value)==1;p.end();return ok;
}
void fail(const char* why){error=why;page=SetupPage::Error;}
void restartSoon(){page=SetupPage::Saving;rebootAt=millis()+400;}
void finishNetworkReset(){
  // Complete an old firmware's already-confirmed network-only transaction
  // with its original scope when upgrading during an interrupted reset.
  // Keep the journal until all three stores are cleared. A power loss resumes
  // the operation before starting either radio or accepting old credentials.
  struct Store {
    bool clearPairing(){return deviceForgetPairing();}
    bool clearNetwork(){return provForgetCreds();}
    bool selectWiFi(){return transportSetMode(TransportMode::WiFi);}
    bool finishJournal(){return resetMarker(false);}
  } store;
  if(!completeNetworkReset(store)){
    fail("Storage failed");return;
  }
  restartSoon();
}
void finishFactoryReset(){
  struct Store {
    std::vector<std::string> namespaces(){
      std::vector<std::string> names;
      auto it=nvs_entry_find("nvs",nullptr,NVS_TYPE_ANY);
      while(it){
        nvs_entry_info_t info;nvs_entry_info(it,&info);
        if(std::find(names.begin(),names.end(),info.namespace_name)==names.end())names.emplace_back(info.namespace_name);
        it=nvs_entry_next(it);
      }
      return names;
    }
    bool clearNamespace(const char* name){return ::clearNamespace(name);}
    bool finishJournal(){return finishFactoryJournal();}
  } store;
  if(!completeFactoryReset(store)){fail("Storage failed");return;}
  Serial.println("[setup] factory reset complete; restarting for first connection");
  restartSoon();
}
void startFactoryReset(){page=SetupPage::Resetting;resetAt=millis()+50;}
}
bool connectionSetupResetPending(){return resetRequest()!=ResetRequest::None;}
void connectionSetupBegin(bool resetHeld){
  const auto pending=resetRequest();
  if(pending==ResetRequest::Failed){fail("Storage failed");return;}
  if(pending==ResetRequest::Factory){startFactoryReset();return;}
  if(pending==ResetRequest::Network){finishNetworkReset();return;}
  if(resetHeld){page=SetupPage::ResetConfirm;return;}
  char ssid[33]{},pass[65]{};
  const bool wifi=provLoadCreds(ssid,sizeof(ssid),pass,sizeof(pass));memset(pass,0,sizeof(pass));
  if(needsConnectionChoice(transportHasChoice(),devicePaired(),wifi))page=SetupPage::Choose;
}
bool connectionSetupActive(){return page!=SetupPage::None;}
SetupPage connectionSetupPage(){return page;}
const char* connectionSetupError(){return error;}
void connectionSetupTick(bool a,bool b,bool aClicked,bool bClicked){
  // Preserve a choice made during an OTA trial, but let the local self-test
  // confirm the image before this intentional restart can trigger rollback.
  if(rebootAt&&int32_t(millis()-rebootAt)>=0&&transportBootVerified())ESP.restart();
  if(page==SetupPage::Resetting){if(int32_t(millis()-resetAt)>=0)finishFactoryReset();return;}
  const auto action=buttons.update(page,a,b,aClicked,bClicked);
  if(action==SetupAction::ConfirmReset){
    if(!factoryMarker())fail("Storage failed");else startFactoryReset();
  }else if(action==SetupAction::Cancel||action==SetupAction::Restart)restartSoon();
  else if(action==SetupAction::ChooseBLE||action==SetupAction::ChooseWiFi){
    if(!transportSetMode(action==SetupAction::ChooseBLE?TransportMode::BLE:TransportMode::WiFi))fail("Storage failed");
    else restartSoon();
  }
}
