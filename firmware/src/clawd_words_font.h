#pragma once
#include <stdint.h>

namespace ClawdWords {
// Reuse M5GFX Font2's native 16px bitmaps. Trim only blank side bearings;
// preserve every ink pixel and one clear column between letters.
class Font {
 public:
  static constexpr int Height=16,Space=3;
  void begin(const uint8_t* advances,const uint8_t* const* bitmaps){
    widths=advances;rows=bitmaps;
    for(unsigned i=0;i<95;i++){
      int left=16,right=-1;const int stride=(widths[i]+6)/8;
      for(int y=0;y<Height;y++)for(int x=0;x<int(widths[i])-1;x++){
        if(rows[i][y*stride+x/8]&(0x80>>(x%8))){if(x<left)left=x;if(x>right)right=x;}
      }
      bearing[i]=right<0?0:left;inkWidth[i]=right<0?0:right-left+1;
    }
  }
  int width(const char* text)const{
    int result=0;
    for(const char* p=text;*p;p++)result+=advance(index(*p));
    return result?result-1:0;
  }
  template<class Canvas>void draw(Canvas& canvas,const char* text,int x,int y,uint16_t ink)const{
    if(!rows)return;
    for(const char* p=text;*p;p++){
      const unsigned i=index(*p);const int stride=(widths[i]+6)/8;
      for(int row=0;row<Height;row++){
        int start=-1;
        for(int col=0;col<=inkWidth[i];col++){
          const int bit=col+bearing[i];
          const bool set=col<inkWidth[i]&&(rows[i][row*stride+bit/8]&(0x80>>(bit%8)));
          if(set&&start<0)start=col;
          if(!set&&start>=0){canvas.fillRect(x+start,y+row,col-start,1,ink);start=-1;}
        }
      }
      x+=advance(i);
    }
  }
 private:
  const uint8_t* widths=nullptr;
  const uint8_t* const* rows=nullptr;
  uint8_t bearing[95]{},inkWidth[95]{};
  static unsigned index(char c){const unsigned i=uint8_t(c)-32;return i<95?i:unsigned('?'-32);}
  int advance(unsigned i)const{return inkWidth[i]?inkWidth[i]+1:Space;}
};
}
