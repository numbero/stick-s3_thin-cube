#include "../src/clawd_story_render.h"
#include <assert.h>
#include <stdio.h>
#include <array>
#include <string>

struct Pixels {
  std::array<uint16_t,135*240> data;
  Pixels(){data.fill(0xffff);for(int y=60;y<170;y++)for(int x=0;x<135;x++)data[y*135+x]=ClawdStoryRaster::Palette::screen;}
  void fillRect(int x,int y,int w,int h,uint16_t c){
    for(int yy=std::max(60,y);yy<std::min(170,y+h);yy++)
      for(int xx=std::max(0,x);xx<std::min(135,x+w);xx++)data[yy*135+xx]=c;
  }
  void drawLine(int x,int y,int ex,int ey,uint16_t c){
    const int dx=abs(ex-x),sx=x<ex?1:-1,dy=-abs(ey-y),sy=y<ey?1:-1;int err=dx+dy;
    for(unsigned n=0;n<1000;n++){fillRect(x,y,1,1,c);if(x==ex&&y==ey)break;
      const int e=err*2;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
  }
  uint64_t hash()const{uint64_t h=1469598103934665603ull;for(auto px:data){h^=px;h*=1099511628211ull;}return h;}
  void save(const std::string& path)const{
    FILE* f=fopen(path.c_str(),"wb");assert(f);fprintf(f,"P6\n135 240\n255\n");
    for(auto p:data){const unsigned char rgb[]={uint8_t((p>>11)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);
  }
};
int main(int argc,char** argv){
  Pixels canonical;
  auto normal=[&](float x,float y,float w,float h,uint16_t c){canonical.fillRect(lroundf(16.35f+x*1.55f),lroundf(77+y*1.55f),ceilf(w*1.55f),ceilf(h*1.55f),c);};
  for(int x:{6,18,42,54})normal(x,39,6,13,ClawdStoryRaster::Palette::body);
  normal(0,13,6,13,ClawdStoryRaster::Palette::body);normal(60,13,6,13,ClawdStoryRaster::Palette::body);
  normal(6,0,54,39,ClawdStoryRaster::Palette::body);
  normal(12,13,6,6.5f,ClawdStoryRaster::Palette::eye);normal(48,13,6,6.5f,ClawdStoryRaster::Palette::eye);
  unsigned frames=0;
  const unsigned keyframes[]={5700,5800,4400,3800,6300,6000,4100,5700,3500,6500,3500,4300,5400};
  static_assert(sizeof(keyframes)/sizeof(*keyframes)==ClawdStory::Count,"Capture every story");
  for(unsigned i=0;i<ClawdStory::Count;i++){
    const auto id=ClawdStory::Id(i);uint64_t first=0;unsigned changes=0;
    for(unsigned ms=0;ms<=ClawdStory::duration(id);ms+=25){
      Pixels image;ClawdStoryRaster::Painter<Pixels>(image,id,ms*.001f).draw();
      if(ms==0){first=image.hash();assert(image.data==canonical.data);}
      if(image.hash()!=first)changes++;
      unsigned visible=0;
      for(unsigned y=0;y<240;y++)for(unsigned x=0;x<135;x++){
        if(y<60||y>=170)assert(image.data[y*135+x]==0xffff);
        else if(image.data[y*135+x]!=ClawdStoryRaster::Palette::screen)visible++;
      }
      assert(visible>0);if(ms==ClawdStory::duration(id))assert(image.data==canonical.data);frames++;
    }
    assert(changes>100);
    if(argc==2){Pixels image;ClawdStoryRaster::Painter<Pixels>(image,id,keyframes[i]*.001f).draw();image.save(std::string(argv[1])+"/"+ClawdStory::name(id)+".ppm");}
  }
  printf("%u actual raster frames: clip boundaries, visible scenes and pixel-exact default restoration passed\n",frames);
}
