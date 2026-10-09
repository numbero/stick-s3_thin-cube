#include "wifi_prov.h"
#include "ui.h"
#include <WiFi.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <esp_http_server.h>
#include <esp_system.h>
#include <cJSON.h>
#include <atomic>
#include <mutex>

namespace {
struct Submission { char ssid[33], pass[65]; };
std::atomic<ProvStage> stage{PROV_IDLE};
std::mutex lock;
Submission target{};
QueueHandle_t submissions=nullptr;
DNSServer dns;
httpd_handle_t server=nullptr;
String scanJSON="[]";
char apSSID[32]{},apPass[9]{},error[32]{};
std::atomic<int> tries{0};
uint32_t verifyAt=0,okAt=0;
bool started=false,scanPending=false;
const char portal[]=R"HTML(<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>ThinCube 配网</title><style>body{max-width:440px;margin:40px auto;padding:20px;background:#0d0d0d;color:#f8b000;font:16px/1.6 system-ui}input,button,select{box-sizing:border-box;width:100%;padding:12px;margin:8px 0;font:inherit}label{display:block}button{cursor:pointer}#status{white-space:pre-wrap}</style><h1>ThinCube</h1><p>选择 2.4 GHz Wi-Fi。连接成功后，在 Mac 上打开 ThinCube 完成配对。</p><select id="net"><option value="">选择网络或手动输入</option></select><form id="form"><label>Wi-Fi 名称<input id="ssid" maxlength="32" required autocomplete="off"></label><label>Wi-Fi 密码<input id="pass" type="password" maxlength="63" autocomplete="off"></label><button id="save">连接 Wi-Fi</button></form><p role="status" id="status"></p><script>
const $=s=>document.querySelector(s);let polling=false;
async function scan(){try{const r=await fetch('/scan');const list=await r.json();for(const n of list){const o=document.createElement('option');o.value=n.s;o.textContent=n.s+' ('+n.r+' dBm)';$('#net').append(o);}if(!list.length)setTimeout(scan,1800);}catch{}}
$('#net').onchange=e=>{$('#ssid').value=e.target.value};scan();
$('#form').onsubmit=async e=>{e.preventDefault();if(polling)return;polling=true;$('#save').disabled=true;$('#status').textContent='正在验证…';try{const r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:$('#ssid').value,password:$('#pass').value})});if(!r.ok)throw Error('请检查网络名称与密码长度');const poll=async()=>{try{const s=await(await fetch('/status')).json();$('#status').textContent=s.stage==='ok'?'连接成功，设备正在重启。':s.stage==='fail'?'连接失败：'+s.err:'正在连接 '+s.tries+'/3';if(s.stage==='ok'||s.stage==='fail'){polling=false;$('#save').disabled=false;$('#pass').value='';return;}setTimeout(poll,800);}catch{$('#status').textContent='连接中断，请重新连接设备热点';polling=false;$('#save').disabled=false;}};poll();}catch(err){$('#status').textContent=err.message;polling=false;$('#save').disabled=false;}};
</script></html>)HTML";
esp_err_t response(httpd_req_t* r,const char* data,const char* type="application/json",const char* code="200 OK"){
  httpd_resp_set_status(r,code);httpd_resp_set_type(r,type);httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_send(r,data,HTTPD_RESP_USE_STRLEN);
}
esp_err_t root(httpd_req_t* r){return response(r,portal,"text/html; charset=utf-8");}
esp_err_t redirect(httpd_req_t* r){httpd_resp_set_hdr(r,"Location","http://192.168.4.1/");return response(r,"","text/plain","302 Found");}
esp_err_t scan(httpd_req_t* r){std::lock_guard<std::mutex> g(lock);return response(r,scanJSON.c_str());}
esp_err_t status(httpd_req_t* r){std::lock_guard<std::mutex> g(lock);char b[160];snprintf(b,sizeof(b),"{\"stage\":\"%s\",\"tries\":%d,\"err\":\"%s\"}",provStageName(stage),int(tries),error);return response(r,b);}
esp_err_t save(httpd_req_t* r){
  if(stage!=PROV_WAIT&&stage!=PROV_FAIL)return response(r,"{\"error\":\"busy\"}","application/json","409 Conflict");
  if(r->content_len<2||r->content_len>512)return response(r,"{}","application/json","400 Bad Request");
  char b[513];size_t n=0;while(n<r->content_len){int got=httpd_req_recv(r,b+n,r->content_len-n);if(got<=0)return ESP_FAIL;n+=got;}b[n]=0;
  cJSON* j=cJSON_Parse(b);auto s=cJSON_GetObjectItemCaseSensitive(j,"ssid");auto p=cJSON_GetObjectItemCaseSensitive(j,"password");
  const bool valid=cJSON_IsString(s)&&cJSON_IsString(p)&&strlen(s->valuestring)>0&&strlen(s->valuestring)<=32&&strlen(p->valuestring)<=63&&(strlen(p->valuestring)==0||strlen(p->valuestring)>=8);
  Submission sub{};if(valid){strcpy(sub.ssid,s->valuestring);strcpy(sub.pass,p->valuestring);}cJSON_Delete(j);
  if(!valid)return response(r,"{\"error\":\"invalid_wifi\"}","application/json","400 Bad Request");
  return xQueueSend(submissions,&sub,0)==pdTRUE?response(r,"{\"accepted\":true}"):response(r,"{\"error\":\"busy\"}","application/json","503 Service Unavailable");
}
}
const char* provStageName(ProvStage s){switch(s){case PROV_AP:return "ap";case PROV_WAIT:return "wait";case PROV_VERIFY:return "verify";case PROV_OK:return "ok";case PROV_FAIL:return "fail";default:return "idle";}}
bool provActive(){return stage!=PROV_IDLE;}
ProvStage provStage(){return stage;}
const char* provTargetSsid(){return target.ssid;}
const char* provLastErr(){return error;}
int provTries(){return tries;}
const char* provApSsid(){return apSSID;}
const char* provApPass(){return apPass;}
void provRetry(){if(stage==PROV_FAIL){tries=0;stage=PROV_WAIT;}}
bool provLoadCreds(char* ssid,size_t n,char* pass,size_t m){
  Preferences p;if(!p.begin("clawd",true))return false;
  Submission sub{};
  if(p.getBytesLength("wifi")==sizeof(sub)&&p.getBytes("wifi",&sub,sizeof(sub))==sizeof(sub)){
    sub.ssid[32]=sub.pass[64]=0;p.end();snprintf(ssid,n,"%s",sub.ssid);snprintf(pass,m,"%s",sub.pass);return sub.ssid[0];
  }
  String s=p.getString("ssid","");String w=p.getString("pass","");p.end();
  if(s.isEmpty())return false;snprintf(ssid,n,"%s",s.c_str());snprintf(pass,m,"%s",w.c_str());return true;
}
bool provSaveCreds(const char* ssid,const char* pass){
  Preferences p;if(!p.begin("clawd",false))return false;
  // One blob avoids a power failure leaving a new SSID with an old password.
  Submission sub{};snprintf(sub.ssid,sizeof(sub.ssid),"%s",ssid);snprintf(sub.pass,sizeof(sub.pass),"%s",pass);
  bool ok=p.putBytes("wifi",&sub,sizeof(sub))==sizeof(sub);p.end();return ok;
}
void provForgetCreds(){Preferences p;if(p.begin("clawd",false)){p.remove("wifi");p.remove("ssid");p.remove("pass");p.end();}}
void provBegin(){
  if(started)return;started=true;submissions=xQueueCreate(2,sizeof(Submission));
  snprintf(apSSID,sizeof(apSSID),"ThinCube-Setup-%06X",unsigned(ESP.getEfuseMac()>>24)&0xffffff);
  Preferences p;
  if(p.begin("thincube",false)){
    String saved=p.isKey("apPass")?p.getString("apPass"):String();
    bool valid=saved.length()==8;
    for(unsigned i=0;i<saved.length();i++)if(saved[i]<'0'||saved[i]>'9')valid=false;
    if(!valid){
      char digits[9];snprintf(digits,sizeof(digits),"%08u",unsigned(esp_random()%100000000));
      saved=p.putString("apPass",digits)==8?String(digits):String();
    }
    if(saved.length()==8)strcpy(apPass,saved.c_str());
    p.end();
  }
  if(strlen(apPass)!=8){strcpy(error,"password storage failed");stage=PROV_FAIL;return;}
  WiFi.mode(WIFI_AP_STA);if(!WiFi.softAP(apSSID,apPass)){strcpy(error,"hotspot failed");stage=PROV_FAIL;return;}dns.start(53,"*",IPAddress(192,168,4,1));
  httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.max_uri_handlers=9;cfg.recv_wait_timeout=5;
  if(httpd_start(&server,&cfg)!=ESP_OK){strcpy(error,"portal failed");stage=PROV_FAIL;return;}
  const httpd_uri_t routes[]={
    {.uri="/",.method=HTTP_GET,.handler=root}, {.uri="/scan",.method=HTTP_GET,.handler=scan},
    {.uri="/status",.method=HTTP_GET,.handler=status}, {.uri="/save",.method=HTTP_POST,.handler=save},
    {.uri="/generate_204",.method=HTTP_GET,.handler=redirect}, {.uri="/hotspot-detect.html",.method=HTTP_GET,.handler=redirect},
    {.uri="/connecttest.txt",.method=HTTP_GET,.handler=redirect}, {.uri="/ncsi.txt",.method=HTTP_GET,.handler=redirect},
  };
  for(auto& route:routes)httpd_register_uri_handler(server,&route);
  WiFi.scanNetworks(true);scanPending=true;stage=PROV_WAIT;uiLog("wifi setup");
}
void provBeginWithCredentials(const char* ssid,const char* pass){provBegin();Submission s{};snprintf(s.ssid,sizeof(s.ssid),"%s",ssid);snprintf(s.pass,sizeof(s.pass),"%s",pass);xQueueSend(submissions,&s,0);}
void provTick(){
  if(!started)return;dns.processNextRequest();
  if(scanPending){int n=WiFi.scanComplete();if(n>=0){auto list=cJSON_CreateArray();for(int i=0;i<n&&i<20;i++){auto item=cJSON_CreateObject();cJSON_AddStringToObject(item,"s",WiFi.SSID(i).c_str());cJSON_AddNumberToObject(item,"r",WiFi.RSSI(i));cJSON_AddItemToArray(list,item);}char* j=cJSON_PrintUnformatted(list);if(j){std::lock_guard<std::mutex> g(lock);scanJSON=j;cJSON_free(j);}cJSON_Delete(list);WiFi.scanDelete();scanPending=false;}}
  Submission sub;const uint32_t ms=millis();
  if((stage==PROV_WAIT||stage==PROV_FAIL)&&xQueueReceive(submissions,&sub,0)==pdTRUE){
    target=sub;tries=1;{std::lock_guard<std::mutex> g(lock);error[0]=0;}WiFi.disconnect(false,false);WiFi.begin(target.ssid,target.pass);verifyAt=ms;stage=PROV_VERIFY;
  }
  if(stage==PROV_VERIFY){
    if(WiFi.status()==WL_CONNECTED&&WiFi.SSID()==target.ssid){
      if(provSaveCreds(target.ssid,target.pass)){stage=PROV_OK;okAt=ms;memset(target.pass,0,sizeof(target.pass));}
      else{std::lock_guard<std::mutex> g(lock);strcpy(error,"storage failed");stage=PROV_FAIL;}
    }else if(ms-verifyAt>12000){
      if(tries>=PROV_MAX_TRIES){std::lock_guard<std::mutex> g(lock);strcpy(error,"network or password");stage=PROV_FAIL;WiFi.disconnect(false,false);}
      else{tries++;WiFi.disconnect(false,false);WiFi.begin(target.ssid,target.pass);verifyAt=ms;}
    }
  }
  if(stage==PROV_OK&&ms-okAt>2000)ESP.restart();
}
