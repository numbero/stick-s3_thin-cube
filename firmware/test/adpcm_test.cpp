#include "../src/ima_adpcm.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(int argc,char** argv){
  int16_t samples[160];uint8_t block[84];int index=0;
  for(auto& sample:samples)sample=0;
  ThinCubeIMA::encode160(samples,block,index);for(auto byte:block)assert(byte==0);
  index=32;
  for(int i=0;i<160;i++)samples[i]=int16_t(12000*sin(2*3.141592653589793*440*i/16000));
  ThinCubeIMA::encode160(samples,block,index);assert(block[2]<=88&&block[3]==0&&(block[83]&0xf0)==0);
  if(argc==2){auto file=fopen(argv[1],"wb");assert(file);assert(fwrite(block,1,84,file)==84);fclose(file);}
  puts("IMA independent block encoding passed");
}
