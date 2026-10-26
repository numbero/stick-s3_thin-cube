#include "ble_link.h"
#include "ble_framing.h"
#include "power_save_policy.h"
#include "connection_timing.h"
#include "ble_advertising_policy.h"
#include "device.h"
#include "connection_setup.h"
#include "transport.h"
#include <Arduino.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_gatt_common_api.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <algorithm>

namespace {
// 8D7A1000-6D43-4F3B-9AC8-4C7E62A8B101, control 1001, audio 1002.
uint8_t serviceUUID[16]={0x01,0xB1,0xA8,0x62,0x7E,0x4C,0xC8,0x9A,0x3B,0x4F,0x43,0x6D,0x00,0x10,0x7A,0x8D};
uint8_t controlUUID[16]={0x01,0xB1,0xA8,0x62,0x7E,0x4C,0xC8,0x9A,0x3B,0x4F,0x43,0x6D,0x01,0x10,0x7A,0x8D};
uint8_t audioUUID[16]={0x01,0xB1,0xA8,0x62,0x7E,0x4C,0xC8,0x9A,0x3B,0x4F,0x43,0x6D,0x02,0x10,0x7A,0x8D};
enum {Service,ControlDecl,ControlValue,ControlCCC,AudioDecl,AudioValue,AudioCCC,Count};
uint16_t handles[Count],primary=ESP_GATT_UUID_PRI_SERVICE,decl=ESP_GATT_UUID_CHAR_DECLARE,cccUUID=ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
uint8_t ctrlProp=ESP_GATT_CHAR_PROP_BIT_WRITE|ESP_GATT_CHAR_PROP_BIT_NOTIFY,audioProp=ESP_GATT_CHAR_PROP_BIT_NOTIFY,empty[2]={};
esp_gatts_attr_db_t db[Count];
esp_gatt_if_t interface=ESP_GATT_IF_NONE;
std::atomic<bool> linked{false},secured{false},congested{false},controlSubscribed{false},audioSubscribed{false},healthy{false};
std::atomic<uint16_t> mtu{23},connection{0};
std::atomic<uint32_t> generation{0};
std::atomic<uint32_t> notifyOK{0},notifyFailed{0},congestionCount{0};
std::atomic<unsigned> notifyPending{0};
uint8_t remote[6]={};
std::atomic<bool> advReady{false},scanReady{false};
std::atomic<unsigned> advStartResult{0},advStopResult{0};
BLEAdvertisingPolicy advertising;
struct Fragment {uint16_t length;uint8_t data[512];};
struct Message {uint16_t length,id,offset;uint32_t created;uint8_t data[2048];};
struct AudioMessage {uint16_t length,id,offset;uint32_t created;uint8_t data[348];};
// macOS negotiates a 30ms connection interval. A 40ms deadline discarded
// audio during ordinary multi-event scheduling stalls. Bound total buffering
// to six 20ms packets so these stalls can drain without an unbounded backlog.
constexpr unsigned audioQueuePackets=6;
constexpr uint32_t audioDeadlineMs=120;
QueueHandle_t incoming=nullptr,controlOut=nullptr,audioOut=nullptr,complete=nullptr;
Message controlTx{},controlScratch{},receiveScratch{};
BLEControlAssembly rx;
Message* statusPending=nullptr;
Message* snapshotPending=nullptr;
uint32_t audioDropped=0;AudioMessage audioTx{};
BLEPowerPolicy powerPolicy;
bool idleRequested=false;
bool modemSleep=false;
std::atomic<uint32_t> intervalObservation{0},intervalRevision{0};
uint32_t observedIntervalRevision=0;
uint16_t nextMessage=0;uint32_t observedGeneration=0,lastPump=0;
esp_ble_adv_params_t params={.adv_int_min=0x30,.adv_int_max=0x60,.adv_type=ADV_TYPE_IND,.own_addr_type=BLE_ADDR_TYPE_PUBLIC,
  .channel_map=ADV_CHNL_ALL,.adv_filter_policy=ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY};
void gap(esp_gap_ble_cb_event_t event,esp_ble_gap_cb_param_t* p){
  switch(event){
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      if(p->adv_start_cmpl.status==ESP_BT_STATUS_SUCCESS)connectionBootMark(ConnectionStage::Advertising);
      advStartResult=p->adv_start_cmpl.status==ESP_BT_STATUS_SUCCESS?1:2;break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
      advStopResult=p->adv_stop_cmpl.status==ESP_BT_STATUS_SUCCESS?1:2;break;
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:advReady=p->adv_data_cmpl.status==ESP_BT_STATUS_SUCCESS;break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:scanReady=p->scan_rsp_data_cmpl.status==ESP_BT_STATUS_SUCCESS;break;
    // Secure Connections with no input/output encrypts the link without a
    // system passkey. ThinCube's PIN authenticates the application separately.
    case ESP_GAP_BLE_SEC_REQ_EVT:esp_ble_gap_security_rsp(p->ble_security.ble_req.bd_addr,true);break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
      intervalObservation.store((uint32_t(p->update_conn_params.status==ESP_BT_STATUS_SUCCESS)<<16)|p->update_conn_params.conn_int);
      intervalRevision.fetch_add(1);
      Serial.printf("[ble] interval %u x1.25ms latency %u status %u\n",p->update_conn_params.conn_int,p->update_conn_params.latency,p->update_conn_params.status);break;
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
      Serial.printf("[ble] data length tx %u rx %u status %u\n",p->pkt_data_lenth_cmpl.params.tx_len,p->pkt_data_lenth_cmpl.params.rx_len,p->pkt_data_lenth_cmpl.status);break;
    case ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT:
      Serial.printf("[ble] PHY tx %u rx %u status %u\n",p->phy_update.tx_phy,p->phy_update.rx_phy,p->phy_update.status);break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      if(!memcmp(p->ble_security.auth_cmpl.bd_addr,remote,6)){
        secured=p->ble_security.auth_cmpl.success&&(p->ble_security.auth_cmpl.auth_mode&ESP_LE_AUTH_REQ_SC_ONLY);
        Serial.printf("[ble] automatic encryption success %d accepted %d mode 0x%02x reason 0x%02x\n",p->ble_security.auth_cmpl.success,secured.load(),p->ble_security.auth_cmpl.auth_mode,p->ble_security.auth_cmpl.fail_reason);
        if(secured){
          connectionTimingMark(ConnectionStage::Secured);
          esp_ble_gap_set_pkt_data_len(remote,251);
          const auto phyResult=esp_ble_gap_set_prefered_phy(remote,0,ESP_BLE_GAP_PHY_2M_PREF_MASK,ESP_BLE_GAP_PHY_2M_PREF_MASK,0);
          Serial.printf("[ble] preferred 2M request result %d\n",int(phyResult));
        }else esp_ble_gap_disconnect(remote);
      }break;
    default:break;
  }
}
void gatts(esp_gatts_cb_event_t event,esp_gatt_if_t iface,esp_ble_gatts_cb_param_t* p){
  switch(event){
    case ESP_GATTS_REG_EVT:
      interface=iface;if(p->reg.status!=ESP_GATT_OK)return;
      esp_ble_gatts_create_attr_tab(db,iface,Count,0);break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
      if(p->add_attr_tab.status!=ESP_GATT_OK||p->add_attr_tab.num_handle!=Count)return;
      memcpy(handles,p->add_attr_tab.handles,sizeof(handles));esp_ble_gatts_start_service(handles[Service]);break;
    case ESP_GATTS_START_EVT:healthy=p->start.status==ESP_GATT_OK;break;
    case ESP_GATTS_CONNECT_EVT:{
      if(linked){esp_ble_gap_disconnect(p->connect.remote_bda);break;}
      memcpy(remote,p->connect.remote_bda,6);connection=p->connect.conn_id;linked=true;secured=false;congested=false;
      controlSubscribed=audioSubscribed=false;mtu=23;
      notifyPending=0;
      const uint16_t initialInterval=p->connect.conn_params.interval;
      intervalObservation=(uint32_t(initialInterval>=6&&initialInterval<=3200)<<16)|initialInterval;
      intervalRevision.fetch_add(1);
      const uint32_t current=generation.fetch_add(1)+1;
      connectionTimingBegin("ble",current);connectionTimingMark(ConnectionStage::Channel,initialInterval);
      esp_ble_set_encryption(remote,ESP_BLE_SEC_ENCRYPT);break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
      connectionTimingMark(ConnectionStage::Disconnected,p->disconnect.reason);
      Serial.printf("[ble] disconnect reason 0x%02x\n",p->disconnect.reason);
      linked=secured=controlSubscribed=audioSubscribed=false;generation.fetch_add(1);break;
    case ESP_GATTS_MTU_EVT:mtu=std::min<uint16_t>(p->mtu.mtu,517);connectionTimingMark(ConnectionStage::MTU,mtu);break;
    case ESP_GATTS_CONGEST_EVT:congested=p->congest.congested;if(congested)congestionCount++;break;
    case ESP_GATTS_CONF_EVT:
      if(p->conf.conn_id==connection){unsigned pending=notifyPending.load();while(pending&&!notifyPending.compare_exchange_weak(pending,pending-1)){};}
      if(p->conf.status==ESP_GATT_OK)notifyOK++;else notifyFailed++;break;
    case ESP_GATTS_READ_EVT:{
      esp_gatt_rsp_t response{};response.attr_value.handle=p->read.handle;
      esp_gatt_status_t status=ESP_GATT_READ_NOT_PERMIT;
      if(secured&&(p->read.handle==handles[ControlCCC]||p->read.handle==handles[AudioCCC])){
        response.attr_value.len=2;response.attr_value.value[0]=p->read.handle==handles[ControlCCC]?controlSubscribed.load():audioSubscribed.load();status=ESP_GATT_OK;
      }
      esp_ble_gatts_send_response(iface,p->read.conn_id,p->read.trans_id,status,&response);break;
    }
    case ESP_GATTS_WRITE_EVT:{
      esp_gatt_status_t status=ESP_GATT_OK;
      if(p->write.is_prep||p->write.offset||!secured)status=ESP_GATT_WRITE_NOT_PERMIT;
      else if(p->write.handle==handles[ControlCCC]||p->write.handle==handles[AudioCCC]){
        if(p->write.len!=2||p->write.value[1]||p->write.value[0]>1)status=ESP_GATT_INVALID_ATTR_LEN;
        else if(p->write.handle==handles[ControlCCC]){controlSubscribed=p->write.value[0]==1;if(controlSubscribed)connectionTimingMark(ConnectionStage::ControlSubscribed);}
        else{audioSubscribed=p->write.value[0]==1;if(audioSubscribed)connectionTimingMark(ConnectionStage::AudioSubscribed);}
      }else if(p->write.handle==handles[ControlValue]){
        Fragment f{};
        if(p->write.len<9||p->write.len>sizeof(f.data))status=ESP_GATT_INVALID_ATTR_LEN;
        else{f.length=p->write.len;memcpy(f.data,p->write.value,f.length);if(xQueueSend(incoming,&f,0)!=pdTRUE)status=ESP_GATT_NO_RESOURCES;}
      }else status=ESP_GATT_WRITE_NOT_PERMIT;
      if(p->write.need_rsp)esp_ble_gatts_send_response(iface,p->write.conn_id,p->write.trans_id,status,nullptr);break;
    }
    default:break;
  }
}
void attribute(int n,uint16_t uuidLength,uint8_t* uuid,uint16_t permission,uint16_t maximum,uint16_t length,uint8_t* value,bool automatic=true){
  db[n].attr_control.auto_rsp=automatic?ESP_GATT_AUTO_RSP:ESP_GATT_RSP_BY_APP;
  db[n].att_desc={uuidLength,uuid,permission,maximum,length,value};
}
void put(uint8_t* b,uint16_t value){b[0]=value>>8;b[1]=value;}
template<typename T> bool sendPart(T& message,uint16_t handle){
  // Bound work submitted to BTC; its completion callback releases the slot.
  if(notifyPending>=2)return false;
  // Keep controller buffers available for ATT write/security responses. These
  // are submitted by BTC independently of this main-loop notification queue.
  if(esp_ble_get_sendable_packets_num() < (handle==handles[AudioValue]?5:3))return false;
  const unsigned count=std::min<unsigned>(message.length-message.offset,std::min<unsigned>(mtu.load()-11,236));
  uint8_t fragment[514];fragment[0]='T';fragment[1]='C';put(fragment+2,message.id);put(fragment+4,message.offset);put(fragment+6,message.length);
  memcpy(fragment+8,message.data+message.offset,count);
  notifyPending++;
  if(esp_ble_gatts_send_indicate(interface,connection,handle,count+8,fragment,false)!=ESP_OK){notifyPending--;return false;}
  message.offset+=count;if(message.offset==message.length)message.length=0;return true;
}
}
bool bleBegin(const char* name){
  static StaticQueue_t queues[4];
  auto makeQueue=[&](unsigned index,unsigned count,unsigned size){
    auto* data=static_cast<uint8_t*>(heap_caps_malloc(count*size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    return data?xQueueCreateStatic(count,size,data,&queues[index]):nullptr;
  };
  incoming=makeQueue(0,8,sizeof(Fragment));controlOut=makeQueue(1,3,sizeof(Message));audioOut=makeQueue(2,audioQueuePackets,sizeof(AudioMessage));complete=makeQueue(3,4,sizeof(Message));
  statusPending=static_cast<Message*>(heap_caps_calloc(1,sizeof(Message),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  snapshotPending=static_cast<Message*>(heap_caps_calloc(1,sizeof(Message),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!incoming||!controlOut||!audioOut||!complete||!statusPending||!snapshotPending)return false;
  esp_bt_controller_config_t config=BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  config.sleep_mode=ESP_BT_SLEEP_MODE_1;config.sleep_clock=ESP_BT_SLEEP_CLOCK_MAIN_XTAL;
  esp_err_t result=esp_bt_controller_init(&config);
  if(result==ESP_OK)result=esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if(result==ESP_OK)result=esp_bt_sleep_enable();
  modemSleep=result==ESP_OK;
  if(!modemSleep){
    Serial.printf("[ble] modem sleep unavailable %d; using normal controller\n",int(result));
    if(esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_ENABLED)esp_bt_controller_disable();
    if(esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_INITED)esp_bt_controller_deinit();
    if(esp_bt_controller_get_status()!=ESP_BT_CONTROLLER_STATUS_IDLE)return false;
    config.sleep_mode=ESP_BT_SLEEP_MODE_NONE;config.sleep_clock=ESP_BT_SLEEP_CLOCK_NONE;
    if(esp_bt_controller_init(&config)!=ESP_OK||esp_bt_controller_enable(ESP_BT_MODE_BLE)!=ESP_OK)return false;
  }
  Serial.printf("[ble] modem sleep %d main XTAL\n",modemSleep);
  if(esp_bluedroid_init()!=ESP_OK||esp_bluedroid_enable()!=ESP_OK)return false;
  esp_ble_gap_register_callback(gap);esp_ble_gatts_register_callback(gatts);esp_ble_gatt_set_local_mtu(517);
  const auto phyResult=esp_ble_gap_set_prefered_default_phy(ESP_BLE_GAP_PHY_2M_PREF_MASK,ESP_BLE_GAP_PHY_2M_PREF_MASK);
  Serial.printf("[ble] default 2M request result %d\n",int(phyResult));
  // No system passkey or MITM input. Persist the automatically negotiated
  // encryption key: macOS reuses it after reconnects and device reboots.
  uint8_t onlyAuth=1,auth=ESP_LE_AUTH_REQ_SC_BOND,io=ESP_IO_CAP_NONE,size=16,keys=ESP_BLE_ENC_KEY_MASK|ESP_BLE_ID_KEY_MASK;
  esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH,&onlyAuth,1);esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE,&auth,1);esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,&io,1);
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,&size,1);esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY,&keys,1);esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,&keys,1);
  attribute(Service,2,(uint8_t*)&primary,ESP_GATT_PERM_READ,16,16,serviceUUID);
  attribute(ControlDecl,2,(uint8_t*)&decl,ESP_GATT_PERM_READ,1,1,&ctrlProp);
  attribute(ControlValue,16,controlUUID,ESP_GATT_PERM_WRITE_ENCRYPTED,512,0,empty,false);
  attribute(ControlCCC,2,(uint8_t*)&cccUUID,ESP_GATT_PERM_READ_ENCRYPTED|ESP_GATT_PERM_WRITE_ENCRYPTED,2,2,empty,false);
  attribute(AudioDecl,2,(uint8_t*)&decl,ESP_GATT_PERM_READ,1,1,&audioProp);
  attribute(AudioValue,16,audioUUID,ESP_GATT_PERM_READ_ENCRYPTED,1,0,empty);
  attribute(AudioCCC,2,(uint8_t*)&cccUUID,ESP_GATT_PERM_READ_ENCRYPTED|ESP_GATT_PERM_WRITE_ENCRYPTED,2,2,empty,false);
  esp_ble_gatts_app_register(0x5443);esp_ble_gap_set_device_name(name);
  esp_ble_adv_data_t adv{};adv.flag=ESP_BLE_ADV_FLAG_GEN_DISC|ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;adv.service_uuid_len=16;adv.p_service_uuid=serviceUUID;
  esp_ble_gap_config_adv_data(&adv);esp_ble_adv_data_t scan{};scan.set_scan_rsp=true;scan.include_name=true;esp_ble_gap_config_adv_data(&scan);
  return true;
}
void bleTick(uint32_t now){
  if(!incoming||!complete||!controlOut||!audioOut||!statusPending)return;
  advertising.observe(now,generation.load());
  if(const unsigned result=advStartResult.exchange(0)){
    advertising.completed(true,result==1,now);
    Serial.printf("[ble-adv] start %s interval %u..%u ms result %u\n",advertising.fast?"fast":"slow",
      unsigned(params.adv_int_min)*5/8,unsigned(params.adv_int_max)*5/8,result);
  }
  if(const unsigned result=advStopResult.exchange(0))advertising.completed(false,result==1,now);
  const auto advOp=advertising.next(now,advReady&&scanReady&&healthy,linked,
    devicePairWindow()||connectionSetupActive()||transportBusy());
  if(advOp!=BLEAdvertisingPolicy::Operation::None){
    esp_err_t result;
    if(advOp==BLEAdvertisingPolicy::Operation::Stop)result=esp_ble_gap_stop_advertising();
    else {
      const bool fast=advOp==BLEAdvertisingPolicy::Operation::StartFast;
      params.adv_int_min=fast?BLEAdvertisingPolicy::FastMin:BLEAdvertisingPolicy::SlowMin;
      params.adv_int_max=fast?BLEAdvertisingPolicy::FastMax:BLEAdvertisingPolicy::SlowMax;
      result=esp_ble_gap_start_advertising(&params);
    }
    advertising.issued(advOp,now,result==ESP_OK);
    if(result!=ESP_OK)Serial.printf("[ble-adv] request rejected %d\n",int(result));
  }
  if(observedGeneration!=generation){
    observedGeneration=generation;rx={};controlTx={};audioTx={};
    if(statusPending)*statusPending={};if(snapshotPending)*snapshotPending={};
    xQueueReset(incoming);xQueueReset(complete);xQueueReset(controlOut);xQueueReset(audioOut);
    observedIntervalRevision=intervalRevision.load();
    const uint32_t value=intervalObservation.load();
    powerPolicy.connected(value>>16?value&0xffff:0);
  }
  const uint32_t revision=intervalRevision.load();
  if(revision!=observedIntervalRevision){observedIntervalRevision=revision;const uint32_t value=intervalObservation.load();powerPolicy.observed(value&0xffff,value>>16);}
  if(bleConnected()&&powerPolicy.requestDue(now,idleRequested)){
    esp_ble_conn_update_params_t cp{};memcpy(cp.bda,remote,6);
    cp.min_int=powerPolicy.idle?BLEPowerPolicy::IdleMin:BLEPowerPolicy::FastMin;
    cp.max_int=powerPolicy.idle?BLEPowerPolicy::IdleMax:BLEPowerPolicy::FastMax;cp.latency=0;cp.timeout=400;
    powerPolicy.requested(now,esp_ble_gap_update_conn_params(&cp)==ESP_OK);
  }
  if(bleConnected()&&powerPolicy.audioReady())connectionTimingMark(ConnectionStage::PerformanceReady,powerPolicy.actual);
  Fragment f;
  for(int i=0;i<8&&xQueueReceive(incoming,&f,0)==pdTRUE;i++){
    if(rx.accept(f.data,f.length,now)){
      auto& m=receiveScratch;m={};m.length=rx.total;memcpy(m.data,rx.data,rx.total+1);
      xQueueSend(complete,&m,0);rx.reset();
    }
  }
  if(!bleConnected()||!controlSubscribed||congested||now==lastPump)return;lastPump=now;
  static uint32_t lastReport=0;
  if(now-lastReport>=5000){lastReport=now;Serial.printf("[ble] notify ok %u failed %u congestion %u credits %u pending %u audio queue %u\n",unsigned(notifyOK.load()),unsigned(notifyFailed.load()),unsigned(congestionCount.load()),esp_ble_get_sendable_packets_num(),notifyPending.load(),unsigned(uxQueueMessagesWaiting(audioOut)));}
  for(unsigned i=0;i<4;i++){
    if(!controlTx.length){
      if(xQueueReceive(controlOut,&controlTx,0)!=pdTRUE){
        if(statusPending->length){controlTx=*statusPending;*statusPending={};}
        else if(snapshotPending->length){controlTx=*snapshotPending;*snapshotPending={};}
      }
    }
    if(controlTx.length){if(!sendPart(controlTx,handles[ControlValue]))break;continue;}
    if(audioTx.length&&uint32_t(now-audioTx.created)>audioDeadlineMs){audioDropped+=audioTx.length==196?2:1;audioTx={};}
    while(!audioTx.length&&xQueueReceive(audioOut,&audioTx,0)==pdTRUE){if(uint32_t(now-audioTx.created)<=audioDeadlineMs)break;audioDropped+=audioTx.length==196?2:1;audioTx={};}
    if(!audioTx.length||!audioSubscribed)break;
    if(!sendPart(audioTx,handles[AudioValue]))break;
  }
}
bool bleTake(char* out,size_t capacity){if(!complete)return false;auto& m=receiveScratch;if(xQueueReceive(complete,&m,0)!=pdTRUE)return false;if(m.length>=capacity)return false;memcpy(out,m.data,m.length+1);return true;}
bool bleSendControl(const char* text){if(!bleConnected()||!controlSubscribed)return false;auto& m=controlScratch;m={};m.length=strlen(text);if(!m.length||m.length>=sizeof(m.data))return false;m.id=++nextMessage;m.created=millis();memcpy(m.data,text,m.length);return xQueueSend(controlOut,&m,0)==pdTRUE;}
bool bleSendAudio(const uint8_t* data,size_t length){if(!bleAudioReady()||length>348)return false;AudioMessage m{};m.length=length;m.id=++nextMessage;m.created=millis();memcpy(m.data,data,length);if(xQueueSend(audioOut,&m,0)==pdTRUE)return true;AudioMessage old;if(xQueueReceive(audioOut,&old,0)==pdTRUE)audioDropped+=old.length==196?2:1;return xQueueSend(audioOut,&m,0)==pdTRUE;}
bool bleConnected(){return linked&&secured;}
bool bleConnecting(){return linked&&!secured;}
bool bleHealthy(){return healthy;}
bool bleAudioReady(){return bleConnected()&&audioSubscribed&&mtu>=185;}
uint32_t bleGeneration(){return generation;}
void bleClearAudio(){if(audioOut)xQueueReset(audioOut);audioTx={};}
void bleDisconnect(){if(linked)esp_ble_gap_disconnect(remote);}
void bleSetIdle(bool idle){idleRequested=idle;}
void bleAdvertisingActivity(uint32_t now){advertising.activity(now);}
bool blePerformanceReady(){return powerPolicy.audioReady();}
uint16_t bleConnectionInterval(){return powerPolicy.actual;}

void bleSendStatus(const char* text){
  if(!bleConnected()||!controlSubscribed||!statusPending)return;
  const size_t length=strlen(text);if(!length||length>=sizeof(statusPending->data))return;
  statusPending->length=length;statusPending->id=++nextMessage;statusPending->offset=0;statusPending->created=millis();memcpy(statusPending->data,text,length);
}
void bleSendSnapshot(const char* text){
  if(!bleConnected()||!controlSubscribed||!snapshotPending)return;
  const size_t length=strlen(text);if(!length||length>=sizeof(snapshotPending->data))return;
  snapshotPending->length=length;snapshotPending->id=++nextMessage;snapshotPending->offset=0;snapshotPending->created=millis();memcpy(snapshotPending->data,text,length);
}
uint16_t bleMTU(){return mtu;}
uint32_t bleAudioDropped(){return audioDropped;}
