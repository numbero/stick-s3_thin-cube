#pragma once
#include <stdint.h>

namespace UIStatus {
constexpr int IconSize = 12, SlotPitch = 16, Width = IconSize + 2*SlotPitch;
enum class VoiceIcon { None, Wave, Muted, Warning };
struct VoiceInputs {
  bool muted, streaming, allowed, connected, audioReady, captureReady, calibrated, sleeping;
  bool captureDemand;
  VoiceInputs(bool mute,bool stream,bool allow,bool connect,bool audio,bool capture,bool calibration,bool sleep,bool demand=true)
    :muted(mute),streaming(stream),allowed(allow),connected(connect),audioReady(audio),captureReady(capture),
     calibrated(calibration),sleeping(sleep),captureDemand(demand){}
};
inline VoiceIcon voiceIcon(const VoiceInputs& s) {
  if(s.muted) return VoiceIcon::Muted;
  if(s.streaming) return VoiceIcon::Wave;
  if(s.allowed && s.connected && !s.sleeping && (!s.audioReady || (!s.captureReady&&s.captureDemand) || !s.calibrated)) return VoiceIcon::Warning;
  return VoiceIcon::None;
}
enum class Icon { WiFi, Bluetooth, Mac, Wave, Muted, Warning };

// Native LCD primitives: every icon stays inside its 12x12 logical-pixel slot.
template<class Canvas> void drawIcon(Canvas& g, Icon icon, int x, int y, uint16_t color, bool disconnected=false) {
  switch(icon) {
    case Icon::WiFi:
      // Two shallow waves and a dot form a centered 12x8 silhouette.
      // The third wave crowded this small slot into an almost square shape.
      g.drawFastHLine(x+3,y+2,6,color);
      g.drawLine(x,y+4,x+3,y+2,color);g.drawLine(x+8,y+2,x+11,y+4,color);
      g.drawFastHLine(x+4,y+5,4,color);
      g.drawLine(x+2,y+6,x+4,y+5,color);g.drawLine(x+7,y+5,x+9,y+6,color);
      g.fillRect(x+5,y+8,2,2,color);break;
    case Icon::Bluetooth:
      g.drawLine(x+5,y,x+5,y+11,color);
      g.drawLine(x+5,y,x+9,y+3,color);g.drawLine(x+9,y+3,x+2,y+9,color);
      g.drawLine(x+2,y+2,x+9,y+8,color);g.drawLine(x+9,y+8,x+5,y+11,color);break;
    case Icon::Mac:
      g.drawRect(x,y+1,12,8,color);g.drawFastVLine(x+5,y+9,2,color);
      g.drawFastHLine(x+3,y+11,6,color);break;
    case Icon::Wave:
      for(int i=0;i<5;i++) {
        const int h=i==2?12:(i==1||i==3?8:4);
        g.drawFastVLine(x+1+i*2,y+(12-h)/2,h,color);
      } break;
    case Icon::Muted:
      g.drawRoundRect(x+4,y,4,7,2,color);
      g.drawLine(x+2,y+5,x+2,y+7,color);g.drawLine(x+2,y+7,x+4,y+9,color);
      g.drawLine(x+4,y+9,x+7,y+9,color);g.drawLine(x+7,y+9,x+9,y+7,color);
      g.drawFastVLine(x+9,y+5,3,color);g.drawFastVLine(x+5,y+9,2,color);
      g.drawFastHLine(x+3,y+11,6,color);disconnected=true;break;
    case Icon::Warning:
      g.drawTriangle(x+5,y,x,y+11,x+11,y+11,color);
      g.drawFastVLine(x+5,y+4,3,color);g.drawPixel(x+5,y+9,color);break;
  }
  if(disconnected) {
    if(icon==Icon::WiFi)g.drawLine(x,y+2,x+11,y+9,color);
    else g.drawLine(x,y,x+11,y+11,color);
  }
}
}
