#pragma once
#include "clawd_light_policy.h"

namespace ClawdLight {
template<class Canvas>void star(Canvas& canvas,int x,int y,uint16_t ink,uint16_t glow,bool small=false){
  const int arm=small?1:3;
  canvas.fillRect(x-arm,y,arm*2+1,1,ink);canvas.fillRect(x,y-arm,1,arm*2+1,ink);
  if(!small)canvas.fillRect(x-1,y-1,3,3,ink);
  canvas.fillRect(x,y,1,1,glow);
}
template<class Canvas,class Text>void draw(Canvas& canvas,Text text,const Policy& light,uint32_t now,
                                         uint16_t ink,uint16_t glow,uint16_t dim){
  if(!light.drawingLight())return;
  if(light.playing()){
    text(Hint,169,ink);
    canvas.fillRect(14,197,108,1,dim);canvas.fillRect(59,197,17,1,ink);
    const int held=int(light.progress()*17);
    if(held)canvas.fillRect(59,201,held,1,glow);
    star(canvas,lroundf(light.x()),190,ink,glow);
  }else{
    const float t=light.fraction(now),u=1-t;
    const int x=lroundf(u*u*light.x()+2*u*t*55+t*t*91);
    const int y=lroundf(u*u*190+2*u*t*154+t*t*133);
    star(canvas,x,y,ink,glow,t>.75f);
  }
}
}
