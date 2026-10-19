#include "../src/tap_storage.h"
#include "../src/tap_transaction.h"
#include <assert.h>
#include <stdio.h>
#include <vector>

struct Store {
  std::vector<uint8_t> bytes;
  unsigned reads=0,writes=0,failReadAt=0,failWriteAt=0;
  bool commitBeforeFailure=false,alwaysFailWrites=false;
  TapStoredRead read(uint8_t* out,size_t capacity,size_t& size){
    if(++reads==failReadAt)return TapStoredRead::Failed;
    size=bytes.size();if(bytes.empty())return TapStoredRead::Missing;
    if(size<=capacity)memcpy(out,bytes.data(),size);
    return TapStoredRead::Found;
  }
  bool write(const uint8_t* data,size_t size){
    ++writes;const bool fail=alwaysFailWrites||writes==failWriteAt;
    if(!fail||commitBeforeFailure)bytes.assign(data,data+size);
    return !fail;
  }
};
static bool same(const TapParameters& a,const TapParameters& b){
  uint8_t x[TapRecordSize]{},y[TapRecordSize]{};
  return tapEncodeRecord(a,x)&&tapEncodeRecord(b,y)&&!memcmp(x,y,sizeof(x));
}
static void checksum(uint8_t (&record)[TapRecordSize]){
  const auto crc=tapRecordCRC(record,TapRecordSize-4);
  for(unsigned i=0;i<4;++i)record[TapRecordSize-4+i]=uint8_t(crc>>(8*i));
}
int main(){
  TapParameters custom;custom.enabled=false;custom.single=true;custom.doubleTap=false;custom.triple=true;
  custom.filtered=false;custom.wait=false;custom.manual=true;custom.software=false;
  custom.axis=1;custom.sensitivity=3;custom.windowMs=755;custom.quietMs=455;
  custom.settleMs=1250;custom.buttonGuardMs=850;custom.cooldownMs=900;
  custom.softThresholdMg=731;custom.softReboundMs=210;custom.softWindowMs=605;
  custom.softGyroDps=173;custom.softMotionGuardMs=433;custom.softBaselineMs=333;
  custom.softGyroFilterMs=123;custom.softRatioPercent=73;
  uint8_t record[TapRecordSize]{};assert(tapEncodeRecord(custom,record));
  TapParameters decoded;assert(tapDecodeRecord(record,sizeof(record),decoded)&&same(custom,decoded));
  for(unsigned byte=0;byte<TapRecordSize;++byte)for(unsigned bit=0;bit<8;++bit){
    record[byte]^=1u<<bit;TapParameters unchanged;
    assert(!tapDecodeRecord(record,sizeof(record),unchanged)&&same(unchanged,TapParameters()));
    record[byte]^=1u<<bit;
  }
  assert(!tapDecodeRecord(record,0,decoded)&&!tapDecodeRecord(record,TapRecordSize-1,decoded)&&!tapDecodeRecord(record,TapRecordSize+1,decoded));
  record[4]=2;checksum(record);assert(!tapDecodeRecord(record,sizeof(record),decoded));
  assert(tapEncodeRecord(custom,record));record[6]=3;checksum(record); // valid CRC, invalid axis
  assert(!tapDecodeRecord(record,sizeof(record),decoded));
  custom.axis=3;assert(!tapEncodeRecord(custom,record));custom.axis=1;

  Store store;TapParameters loaded;
  assert(tapLoadSettings(store,loaded)==TapLoadState::Defaults&&store.writes==0);
  assert(tapSaveSettings(store,custom)&&store.writes==1);
  assert(tapSaveSettings(store,custom)&&store.writes==1); // no redundant commit
  Store rebooted;rebooted.bytes=store.bytes;
  assert(tapLoadSettings(rebooted,loaded)==TapLoadState::Saved&&same(custom,loaded));
  rebooted.bytes[10]^=1;loaded=TapParameters();
  assert(tapLoadSettings(rebooted,loaded)==TapLoadState::Invalid&&same(loaded,TapParameters())&&rebooted.writes==0);
  rebooted.bytes.resize(TapRecordSize+1);
  assert(tapLoadSettings(rebooted,loaded)==TapLoadState::Invalid);
  rebooted.failReadAt=rebooted.reads+1;assert(tapLoadSettings(rebooted,loaded)==TapLoadState::Unavailable);

  TapParameters original,next;next.softThresholdMg=700;next.softWindowMs=550;
  for(unsigned failure=0;failure<5;++failure){
    Store disk;assert(tapSaveSettings(disk,original));disk.reads=disk.writes=0;
    if(failure==1)disk.failWriteAt=1;
    if(failure==2){disk.failWriteAt=1;disk.commitBeforeFailure=true;}
    if(failure==3)disk.failReadAt=2; // acknowledgement read fails after successful commit
    if(failure==4){disk.alwaysFailWrites=true;disk.commitBeforeFailure=true;}
    TapParameters current=original;bool verified=true,saved=true;unsigned configurations=0;
    const char* result=tapSaveTransaction(current,verified,saved,next,
      [&](bool on){++configurations;assert(on);return true;},[&](const TapParameters& p){return tapSaveSettings(disk,p);});
    assert(verified);
    if(failure==0){assert(!strcmp(result,"ok")&&saved&&same(current,next)&&configurations==1);}
    else {
      assert(same(current,original)&&configurations==2);
      assert(!strcmp(result,failure==4?"storage_rollback_failed":"storage_failed_restored"));
      assert(saved==(failure!=4));
    }
    Store boot;boot.bytes=disk.bytes;TapParameters actual;
    assert(tapLoadSettings(boot,actual)==TapLoadState::Saved&&same(actual,failure==0?next:original));
  }
  bool verified=true,saved=true;unsigned configs=0,persists=0;TapParameters current;
  const char* result=tapSaveTransaction(current,verified,saved,next,
    [&](bool){return ++configs==2;},[&](const TapParameters&){++persists;return true;});
  assert(!strcmp(result,"write_failed_restored")&&verified&&saved&&persists==0&&same(current,original));
  configs=0;
  result=tapSaveTransaction(current,verified,saved,next,[&](bool on){++configs;if(configs==3)assert(!on);return false;},
    [&](const TapParameters&){++persists;return true;});
  assert(!strcmp(result,"readback_failed")&&!verified&&configs==3&&persists==0);
  puts("tap persistence: all fields, schema/CRC/bounds, reboot loading, deduplication, storage failures and rollback passed");
}
