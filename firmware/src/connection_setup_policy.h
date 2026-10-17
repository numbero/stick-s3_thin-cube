#pragma once
#include "transport_policy.h"
#include <stddef.h>

enum class SetupPage { None, Choose, ResetConfirm, Resetting, Saving, Error };
enum class SetupAction { None, ConfirmReset, Cancel, ChooseBLE, ChooseWiFi, Restart };
struct SetupButtons {
  bool released=false;
  SetupAction update(SetupPage page,bool a,bool b,bool aClicked,bool bClicked) {
    // Consume the boot hold and its release before accepting a new click.
    if(!released){released=!a&&!b;return SetupAction::None;}
    if(page==SetupPage::ResetConfirm){
      if(bClicked)return SetupAction::Cancel;
      if(aClicked&&!b)return SetupAction::ConfirmReset;
    }else if(page==SetupPage::Choose){
      if(aClicked&&!b&&!bClicked)return SetupAction::ChooseBLE;
      if(bClicked&&!a&&!aClicked)return SetupAction::ChooseWiFi;
    }else if(page==SetupPage::Error&&bClicked)return SetupAction::Restart;
    return SetupAction::None;
  }
};
template<class Store> bool completeFactoryReset(Store& store) {
  // Snapshot names before erasing: NVS iterators cannot survive mutation.
  // Keep the journal until every other namespace has been cleared, including
  // SDK Wi-Fi/BLE records, legacy settings and future configuration stores.
  const auto names=store.namespaces();
  for(const auto& name:names)
    if(name!="tcsetup"&&!store.clearNamespace(name.c_str()))return false;
  return store.finishJournal();
}
inline bool connectionChoiceSaved(bool valid,const TransportRecord& record) {
  // Older boot-B recovery wrote an empty stable Wi-Fi record before the user
  // had actually selected a connection. Do not mistake that for onboarding.
  return valid&&(record.mode==TransportMode::BLE||record.source[0]||record.request[0]);
}
inline bool needsConnectionChoice(bool chosen, bool paired, bool wifiConfigured) {
  return !chosen && !paired && !wifiConfigured;
}
template<class Store> bool completeNetworkReset(Store& store) {
  return store.clearPairing()&&store.clearNetwork()&&store.selectWiFi()&&store.finishJournal();
}
inline bool validWiFiCredentials(const char* ssid, const char* pass) {
  if(!ssid||!pass)return false;
  const auto s=strnlen(ssid,33),p=strnlen(pass,65);
  return s>0&&s<=32&&p<=63&&(p==0||p>=8);
}
enum ProvStage { PROV_IDLE, PROV_AP, PROV_WAIT, PROV_VERIFY, PROV_OK, PROV_FAIL, PROV_LEAVING };
inline bool provCanLeave(ProvStage stage) { return stage==PROV_WAIT||stage==PROV_FAIL; }
struct WiFiChange {
  enum Phase : uint8_t { Idle, Pending, Succeeded, Failed };
  uint32_t magic=0x57494631;
  Phase phase=Idle;
  TransportMode previous=TransportMode::WiFi;
  bool rebootPending=false;
  char request[37]{},ssid[33]{},pass[65]{},error[32]{};
  bool valid() const {
    return magic==0x57494631&&phase<=Failed&&uint8_t(previous)<=1&&request[36]==0&&
      ssid[32]==0&&pass[64]==0&&error[31]==0&&
      (phase==Idle||(request[0]&&(phase!=Pending||validWiFiCredentials(ssid,pass))));
  }
  void finish(bool success,const char* why="") {
    phase=success?Succeeded:Failed;rebootPending=true;memset(ssid,0,sizeof(ssid));memset(pass,0,sizeof(pass));
    strncpy(error,why,sizeof(error)-1);error[sizeof(error)-1]=0;
  }
};
