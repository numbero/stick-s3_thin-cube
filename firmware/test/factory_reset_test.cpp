#include "../src/connection_setup_policy.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

struct FactoryStore {
  std::map<std::string,std::map<std::string,std::string>> saved{
    {"tcsetup",{{"factory","1"},{"reset","0"}}},
    {"thincube",{{"key","old-host"},{"apPass","old-hotspot"},{"bright","42"},
                 {"upright","-1"},{"faceup","1"},{"sndVol","70"},{"deep","1"},{"ledDebug","1"}}},
    {"clawd",{{"wifi","old-network"},{"ssid","legacy-network"},{"pass","legacy-password"},{"wifiChange","pending"}}},
    {"tcmode",{{"transaction","bluetooth"}}},
    {"bt_config.conf",{{"bt_cfg_key0","two-host-bonds"}}},
    {"nvs.net80211",{{"sta.ssid","sdk-network"},{"sta.pswd","sdk-password"}}},
    {"dhcp_state",{{"WIFI_STA_DEF","old-address"}}},
    {"uiflow",{{"ssid0","factory-demo-network"}}},
    {"M5GFX",{{"AUTODETECT","cached-board"}}},
    {"M5Unified",{{"ax","old-offset"}}},
    {"phy",{{"cal_data","cached-radio-calibration"}}},
    {"misc",{{"cal_data","cached-radio-calibration"}}},
    {"future-setting",{{"new-key","must-also-reset"}}}
  };
  int failAt=0,step=0;
  std::vector<std::string> cleared;
  std::vector<std::string> namespaces() const {
    std::vector<std::string> result;
    for(const auto& space:saved)result.push_back(space.first);
    return result;
  }
  bool clearNamespace(const char* name){
    assert(std::string(name)!="tcsetup");
    assert(saved.at("tcsetup").at("factory")=="1");
    if(++step==failAt){
      // A namespace erase can stop midway through its keys.
      auto& values=saved.at(name);values.erase(values.begin());return false;
    }
    saved.erase(name);cleared.emplace_back(name);return true;
  }
  bool finishJournal(){
    assert(saved.size()==1&&saved.count("tcsetup"));
    if(++step==failAt)return false;
    saved.erase("tcsetup");return true;
  }
};

int main(){
  using P=SetupPage;using A=SetupAction;
  SetupButtons boot;
  assert(boot.update(P::ResetConfirm,false,true,false,false)==A::None);
  assert(boot.update(P::ResetConfirm,true,true,true,false)==A::None);
  assert(boot.update(P::ResetConfirm,false,false,true,true)==A::None);
  assert(boot.update(P::ResetConfirm,false,false,false,true)==A::Cancel);
  FactoryStore unchanged;const auto original=unchanged.saved;
  // Entering, releasing boot B, waiting and cancelling never request a reset.
  SetupButtons cancel;
  assert(cancel.update(P::ResetConfirm,false,true,false,false)==A::None);
  assert(cancel.update(P::ResetConfirm,false,false,false,true)==A::None);
  for(int n=0;n<100;n++)assert(cancel.update(P::ResetConfirm,false,false,false,false)==A::None);
  assert(cancel.update(P::ResetConfirm,false,false,false,true)==A::Cancel);
  assert(unchanged.saved==original&&unchanged.cleared.empty());
  SetupButtons confirm;
  assert(confirm.update(P::ResetConfirm,false,false,false,true)==A::None);
  assert(confirm.update(P::ResetConfirm,false,true,true,false)==A::None);
  assert(confirm.update(P::ResetConfirm,false,false,true,true)==A::Cancel);
  assert(confirm.update(P::ResetConfirm,false,false,true,false)==A::ConfirmReset);
  assert(confirm.update(P::Resetting,false,false,true,false)==A::None);
  assert(confirm.update(P::Saving,false,false,false,true)==A::None);
  assert(confirm.update(P::Error,false,false,false,true)==A::Restart);
  assert(confirm.update(P::Choose,false,false,true,true)==A::None);
  assert(confirm.update(P::Choose,false,false,true,false)==A::ChooseBLE);
  assert(confirm.update(P::Choose,false,false,false,true)==A::ChooseWiFi);

  FactoryStore success;const int steps=int(success.saved.size());
  assert(completeFactoryReset(success));assert(success.saved.empty());
  assert(needsConnectionChoice(false,false,false));
  for(int failure=1;failure<=steps;failure++){
    FactoryStore interrupted;interrupted.failAt=failure;
    assert(!completeFactoryReset(interrupted));
    assert(interrupted.saved.at("tcsetup").at("factory")=="1");
    // Simulate a new boot: only persisted state survives.
    FactoryStore resumed;resumed.saved=interrupted.saved;
    assert(completeFactoryReset(resumed));assert(resumed.saved.empty());
  }
  FactoryStore journalOnly;journalOnly.saved={{"tcsetup",{{"factory","1"}}}};
  assert(completeFactoryReset(journalOnly));assert(journalOnly.saved.empty());
  puts("Factory reset: explicit click, cancel, all namespaces, legacy/future settings and every interrupted erase passed");
}
