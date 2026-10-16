#include "connection_setup.h"
#include "transport.h"
#include "device.h"
#include "wifi_prov.h"
#include <Preferences.h>
#include <Arduino.h>
namespace {
SetupPage page=SetupPage::None;
bool released=false;
uint32_t rebootAt=0;
const char* error="";
bool resetMarker(bool value){
  Preferences p;if(!p.begin("tcsetup",false))return false;
  const bool ok=p.putBool("reset",value)==1;p.end();return ok;
}
void fail(const char* why){error=why;page=SetupPage::Error;}
void restartSoon(){page=SetupPage::Saving;rebootAt=millis()+400;}
void finishReset(){
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
}
void connectionSetupBegin(bool resetHeld){
  Preferences p;bool pending=false;
  if(p.begin("tcsetup",true)){pending=p.getBool("reset",false);p.end();}
  if(pending){finishReset();return;}
  if(resetHeld){page=SetupPage::ResetConfirm;return;}
  char ssid[33]{},pass[65]{};
  const bool wifi=provLoadCreds(ssid,sizeof(ssid),pass,sizeof(pass));memset(pass,0,sizeof(pass));
  if(needsConnectionChoice(transportHasChoice(),devicePaired(),wifi))page=SetupPage::Choose;
}
bool connectionSetupActive(){return page!=SetupPage::None;}
SetupPage connectionSetupPage(){return page;}
const char* connectionSetupError(){return error;}
void connectionSetupTick(bool a,bool b,bool aClicked,bool bClicked){
  if(rebootAt&&int32_t(millis()-rebootAt)>=0)ESP.restart();
  if(!released){released=!a&&!b;return;}
  if(page==SetupPage::ResetConfirm){
    if(aClicked){if(!resetMarker(true))fail("Storage failed");else finishReset();}
    else if(bClicked)restartSoon();
  }else if(page==SetupPage::Choose&&(aClicked||bClicked)){
    if(!transportSetMode(aClicked?TransportMode::BLE:TransportMode::WiFi))fail("Storage failed");
    else restartSoon();
  }else if(page==SetupPage::Error&&bClicked)restartSoon();
}
