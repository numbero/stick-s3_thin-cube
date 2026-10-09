#include "../src/control_lease.h"
#include "../src/ui_status.h"
#include "../src/voice_state.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

struct BoundsCanvas {
  int count=0;
  void point(int x,int y){assert(x>=0&&x<12&&y>=0&&y<12);count++;}
  void drawPixel(int x,int y,uint16_t){point(x,y);}
  void drawLine(int x,int y,int x2,int y2,uint16_t){point(x,y);point(x2,y2);}
  void drawRect(int x,int y,int w,int h,uint16_t){point(x,y);point(x+w-1,y+h-1);}
  void fillRect(int x,int y,int w,int h,uint16_t c){drawRect(x,y,w,h,c);}
  void drawRoundRect(int x,int y,int w,int h,int,uint16_t c){drawRect(x,y,w,h,c);}
  void drawFastVLine(int x,int y,int h,uint16_t c){drawRect(x,y,1,h,c);}
  void drawFastHLine(int x,int y,int w,uint16_t c){drawRect(x,y,w,1,c);}
  void drawTriangle(int x,int y,int x2,int y2,int x3,int y3,uint16_t){point(x,y);point(x2,y2);point(x3,y3);}
};
int main(){
  ControlLease control;
  VoiceState voice;
  assert(!control.connected(0));
  // A ready=false heartbeat must still indicate an authenticated Mac control connection.
  control.heartbeat(10);voice.setPeer(false,10);
  assert(control.connected(10)&&!voice.peer);
  assert(control.connected(1010));assert(!control.connected(1011));
  control.reset();assert(!control.connected(10));
  control.heartbeat(0xfffffff0);assert(control.connected(984));assert(!control.connected(985));
  control.heartbeat(0);assert(control.connected(0));

  using namespace UIStatus;
  VoiceInputs s{false,false,true,true,true,true,true,false};
  assert(voiceIcon(s)==VoiceIcon::None);
  s.audioReady=false;assert(voiceIcon(s)==VoiceIcon::Warning);
  s.connected=false;assert(voiceIcon(s)==VoiceIcon::None);
  s.connected=true;s.allowed=false;assert(voiceIcon(s)==VoiceIcon::None);
  s.allowed=true;s.sleeping=true;assert(voiceIcon(s)==VoiceIcon::None);
  s.sleeping=false;s.audioReady=true;s.captureReady=false;assert(voiceIcon(s)==VoiceIcon::Warning);
  s.captureReady=true;s.calibrated=false;assert(voiceIcon(s)==VoiceIcon::Warning);
  s.streaming=true;assert(voiceIcon(s)==VoiceIcon::Wave);
  s.muted=true;assert(voiceIcon(s)==VoiceIcon::Muted);
  for(auto icon:{Icon::WiFi,Icon::Bluetooth,Icon::Mac,Icon::Wave,Icon::Muted,Icon::Warning}){
    for(bool slash:{false,true}){BoundsCanvas canvas;drawIcon(canvas,icon,0,0,1,slash);assert(canvas.count>0);}
  }
  // Fixed slots leave both the portrait battery and landscape title/battery clear.
  assert(135-4-Width>4+46);
  assert(240-8-46-6-Width>8+13*6);
  assert(IconSize<SlotPitch);
  puts("UI status: independent control lease, voice priority and 12px icon bounds passed");
}
