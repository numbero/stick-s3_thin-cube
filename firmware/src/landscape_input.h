#pragma once
#include <math.h>
#include <stdint.h>

// All games use a grip-relative, screen-relative tilt, independent of units.
struct LandscapeTilt {
  static constexpr float DeadDegrees=3, FullDegrees=15, SmoothMs=90;
  static constexpr float MazePitchDeadDegrees=1.5f,MazePitchFullDegrees=10;
  static float tiltDegrees(float x,float y,uint8_t rotation) {
    if(!isfinite(x)||!isfinite(y)||x*x+y*y<.01f)return NAN;
    return atan2f(rotation==1?-y:y,fabsf(x))*57.2957795f;
  }
  static float mazeHorizontalDegrees(float x,float y,uint8_t rotation){
    if(rotation!=1&&rotation!=3)return NAN;
    // The existing pose coordinate points uphill. A rolling ball must instead
    // accelerate toward the low edge; apply this before grip capture/rebase.
    return -tiltDegrees(x,y,rotation);
  }
  static float verticalDegrees(float x,float y,float z) {
    if(!isfinite(x)||!isfinite(y)||!isfinite(z)||x*x+y*y+z*z<.01f)return NAN;
    // Pitch uses the screen normal; an in-plane rotation does not reverse it.
    // Tilting toward face-up moves the probe upward (negative screen Y).
    return atan2f(-z,hypotf(x,y))*57.2957795f;
  }
  static float mazePitchDegrees(float x,float y,float z,uint8_t rotation){
    if((rotation!=1&&rotation!=3)||!isfinite(x)||!isfinite(y)||!isfinite(z)||x*x+y*y+z*z<.01f)return NAN;
    // Keep the signed in-plane hemisphere after tilting through face-up. An
    // unsigned hypot folds -95 degrees back to -85 and weakens/reverses input.
    const float plane=copysignf(hypotf(x,y),rotation==1?x:-x);
    return atan2f(-z,plane)*57.2957795f;
  }
  static float mapped(float degrees,float dead=DeadDegrees,float full=FullDegrees) {
    const float magnitude=fabsf(degrees);
    if(magnitude<=dead)return 0;
    const float value=fminf(1,(magnitude-dead)/(full-dead));
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
  Action update(uint32_t now,bool a,bool clicked,bool active,bool canEnter,bool consumed,bool actionWhileActive=false) {
    if(previousActive&&!active&&a)blocked=true;
    previousActive=active;
    if(a&&!previousA)since=now;
    previousA=a;
    if(consumed&&(a||clicked))blocked=true;
    if(!a&&!clicked)blocked=false;
    if(blocked)return Action::None;
    if((active||canEnter)&&a&&uint32_t(now-since)>=HoldMs){blocked=true;return active?Action::Exit:Action::None;}
    if((!active||actionWhileActive)&&canEnter&&clicked)return Action::Enter;
    return Action::None;
  }
};

// A press keeps its initial context through the final click. Rotation and
// wake-only presses cannot leak into the destination game's A/B operation.
struct LandscapePressOwner {
  struct Sample {bool landscape,valid;Sample(bool own=false,bool ok=false):landscape(own),valid(ok){}};
  bool previous=false,tracking=false,blocked=false;
  uint8_t owner=0;
  Sample update(bool down,bool clicked,uint8_t rotation,bool allowed){
    if((down&&!previous)||(clicked&&!tracking)){owner=rotation;tracking=true;blocked=!allowed;}
    if(tracking&&(owner!=rotation||!allowed))blocked=true;
    const Sample sample{tracking&&(owner==1||owner==3),tracking&&!blocked&&owner==rotation&&(owner==1||owner==3)};
    if(!down&&!clicked){tracking=false;blocked=false;}
    previous=down;
    return sample;
  }
};
