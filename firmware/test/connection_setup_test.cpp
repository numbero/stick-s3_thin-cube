#include "../src/connection_setup_policy.h"
#include "../src/pairing_policy.h"
#include "../src/button_gestures.h"
#include <cassert>
#include <cstdio>
#include <string>

struct ResetStore {
  int failure,step=0;
  bool journal=true,paired=true,wifi=true;
  TransportMode mode=TransportMode::BLE;
  bool clearPairing(){if(++step==failure)return false;paired=false;return true;}
  bool clearNetwork(){if(++step==failure)return false;wifi=false;return true;}
  bool selectWiFi(){if(++step==failure)return false;mode=TransportMode::WiFi;return true;}
  bool finishJournal(){if(++step==failure)return false;journal=false;return true;}
};
int main(){
  assert(needsConnectionChoice(false,false,false));
  TransportRecord legacy;assert(!connectionChoiceSaved(true,legacy));
  legacy.mode=TransportMode::BLE;assert(connectionChoiceSaved(true,legacy));
  legacy.mode=TransportMode::WiFi;strcpy(legacy.source,"setup");assert(connectionChoiceSaved(true,legacy));
  assert(!connectionChoiceSaved(false,legacy));
  for(int flags=1;flags<8;flags++)assert(!needsConnectionChoice(flags&1,flags&2,flags&4));
  assert(validWiFiCredentials("network",""));assert(validWiFiCredentials("network","12345678"));
  assert(validWiFiCredentials(std::string(32,'s').c_str(),std::string(63,'p').c_str()));
  assert(!validWiFiCredentials("","12345678"));assert(!validWiFiCredentials("n","short"));
  assert(!validWiFiCredentials(std::string(33,'s').c_str(),""));assert(!validWiFiCredentials("n",std::string(64,'p').c_str()));
  assert(!validWiFiCredentials(nullptr,""));
  for(int stage=PROV_IDLE;stage<=PROV_LEAVING;stage++)assert(provCanLeave(ProvStage(stage))==(stage==PROV_WAIT||stage==PROV_FAIL));
  ButtonGestures button;using A=ButtonGestures::Action;
  button.update(0,false,true,false,provCanLeave(PROV_WAIT),false);
  assert(button.update(3000,false,false,false,provCanLeave(PROV_WAIT),false)==A::Switch);
  button={};button.update(0,false,true,false,provCanLeave(PROV_VERIFY),false);
  assert(button.update(3001,false,false,false,provCanLeave(PROV_WAIT),false)==A::None);
  for(int failure=1;failure<=4;failure++){
    ResetStore store{failure};assert(!completeNetworkReset(store));assert(store.journal);
    store.failure=0;store.step=0;assert(completeNetworkReset(store));
    assert(!store.journal&&!store.paired&&!store.wifi&&store.mode==TransportMode::WiFi);
  }
  WiFiChange request;request.phase=WiFiChange::Pending;request.previous=TransportMode::BLE;
  strcpy(request.request,"one");strcpy(request.ssid,"network");strcpy(request.pass,"12345678");assert(request.valid());
  auto failed=request;failed.finish(false,"network_or_password");assert(failed.valid()&&failed.rebootPending);assert(failed.previous==TransportMode::BLE);assert(!failed.pass[0]&&!failed.ssid[0]);
  auto saved=request;saved.finish(true);assert(saved.valid()&&saved.phase==WiFiChange::Succeeded&&!saved.pass[0]);
  request.pass[64]='x';assert(!request.valid());
  PairingPolicy pair;pair.offer(0,false);assert(pair.visible&&!pair.active(999999));assert(!pair.attempt(999999));
  assert(pair.begin(999999));assert(pair.active(1000000));assert(!pair.begin(1000000));
  assert(pair.attempt(1000000));assert(!pair.attempt(1000500));assert(pair.attempt(1001000));
  for(unsigned i=2;i<10;i++)assert(pair.attempt(1000000+i*1000));assert(!pair.attempt(1010000));
  assert(pair.expire(1120000,true));assert(!pair.visible);
  pair.offer(0xfffffff0,true);assert(pair.active(20));assert(pair.expire(120000,false));assert(pair.visible&&!pair.timed);
  pair.offer(100,true);pair.close();assert(!pair.visible&&!pair.active(101));
  puts("first connection, reset journal fault recovery, Wi-Fi handoff, setup escape and pairing expiry passed");
}
