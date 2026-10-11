#include "../src/connection_hint.h"
#include "../src/button_gestures.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <initializer_list>
int main(){
  for(const auto current:{TransportMode::WiFi,TransportMode::BLE}){
    ButtonGestures combo;combo.update(0,true,true,false,true,true);combo.update(3000,true,true,false,true,true);
    assert(combo.comboReady);
    assert(connectionHintMode(current,combo.switchReady&&!combo.comboReady,false,current)==current);
    assert(combo.update(3001,false,false,false,true,true)==ButtonGestures::Action::Pair);
    assert(connectionHintMode(current,false,false,current)==current);
    ButtonGestures single;single.update(0,false,true,false,true,true);single.update(3000,false,true,false,true,true);
    const auto target=current==TransportMode::BLE?TransportMode::WiFi:TransportMode::BLE;
    assert(connectionHintMode(current,single.switchReady,false,current)==target);
    assert(connectionHintMode(current,false,true,target)==target);
  }
  assert(!strcmp(connectionHintLabel(TransportMode::BLE),"BLUETOOTH"));
  assert(!strcmp(connectionHintLabel(TransportMode::WiFi),"WI-FI"));
  puts("re-pair and transport switch hint modes passed");
}
