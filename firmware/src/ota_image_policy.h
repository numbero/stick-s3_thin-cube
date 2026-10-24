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
// Compare the numeric core of release versions, including diagnostic/RC
// suffixes. Preserve support for explicitly named custom development builds.
inline bool releaseCore(const char* value,uint32_t (&parts)[3]) {
  if(!value)return false;
  if(*value=='v')++value;
  for(unsigned i=0;i<3;++i){
    if(*value<'0'||*value>'9')return false;
    uint32_t number=0;
    while(*value>='0'&&*value<='9'){
      const unsigned digit=unsigned(*value++-'0');
      if(number>(UINT32_MAX-digit)/10)return false;
      number=number*10+digit;
    }
    parts[i]=number;
    if(i<2){if(*value++!='.')return false;}
  }
  return !*value||*value=='-'||*value=='+';
}
inline bool isDowngrade(const char* current,const char* target) {
  uint32_t running[3],next[3];
  if(!releaseCore(current,running)||!releaseCore(target,next))return false;
  for(unsigned i=0;i<3;++i)if(running[i]!=next[i])return next[i]<running[i];
  return false; // Reinstalling the same version still needs a fresh boot check.
}
inline bool prefixValid(const uint8_t* data,size_t length,const char* expectedVersion) {
  static const char identity[32]="ThinCube-StickS3-v1";
  return data&&length>=MinimumSize&&data[0]==0xe9&&data[12]==9&&data[13]==0&&
    data[32]==0x32&&data[33]==0x54&&data[34]==0xcd&&data[35]==0xab&&
    !memcmp(data+288,identity,sizeof(identity))&&versionValid(expectedVersion)&&
    !strncmp(reinterpret_cast<const char*>(data+320),expectedVersion,32);
}
}
