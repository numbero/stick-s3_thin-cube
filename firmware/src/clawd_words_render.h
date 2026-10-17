#pragma once
#include "clawd_words_policy.h"

namespace ClawdWords {
// Native 16px glyphs and 18px line pitch. Keep all ink below the character
// and above the connection caption at y=209.
struct Layout {
  static constexpr int Left=4,Width=127,TextWidth=121,Top=158,Bottom=205,FirstY=169,SecondY=187;
  static int textX(int width){return (135-width)/2;}
};
template<class Canvas,class DrawText,class Measure>
void draw(Canvas& canvas,DrawText text,Measure measure,const Policy& words,uint32_t now,
          uint16_t ink,uint16_t outline,uint16_t screen){
  if(!words.visible())return;
  const auto& value=words.text();
  const float progress=words.entry(now);
  if(words.talking()){
    canvas.drawRect(Layout::Left,166,Layout::Width,39,outline);
    canvas.fillRect(33,166,7,1,screen);canvas.fillRect(33,160,1,7,outline);
    for(int i=0;i<7;i++)canvas.fillRect(33+i,160+i,1,1,outline);
  }
  // Reveal complete lines; no per-character typewriter or perpetual cursor.
  if(progress>0)text(value.line[0],Layout::textX(measure(value.line[0])),Layout::FirstY+(progress<.7f?1:0),ink);
  if(progress>=.26f)text(value.line[1],Layout::textX(measure(value.line[1])),Layout::SecondY+(progress<1?1:0),ink);
}
}
