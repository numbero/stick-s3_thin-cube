#pragma once
#include <stdint.h>
#include <string.h>
#include <algorithm>

// Independent 10ms IMA blocks: predictor LE16, step index, zero, 159 low-first
// nibbles (last high nibble is zero). A lost BLE packet cannot corrupt the next.
namespace ThinCubeIMA {
static constexpr int steps[89]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static constexpr int adjust[8]={-1,-1,-1,-1,2,4,6,8};
inline void encode160(const int16_t* samples,uint8_t* block,int& index){
  index=std::max(0,std::min(88,index));memset(block,0,84);
  int predictor=samples[0];block[0]=predictor&255;block[1]=(predictor>>8)&255;block[2]=index;
  for(int i=1;i<160;i++){
    int delta=int(samples[i])-predictor,code=0,step=steps[index],difference=step>>3;
    if(delta<0){code=8;delta=-delta;}
    if(delta>=step){code|=4;delta-=step;difference+=step;}
    step>>=1;if(delta>=step){code|=2;delta-=step;difference+=step;}
    step>>=1;if(delta>=step){code|=1;difference+=step;}
    predictor=std::max(-32768,std::min(32767,predictor+(code&8?-difference:difference)));
    index=std::max(0,std::min(88,index+adjust[code&7]));
    block[4+(i-1)/2]|=uint8_t(code<<(((i-1)&1)*4));
  }
}
}
