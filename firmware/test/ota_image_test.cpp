#include "../src/ota_image_policy.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

int main(){
  std::array<uint8_t,352> valid{};
  valid[0]=0xe9;valid[12]=9;
  valid[32]=0x32;valid[33]=0x54;valid[34]=0xcd;valid[35]=0xab;
  strcpy(reinterpret_cast<char*>(valid.data()+288),"ThinCube-StickS3-v1");
  memcpy(valid.data()+320,"v0.4.65",7);
  assert(OtaImagePolicy::prefixValid(valid.data(),valid.size(),"v0.4.65"));
  assert(!OtaImagePolicy::prefixValid(valid.data(),351,"v0.4.65"));
  assert(!OtaImagePolicy::prefixValid(nullptr,352,"v0.4.65"));
  assert(!OtaImagePolicy::prefixValid(valid.data(),352,"v0.4.64"));
  // Wrong device/chip/descriptor must be rejected before esp_ota_begin erases the spare slot.
  for(const size_t offset:{size_t(0),size_t(12),size_t(13),size_t(32),size_t(33),size_t(34),size_t(35),size_t(288),size_t(319),size_t(320)}){
    auto corrupt=valid;corrupt[offset]^=1;
    assert(!OtaImagePolicy::prefixValid(corrupt.data(),corrupt.size(),"v0.4.65"));
  }
  assert(OtaImagePolicy::versionValid("v0.4.65-rc1+2"));
  for(const auto invalid:{"","\"bad","v1\n","v1\\","v1 space","01234567890123456789012345678901ab"})assert(!OtaImagePolicy::versionValid(invalid));
  assert(!OtaImagePolicy::versionValid(nullptr));
  assert(OtaImagePolicy::isDowngrade("v0.4.70","v0.4.67"));
  assert(OtaImagePolicy::isDowngrade("v0.4.70","v0.4.67-legacy"));
  assert(OtaImagePolicy::isDowngrade("v1.0.0","v0.99.999"));
  assert(OtaImagePolicy::isDowngrade("v0.5.0","v0.4.999"));
  assert(!OtaImagePolicy::isDowngrade("v0.4.70","v0.4.70"));
  assert(!OtaImagePolicy::isDowngrade("v0.4.70","v0.4.71"));
  assert(!OtaImagePolicy::isDowngrade("v0.4.9","v0.4.10"));
  assert(!OtaImagePolicy::isDowngrade("v0.4.70","v0.5.0"));
  assert(!OtaImagePolicy::isDowngrade("dev","v0.4.70"));
  assert(!OtaImagePolicy::isDowngrade("v0.4.70","dev"));
  uint32_t parts[3];
  assert(!OtaImagePolicy::releaseCore("v0.4",parts));
  assert(!OtaImagePolicy::releaseCore("v0..70",parts));
  assert(!OtaImagePolicy::releaseCore("v0.4.4294967296",parts));
  assert(!OtaImagePolicy::releaseCore(nullptr,parts));
  puts("OTA image identity and displayed target version checked before flash erase");
}
