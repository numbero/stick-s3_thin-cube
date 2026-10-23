#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Fixed custom descriptor contract shared with package-firmware.py and the phone page.
namespace OtaImagePolicy {
constexpr size_t MinimumSize=352, MaximumSize=2*1024*1024;
inline bool versionValid(const char* value) {
  if(!value||!value[0])return false;
  size_t n=0;
  for(;n<32&&value[n];++n){
    const unsigned char c=value[n];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'||c=='_'||c=='+'))return false;
  }
  return n<32;
}
inline bool prefixValid(const uint8_t* data,size_t length,const char* expectedVersion) {
  static const char identity[32]="ThinCube-StickS3-v1";
  return data&&length>=MinimumSize&&data[0]==0xe9&&data[12]==9&&data[13]==0&&
    data[32]==0x32&&data[33]==0x54&&data[34]==0xcd&&data[35]==0xab&&
    !memcmp(data+288,identity,sizeof(identity))&&versionValid(expectedVersion)&&
    !strncmp(reinterpret_cast<const char*>(data+320),expectedVersion,32);
}
}
