#pragma once
#include <math.h>
#include <stdint.h>

// Both games use a grip-relative, screen-relative tilt, independent of units.
struct LandscapeTilt {
  static constexpr float DeadDegrees=3, FullDegrees=15, SmoothMs=90;
  static float tiltDegrees(float x,float y,uint8_t rotation) {
    if(!isfinite(x)||!isfinite(y)||x*x+y*y<.01f)return NAN;
    return atan2f(rotation==1?-y:y,fabsf(x))*57.2957795f;
  }
  static float verticalDegrees(float x,float y,float z) {
    if(!isfinite(x)||!isfinite(y)||!isfinite(z)||x*x+y*y+z*z<.01f)return NAN;
    // Pitch uses the screen normal; an in-plane rotation does not reverse it.
    // Tilting toward face-up moves the probe upward (negative screen Y).
    return atan2f(-z,hypotf(x,y))*57.2957795f;
  }
  static float mapped(float degrees) {
    const float magnitude=fabsf(degrees);
    if(magnitude<=DeadDegrees)return 0;
    const float value=fminf(1,(magnitude-DeadDegrees)/(FullDegrees-DeadDegrees));
    return degrees<0?-value:value;
  }
  static float delta(float degrees,float baseline) {
    return fmodf(degrees-baseline+540,360)-180;
  }
};

// Consume the whole exit press and release, including a held press at success.
struct LandscapeButton {
  enum class Action { None, Enter, Exit };
  static constexpr uint32_t HoldMs=1200;
  bool previousA=false,previousActive=false,blocked=false;
  uint32_t since=0;
  Action update(uint32_t now,bool a,bool clicked,bool active,bool canEnter,bool consumed) {
    if(previousActive&&!active&&a)blocked=true;
    previousActive=active;
    if(a&&!previousA)since=now;
    previousA=a;
    if(consumed&&(a||clicked))blocked=true;
    if(!a&&!clicked)blocked=false;
    if(blocked)return Action::None;
    if(active&&a&&uint32_t(now-since)>=HoldMs){blocked=true;return Action::Exit;}
    if(!active&&canEnter&&clicked)return Action::Enter;
    return Action::None;
  }
};
