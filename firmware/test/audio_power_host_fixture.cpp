static void prepare(bool ble){
  audioTick(false);powerSavePerformance();
  const auto next=host.now+2000;
  host=AudioPowerHost{};host.now=next;M5.Mic=HostMic{};audioBegin(ble);
}
int main(){
  for(bool ble:{false,true}){
    prepare(ble);
    powerSaveTick(false,true);
    assert(host.cpu==160&&powerSaveReduced()&&!powerSaveActive()&&!host.bleIdle);
    const unsigned changes=host.frequencyChanges;
    const unsigned count=ble?160:480,rate=ble?16000:48000;
    std::array<int16_t,482> pcm{};
    // Continuous RMS capture must not bounce 160->240->160 on every loop, or
    // stop/reconfigure a warm microphone before voice can take it over.
    for(unsigned i=0;i<100;++i){
      host.now+=10;audioTick(true,false);M5.Mic.complete(8192);
      pcm.fill(1234);assert(audioTake(pcm.data()+1));
      assert(pcm.front()==1234&&pcm[count+1]==1234);
      assert(std::abs(audioRms()-.25f)<.00001f);
      powerSaveTick(false,true);
    }
    assert(host.frequencyChanges==changes&&host.cpu==160&&!host.bleIdle);
    assert(host.begins==1&&host.ends==0&&audioSampleRate()==rate);
    // Existing buffers and sample rate carry directly into voice, with no
    // additional codec restart, queue replacement or first-frame warmup.
    audioTick(true);assert(host.cpu==240&&!powerSaveReduced()&&!host.bleIdle);
    assert(host.begins==1&&host.ends==0&&audioSampleRate()==rate);
    M5.Mic.complete(4096);assert(audioTake(pcm.data()+1));
    assert(std::abs(audioRms()-.125f)<.00001f);
    // Stop joins any late callback, discards old PCM and keeps standby's
    // slower radio separate from the responsive visible home profile.
    audioTick(false);assert(!audioReady()&&!audioTake(pcm.data()+1));
    powerSaveTick(true);assert(host.cpu==160&&powerSaveActive()&&host.bleIdle);
    powerSaveTick(false,true);assert(host.cpu==160&&!powerSaveActive()&&!host.bleIdle);
    audioTick(true);assert(host.cpu==240&&!host.bleIdle&&audioReady());
  }
  {
    prepare(true);powerSaveTick(false,true);audioTick(true,false);
    host.recordFails=true;M5.Mic.complete(100);assert(!audioReady());
    const unsigned begins=host.begins;audioTick(true,false);
    assert(!audioReady()&&host.cpu==160&&host.begins==begins);
    host.recordFails=false;host.now+=1000;audioTick(true,false);
    assert(audioReady()&&host.cpu==160&&!host.bleIdle);
  }
  {
    prepare(false);host.frequencyFails=true;powerSaveTick(false,true);
    assert(!powerSaveReduced()&&host.cpu==240&&!host.bleIdle);
    host.frequencyFails=false;powerSaveTick(true);
    assert(powerSaveActive()&&host.bleIdle&&host.cpu==160);
    host.frequencyFails=true;powerSavePerformance();
    assert(powerSaveActive()&&host.bleIdle&&host.cpu==160); // no false success
    host.frequencyFails=false;powerSavePerformance();
    assert(!powerSaveReduced()&&!host.bleIdle&&host.cpu==240);
  }
  audioTick(false);
  puts("real audio/power: BLE+Wi-Fi continuous home, warm voice takeover, standby, refill failure and CPU failure passed");
}
