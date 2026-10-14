#pragma once
#include "pose_detector.h"

// Shares the pose detector's confirmation window, rather than waiting for a
// second face-down motion event. Real handling must also settle before entry.
struct FaceDownRestPolicy {
  enum class Action { None, Sleep, Wake };
  bool active=false,observing=false,inhibited=false;
  uint32_t quietSince=0;

  void cancel(){active=false;observing=false;}
  // A button or explicit Mac start remains usable while the device is face
  // down. Re-arm only after lifting it, instead of immediately sleeping again.
  void overrideWake(){cancel();inhibited=true;}

  Action update(uint32_t now,DevicePose pose,DevicePose candidate,
                bool healthy,bool busy,bool motion,bool button){
    const bool lifted=pose!=DevicePose::FaceDown&&candidate!=DevicePose::FaceDown;
    if(lifted)inhibited=false;
    const bool wasActive=active;
    if(button){overrideWake();return wasActive?Action::Wake:Action::None;}
    if(!healthy||busy){cancel();return wasActive?Action::Wake:Action::None;}
    if(active){
      // Brief vibration or an unconfirmed pose exit must not flash the screen.
      // A confirmed slow tilt wakes even without a pickup acceleration event.
      if(lifted){cancel();return Action::Wake;}
      return Action::None;
    }
    if(inhibited||motion||candidate!=DevicePose::FaceDown){observing=false;return Action::None;}
    if(!observing){observing=true;quietSince=now;}
    if(pose==DevicePose::FaceDown&&uint32_t(now-quietSince)>=PoseDetector::FaceDownHoldMs){
      active=true;return Action::Sleep;
    }
    return Action::None;
  }
};
