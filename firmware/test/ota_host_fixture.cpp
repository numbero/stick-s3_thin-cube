// Included after the real ota.cpp in one translation unit. This deliberately
// reaches its anonymous namespace without exposing test hooks in production.
namespace {
constexpr const char* TargetVersion="v0.4.65";
unsigned scenarios=0;
void resetWorker(){
  host=OtaHost{};stage=OTA_IDLE;cancel=false;written=0;sessionGeneration=0;
  bootNotice=OtaBootNotice::None;
  std::memset(token,0,sizeof(token));std::memset(version,0,sizeof(version));
  std::memset(error,0,sizeof(error));std::memset(digest,0,sizeof(digest));
  size=since=doneAt=batteryAt=0;battery=-1;server=nullptr;started=false;++scenarios;
}
std::vector<uint8_t> image(size_t length=9000){
  std::vector<uint8_t> bytes(length,0x5a);
  bytes[0]=0xe9;bytes[12]=9;bytes[13]=0;
  bytes[32]=0x32;bytes[33]=0x54;bytes[34]=0xcd;bytes[35]=0xab;
  std::memset(bytes.data()+288,0,64);
  std::strcpy(reinterpret_cast<char*>(bytes.data()+288),"ThinCube-StickS3-v1");
  std::strcpy(reinterpret_cast<char*>(bytes.data()+320),TargetVersion);
  return bytes;
}
std::string digestHex(){
  char result[65];for(size_t i=0;i<host.hash.size();++i)std::snprintf(result+i*2,3,"%02x",host.hash[i]);return result;
}
httpd_req_t jsonRequest(const std::string& body){
  httpd_req_t request;request.body.assign(body.begin(),body.end());request.content_len=request.body.size();return request;
}
httpd_req_t manifest(size_t length,const char* ver=TargetVersion){
  return jsonRequest("{\"size\":"+std::to_string(length)+",\"sha256\":\""+digestHex()+"\",\"ver\":\""+ver+"\"}");
}
httpd_req_t prepare(const std::vector<uint8_t>& bytes,bool confirm=true){
  otaSetBattery(85);auto request=manifest(bytes.size());
  assert(begin(&request)==ESP_OK&&request.responseStatus=="200 OK");
  assert(stage==OTA_CONFIRM&&token[0]&&host.erases==0);
  if(confirm){otaConfirm(otaSessionGeneration());assert(stage==OTA_READY);}
  httpd_req_t uploadRequest;uploadRequest.body=bytes;uploadRequest.content_len=bytes.size();
  uploadRequest.headers["X-ThinCube-OTA"]=token;return uploadRequest;
}
void unchangedBoot(){assert(host.bootPartition==&runningPartition&&!host.commits&&!host.handleOpen);}
void failed(const char* why){assert(stage==OTA_FAIL);assert(std::string(otaLastErr())==why);unchangedBoot();}

void admissionAndManifest(){
  resetWorker();host.paired=false;otaSetBattery(85);auto denied=manifest(9000);
  begin(&denied);assert(denied.responseStatus=="401 Unauthorized"&&denied.receiveCalls==0&&stage==OTA_IDLE);
  resetWorker();host.maintenance=true;host.maintenanceAuthorized=false;otaSetBattery(85);denied=manifest(9000);
  begin(&denied);assert(denied.responseStatus=="401 Unauthorized"&&host.erases==0);
  resetWorker();host.maintenance=true;host.maintenanceReady=false;otaSetBattery(85);denied=manifest(9000);
  begin(&denied);assert(denied.responseStatus=="401 Unauthorized");
  for(int barrier=0;barrier<8;++barrier){
    resetWorker();otaSetBattery(85);
    if(barrier==0)host.maintenancePending=true;
    if(barrier==1)host.setup=true;
    if(barrier==2)host.provisioning=true;
    if(barrier==3)host.calibration=true;
    if(barrier==4)host.sleep=true;
    if(barrier==5)host.switching=true;
    if(barrier==6)host.imageState=ESP_OTA_IMG_PENDING_VERIFY;
    if(barrier==7)host.haveSpare=false;
    auto request=manifest(9000);begin(&request);
    assert(request.responseStatus=="409 Conflict"&&stage==OTA_IDLE&&host.erases==0);
  }
  for(int invalid=0;invalid<3;++invalid){
    resetWorker();otaSetBattery(85);
    auto request=manifest(invalid==0?351:9000,invalid==1?"bad\nversion":invalid==2?"":TargetVersion);
    begin(&request);assert(request.responseStatus=="400 Bad Request"&&stage==OTA_IDLE&&host.erases==0);
  }
  resetWorker();otaSetBattery(49);auto low=manifest(9000);begin(&low);assert(low.responseStatus=="409 Conflict");
  resetWorker();otaSetBattery(85);host.clock+=15001;auto stale=manifest(9000);begin(&stale);assert(stale.responseStatus=="409 Conflict");
}

void preEraseRejections(){
  resetWorker();auto body=image();auto request=prepare(body);host.paired=false;
  upload(&request);assert(request.responseStatus=="401 Unauthorized"&&host.erases==0&&request.receiveCalls==0);
  resetWorker();request=prepare(body,false);upload(&request);
  assert(request.response.find("confirm_on_device")!=std::string::npos&&host.erases==0&&stage==OTA_CONFIRM);
  resetWorker();request=prepare(body);request.headers["X-ThinCube-OTA"]="incorrect";upload(&request);
  assert(request.responseStatus=="401 Unauthorized"&&host.erases==0&&stage==OTA_READY);
  resetWorker();request=prepare(body);--request.content_len;upload(&request);
  assert(request.response.find("exact_length_required")!=std::string::npos&&host.erases==0&&stage==OTA_READY);
  for(size_t damagedOffset:{size_t(0),size_t(12),size_t(13),size_t(32),size_t(288),size_t(320)}){
    resetWorker();body=image();body[damagedOffset]^=0x40;request=prepare(body);upload(&request);
    failed("wrong_device_image");assert(host.erases==0&&host.writeCalls==0&&host.ends==0&&host.aborts==0);
  }
}

void successAndWorkerFailures(){
  resetWorker();host.maintenance=true;const auto body=image();auto request=prepare(body);request.maxChunk=311;
  upload(&request);
  assert(stage==OTA_DONE&&request.response=="{\"ok\":true}"&&host.erases==1&&host.ends==1&&host.commits==1);
  assert(host.flash==body&&host.hashed==body&&written==body.size()&&otaPct()==100);
  assert(host.bootPartition==&sparePartition&&host.effects.front()=="erase spare"&&host.effects.back()=="set boot spare");
  assert(!host.handleOpen&&host.aborts==0&&host.writeCalls==3&&host.touches>=5);
  otaTick(doneAt-1);assert(host.restarts==0);otaTick(doneAt+2000);assert(host.restarts==0);
  otaTick(doneAt+2001);assert(host.restarts==1);

  resetWorker();request=prepare(body);request.disconnectAt=4096+128;upload(&request);
  failed("connection_lost");assert(host.erases==1&&host.aborts==1&&host.ends==0&&host.flash.size()==4096);
  resetWorker();request=prepare(body);host.hash[0]^=0xff;upload(&request);
  failed("sha256_mismatch");assert(host.aborts==1&&host.ends==0&&host.flash==body);
  resetWorker();request=prepare(body);host.failErase=true;upload(&request);
  failed("erase_failed");assert(host.erases==1&&host.writeCalls==0&&host.aborts==0);
  resetWorker();request=prepare(body);host.failWriteAt=2;upload(&request);
  failed("write_failed");assert(host.aborts==1&&host.ends==0&&host.writeCalls==2&&host.flash.size()==4096);
  resetWorker();request=prepare(body);host.failEnd=true;upload(&request);
  failed("image_verification_failed");assert(host.ends==1&&host.aborts==0);
  // esp_ota_end consumes its handle even on verification failure. There must
  // be neither a switch nor a second abort of that already-consumed handle.
  resetWorker();request=prepare(body);host.failSetBoot=true;upload(&request);
  assert(stage==OTA_FAIL&&std::string(otaLastErr())=="set_boot_failed");
  assert(host.commits==1&&host.bootPartition==&runningPartition&&!host.handleOpen&&host.aborts==0);

  resetWorker();request=prepare(body);request.timeoutReads=2;request.receiveMs=6000;upload(&request);
  failed("receive_timeout");assert(host.erases==0&&host.aborts==0);
}

void cancellationAndTiming(){
  const auto body=image();
  resetWorker();auto request=prepare(body,false);assert(otaCancel());
  failed("cancelled");assert(!token[0]&&host.erases==0);
  resetWorker();request=prepare(body);assert(otaCancel());
  failed("cancelled");assert(!token[0]&&host.erases==0);

  resetWorker();request=prepare(body,false);const uint32_t oldGeneration=otaSessionGeneration();
  otaConfirm(oldGeneration-1);assert(stage==OTA_CONFIRM);
  upload(&request);assert(host.erases==0&&stage==OTA_CONFIRM);
  assert(otaCancel());otaDismiss();auto next=manifest(body.size());begin(&next);
  assert(otaSessionGeneration()!=oldGeneration&&stage==OTA_CONFIRM);
  otaConfirm(oldGeneration);assert(stage==OTA_CONFIRM&&host.erases==0);
  otaConfirm(otaSessionGeneration());assert(stage==OTA_READY);

  resetWorker();request=prepare(body);bool cancelledAtHash=false;
  host.onShaFinish=[&]{assert(stage==OTA_RECV);cancelledAtHash=otaCancel();};
  upload(&request);failed("cancelled");
  assert(cancelledAtHash&&host.aborts==1&&host.ends==0&&host.flash==body);

  resetWorker();request=prepare(body);bool cancellationRefused=false;
  host.onEnd=[&]{assert(stage==OTA_VERIFY);cancellationRefused=!otaCancel();};
  upload(&request);assert(cancellationRefused&&stage==OTA_DONE&&host.commits==1&&!host.aborts);
  assert(!otaCancel()&&stage==OTA_DONE);

  resetWorker();request=prepare(body);bool tickSawReceiving=false;
  request.beforeReceive=[&](httpd_req_t&){
    if(tickSawReceiving)return;
    tickSawReceiving=true;assert(stage==OTA_RECV);
    otaTick(since+30001);assert(stage==OTA_RECV);
  };
  upload(&request);assert(tickSawReceiving&&stage==OTA_DONE&&host.commits==1);

  resetWorker();request=prepare(body);const uint32_t readySince=since;
  otaTick(readySince-1);assert(stage==OTA_READY&&!host.restarts);
  otaTick(readySince+30000);assert(stage==OTA_READY);
  otaTick(readySince+30001);failed("timeout");assert(!token[0]);

  resetWorker();request=prepare(body);request.receiveMs=0;host.clock=0xffffff00u;
  since=host.clock;
  otaTick(0x0000742fu);assert(stage==OTA_READY); // 29999 ms across uptime wrap.
  otaTick(0x00007431u);failed("timeout");
}

void rollbackHealth(){
  resetWorker();assert(verifyRollbackLater());host.imageState=ESP_OTA_IMG_PENDING_VERIFY;
  host.clock=59999;assert(!otaMarkValid()&&host.markValidCalls==0);
  host.clock=60000;host.localHealthy=false;assert(!otaMarkValid()&&host.markValidCalls==0);
  host.localHealthy=true;host.imuHealthy=false;assert(!otaMarkValid()&&host.markValidCalls==0);
  host.imuHealthy=true;host.failMarkValid=true;assert(!otaMarkValid()&&host.markValidCalls==1);
  host.failMarkValid=false;assert(otaMarkValid()&&host.markValidCalls==2&&host.imageState==ESP_OTA_IMG_VALID);
  otaRollbackUnhealthy();assert(!host.rollbackCalls);
  resetWorker();host.imageState=ESP_OTA_IMG_PENDING_VERIFY;host.clock=90000;
  otaRollbackUnhealthy();assert(!host.rollbackCalls);
  host.clock=90001;otaRollbackUnhealthy();assert(host.rollbackCalls==1);
}

void bootHealthNotice(){
  // A normal confirmed boot never announces an upgrade completion.
  resetWorker();otaBootBegin();assert(otaBootNotice()==OtaBootNotice::None);
  assert(otaMarkValid()&&host.markValidCalls==0&&otaBootNotice()==OtaBootNotice::None);

  // The warning remains until the actual IDF mark succeeds, including health,
  // timing, metadata-read and mark failures. Button dismissal cannot hide it.
  resetWorker();host.imageState=ESP_OTA_IMG_PENDING_VERIFY;host.clock=2000;
  otaBootBegin();assert(otaBootNotice()==OtaBootNotice::Checking&&otaBootElapsedSec()==2);
  otaBootDismiss();assert(otaBootNotice()==OtaBootNotice::Checking);
  host.clock=59999;assert(!otaMarkValid()&&host.markValidCalls==0);
  assert(otaBootNotice()==OtaBootNotice::Checking);
  host.clock=60000;host.localHealthy=false;assert(!otaMarkValid());
  host.localHealthy=true;host.imuHealthy=false;assert(!otaMarkValid());
  host.imuHealthy=true;host.failGetState=true;assert(!otaMarkValid()&&host.markValidCalls==0);
  host.failGetState=false;host.failMarkValid=true;assert(!otaMarkValid()&&host.markValidCalls==1);
  assert(otaBootNotice()==OtaBootNotice::Checking);
  host.failMarkValid=false;assert(otaMarkValid()&&host.markValidCalls==2);
  assert(host.imageState==ESP_OTA_IMG_VALID&&otaBootNotice()==OtaBootNotice::Complete);
  host.clock=65000;assert(otaMarkValid()&&host.markValidCalls==2);
  assert(otaBootNotice()==OtaBootNotice::Complete);
  otaBootDismiss();assert(otaBootNotice()==OtaBootNotice::None);
  otaBootBegin();assert(otaBootNotice()==OtaBootNotice::None);

  // A metadata read failure does not fabricate either a trial or success.
  resetWorker();host.failGetState=true;otaBootBegin();
  assert(otaBootNotice()==OtaBootNotice::None&&!otaMarkValid()&&host.markValidCalls==0);
}
}

int main(){
  admissionAndManifest();preEraseRejections();successAndWorkerFailures();cancellationAndTiming();rollbackHealth();bootHealthNotice();
  std::printf("real ota.cpp host harness: %u scenarios passed (ASan/UBSan)\n",scenarios);
}
