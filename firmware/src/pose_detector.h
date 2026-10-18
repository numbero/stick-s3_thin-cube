#pragma once
#include <math.h>
#include <stdint.h>

enum class DevicePose { Other, Upright, Inverted, FaceUp, FaceDown, Landscape };
inline const char* poseName(DevicePose pose) {
  switch(pose) {
    case DevicePose::Upright: return "upright";
    case DevicePose::Inverted: return "inverted";
    case DevicePose::FaceUp: return "face_up";
    case DevicePose::FaceDown: return "face_down";
    case DevicePose::Landscape: return "landscape";
    default: return "moving";
  }
}
struct PoseDetector {
  static constexpr uint32_t VerticalHoldMs = 220, FaceUpHoldMs = 160, FaceDownHoldMs = 1500;
  static constexpr uint32_t VoiceExitHoldMs = 450;
  static constexpr float VoiceExitAxis = .966f; // Within about 15 degrees of the target axis.
  int uprightSign = 0, faceUpSign = 0;
  DevicePose stable = DevicePose::Other, candidate = DevicePose::Other;
  uint32_t since = 0;
  bool calibrateUpright(float x, float y, float z) {
    return calibrateVertical(x,y,z,false);
  }
  bool calibrateInverted(float x, float y, float z) {
    return calibrateVertical(x,y,z,true);
  }
private:
  bool calibrateVertical(float x, float y, float z, bool inverted) {
    const float n = sqrtf(x*x+y*y+z*z);
    if (!(n >= .5f && fabsf(y)/n >= .85f && fabsf(z)/n <= .35f)) return false;
    // Persist the same upright reference as before, even when the USB-friendly
    // calibration pose has A above the display.
    uprightSign = (y > 0 ? 1 : -1) * (inverted ? -1 : 1);
    stable = candidate = inverted ? DevicePose::Inverted : DevicePose::Upright;
    return true;
  }
public:
  bool calibrateFaceUp(float x, float y, float z) {
    const float n = sqrtf(x*x+y*y+z*z);
    if (!(n >= .5f && fabsf(z)/n >= .9f)) return false;
    faceUpSign = z > 0 ? 1 : -1;
    stable = candidate = DevicePose::FaceUp;
    return true;
  }
  DevicePose update(float x, float y, float z, float gyro, uint32_t now, bool voiceExitGuarded=false) {
    const float n = sqrtf(x*x+y*y+z*z);
    DevicePose p = DevicePose::Other;
    if (n > .80f && n < 1.20f && gyro >= 0 && gyro < 100) {
      if (uprightSign) {
        const float vertical = y*uprightSign/n;
        const DevicePose next = vertical > 0 ? DevicePose::Upright : DevicePose::Inverted;
        const bool retained = next == stable;
        const float threshold = voiceExitGuarded && next == DevicePose::Upright ? VoiceExitAxis : (retained ? .72f : .82f);
        if(fabsf(vertical) > threshold &&
           fabsf(z)/n < (retained ? .60f : .50f) &&
           fabsf(x)/n < (retained ? .65f : .55f)) p = next;
      }
      if (faceUpSign && z*faceUpSign/n > (voiceExitGuarded ? VoiceExitAxis : (stable == DevicePose::FaceUp ? .85f : .9f))) p = DevicePose::FaceUp;
      if (faceUpSign && z*faceUpSign/n < -(stable == DevicePose::FaceDown ? .85f : .9f)) p = DevicePose::FaceDown;
      if(fabsf(x)/n > (voiceExitGuarded ? VoiceExitAxis : (stable==DevicePose::Landscape?.72f:.82f)) &&
         fabsf(y)/n < .55f && fabsf(z)/n < .5f) p=DevicePose::Landscape;
    }
    if (p != candidate) { candidate = p; since = now; }
    // Moving remains ambiguous and preserves voice in PoseVoiceController.
    // Only ordinary exits get a longer hold; inversion and face-down rest keep
    // their existing admission times.
    const bool ordinaryExit = p == DevicePose::Upright || p == DevicePose::FaceUp || p == DevicePose::Landscape;
    const uint32_t dwell = voiceExitGuarded && ordinaryExit ? VoiceExitHoldMs :
      p == DevicePose::FaceDown ? FaceDownHoldMs : (p == DevicePose::FaceUp || p==DevicePose::Landscape) ? FaceUpHoldMs : p == DevicePose::Other ? 100 : VerticalHoldMs;
    if (uint32_t(now-since) >= dwell) stable = p;
    return stable;
  }
};
