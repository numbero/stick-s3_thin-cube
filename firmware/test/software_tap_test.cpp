#include "../src/software_tap.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <fstream>
#include <sstream>
#include <string>
static std::string fixture(const char* name){return std::string(__FILE__).substr(0,std::string(__FILE__).find_last_of('/'))+"/fixtures/"+name;}
int main(){
  TapParameters p;p.softThresholdMg=400;p.softGyroDps=150;p.softGyroFilterMs=0;
  // Recorded five prompted double taps, 200 Hz FIFO; only one native report.
  for(unsigned phase=0;phase<3;++phase){
    SoftwareTapDetector d;std::ifstream f(fixture("tap_front_fifo.csv"));assert(f.good());
    std::string row;std::getline(f,row);unsigned index=0,hits=0;
    while(std::getline(f,row)){
      unsigned ms;int x,y,z;assert(sscanf(row.c_str(),"%u,%d,%d,%d",&ms,&x,&y,&z)==4);
      // Also exercise both phases of 100 Hz replay, though runtime uses FIFO.
      if(phase==2||(index++%2)==phase)if(d.update(ms,z*(8.f/32768),0,p))++hits;
    }
    printf("recorded phase %u: candidates=%u recognized=%u rejected=%u\n",phase,d.candidates,hits,d.rejected);
    assert(hits==(phase==0?4u:5u));
  }
  // The actual 200 Hz stream and the exact gyro magnitude used at runtime.
  // 150 dps rejects all three operator-confirmed taps; 450 dps recovers each.
  for(unsigned gyro:{150,450}){
    TapParameters q=p;q.softGyroDps=gyro;SoftwareTapDetector d;
    std::ifstream f(fixture("tap_front_gyro_fifo.csv"));assert(f.good());
    std::string row;std::getline(f,row);unsigned hits=0;
    while(std::getline(f,row)){
      unsigned index,ms;int x,y,z;float g;
      assert(sscanf(row.c_str(),"%u,%u,%d,%d,%d,%f",&index,&ms,&x,&y,&z,&g)==6);
      if(d.update(ms,z*(8.f/32768),g,q))++hits;
    }
    assert(hits==(gyro==450?3u:0u));
    if(gyro==150)assert(d.rejectedMotion==3);
  }
  // A looser rotation guard alone admitted two daily-motion false reports.
  // Preserve both traces and verify the calibrated profile on all of them.
  for(const char* name:{"tap_front_gyro_fifo.csv","tap_natural_gyro_fifo.csv"}){
    TapParameters q=p;q.softGyroDps=450;q.softThresholdMg=550;SoftwareTapDetector d;
    std::ifstream f(fixture(name));assert(f.good());std::string row;std::getline(f,row);unsigned hits=0;
    while(std::getline(f,row)){
      unsigned index,ms;int x,y,z;float g;
      assert(sscanf(row.c_str(),"%u,%u,%d,%d,%d,%f",&index,&ms,&x,&y,&z,&g)==6);
      if(d.update(ms,z*(8.f/32768),g,q))++hits;
    }
    assert(hits==(std::string(name)=="tap_front_gyro_fifo.csv"?3u:0u));
  }
  {TapParameters q=p;q.softGyroDps=450;q.softThresholdMg=550;SoftwareTapDetector d;
    std::ifstream f(fixture("tap_front_fifo.csv"));assert(f.good());std::string row;std::getline(f,row);unsigned hits=0;
    while(std::getline(f,row)){unsigned ms;int x,y,z;assert(sscanf(row.c_str(),"%u,%d,%d,%d",&ms,&x,&y,&z)==4);if(d.update(ms,z*(8.f/32768),0,q))++hits;}
    assert(hits==5);
  }
  for(bool filtered:{false,true})for(const char* name:{"flick_handheld_single.csv","flick_handheld_double.csv","flick_handheld_natural.csv"}){
    TapParameters q=p;if(filtered){q.softThresholdMg=550;q.softGyroDps=200;q.softGyroFilterMs=100;}
    SoftwareTapDetector d;std::ifstream f(fixture(name));assert(f.good());std::string row;std::getline(f,row);unsigned hits=0;
    while(std::getline(f,row)){
      unsigned ms;float x,y,z,gx,gy,gz;assert(sscanf(row.c_str(),"%u,%f,%f,%f,%f,%f,%f",&ms,&x,&y,&z,&gx,&gy,&gz)==7);
      if(d.update(ms,z,sqrtf(gx*gx+gy*gy+gz*gz),q))++hits;
    }
    printf("cross-gesture %s: recognized=%u\n",name,hits);assert(hits==0);
  }
  for(const char* name:{"tap_front_gyro_fifo.csv","tap_natural_gyro_fifo.csv","tap_validation_front_gyro_fifo.csv","tap_validation_natural_gyro_fifo.csv"}){
    TapParameters q=p;q.softThresholdMg=550;q.softGyroDps=200;q.softGyroFilterMs=100;SoftwareTapDetector d;
    std::ifstream f(fixture(name));assert(f.good());std::string row;std::getline(f,row);unsigned hits=0;
    while(std::getline(f,row)){
      unsigned index,ms;int x,y,z;float g;
      assert(sscanf(row.c_str(),"%u,%u,%d,%d,%d,%f",&index,&ms,&x,&y,&z,&g)==6);
      if(d.update(ms,z*(8.f/32768),g,q))++hits;
    }
    assert(hits==(std::string(name).find("front")!=std::string::npos?3u:0u));
  }
  for(bool sustained:{false,true}){
    TapParameters q=p;q.softGyroDps=200;q.softGyroFilterMs=100;SoftwareTapDetector d;unsigned hits=0;
    for(unsigned ms=0;ms<3000;ms+=5){
      const float z=(ms>=1000&&ms<1015)||(ms>=1260&&ms<1275)?-1:0;
      const float g=sustained?(ms>=1050&&ms<1300?500:0):(ms==1100?600:0);
      if(d.update(ms,z,g,q))++hits;
    }
    assert(hits==(sustained?0u:1u));
  }
  for(unsigned taps=1;taps<=3;++taps){
    SoftwareTapDetector d;unsigned hits=0;
    for(unsigned ms=0;ms<3000;ms+=5){
      float z=0;
      for(unsigned i=0;i<taps;++i){const unsigned at=1000+i*150;if(ms>=at&&ms<at+15)z=-1;}
      if(d.update(ms,z,0,p))++hits;
    }
    assert(hits==(taps==2?1u:0u));
  }
  {SoftwareTapDetector d;unsigned hits=0;
    for(unsigned ms=0;ms<3000;ms+=5){float z=0;
      if((ms>=1000&&ms<1015)||(ms>=1260&&ms<1275))z=-1;
      if((ms>=1120&&ms<1135)||(ms>=1380&&ms<1395))z=-.45f;
      if(d.update(ms,z,0,p))++hits;
    }assert(hits==1);
  }
  {SoftwareTapDetector d;unsigned hits=0;
    for(unsigned ms=0;ms<3000;ms+=5){float z=0;
      if((ms>=1000&&ms<1015)||(ms>=1260&&ms<1275))z=-1;
      if((ms>=1120&&ms<1135)||(ms>=1380&&ms<1395))z=1.1f;
      if(d.update(ms,z,0,p))++hits;
    }assert(hits==1);
  }
  {SoftwareTapDetector d;unsigned hits=0;
    for(unsigned ms=0;ms<3000;ms+=5){float z=(ms>=1000&&ms<1015)||(ms>=1260&&ms<1275)?-1:0;
      if(d.update(ms,z,ms==1100?300:0,p))++hits;
    }assert(hits==0&&d.rejectedMotion==1);
  }
  puts("recorded tap replay and existing single/double/natural flick rejection passed");
}
