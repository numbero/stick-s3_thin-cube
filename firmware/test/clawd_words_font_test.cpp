#include "../src/clawd_words_font.h"
#include <array>
#include <assert.h>
#include <stdio.h>
struct Canvas {
  std::array<uint16_t,135*240> pixels{};
  void fillRect(int x,int y,int w,int h,uint16_t ink){
    assert(x>=0&&x+w<=135&&y>=0&&y+h<=240);
    for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)pixels[yy*135+xx]=ink;
  }
};
int main(){
  uint8_t advances[95];const uint8_t* bitmaps[95];
  uint8_t empty[16]={},wide[16]={},narrow[16]={};
  // Deliberately different bearings and a full descender: trimming may only
  // remove empty columns, with no font scaling, clipped ink or merged letters.
  wide[3]=0x38;wide[15]=0x28;narrow[0]=0x20;narrow[12]=0x20;
  for(unsigned i=0;i<95;i++){advances[i]=7;bitmaps[i]=empty;}
  bitmaps['A'-32]=wide;bitmaps['i'-32]=narrow;bitmaps['?'-32]=narrow;
  ClawdWords::Font font;font.begin(advances,bitmaps);
  assert(font.width("")==0&&font.width("A")==3&&font.width("i")==1);
  assert(font.width("Ai")==5&&font.width("A i")==8);
  assert(font.width("\x01")==font.width("?"));
  Canvas canvas;font.draw(canvas,"Ai",10,169,0x1234);
  assert(canvas.pixels[172*135+10]==0x1234&&canvas.pixels[172*135+12]==0x1234);
  assert(canvas.pixels[184*135+10]==0x1234&&canvas.pixels[184*135+12]==0x1234);
  assert(canvas.pixels[169*135+14]==0x1234&&canvas.pixels[181*135+14]==0x1234);
  for(int y=0;y<240;y++)for(int x=0;x<135;x++){
    if(x==13||y<169||y>=185||x<10||x>14)assert(canvas.pixels[y*135+x]==0);
  }
  puts("Words font: variable bearings, native ink, clear letter gaps, descenders and fallback passed");
}
