#include "motion.h"
#include "power_save.h"
#include "power_save_policy.h"
#include <M5Unified.h>
#include <math.h>
#include "device.h"
#include "motion_activity.h"

namespace {
/* 阈值（待真机标定，先给保守值） */
constexpr float STILL_DEV   = 0.10f;   /* 偏离 1g 小于此值算静止 */
constexpr float HOME_LATCH  = 1.20f;   /* 持续静止这么久才认「家」的朝向（秒） */
constexpr float DOWN_DOT    = -0.85f;  /* 朝向与 home 的点积小于此值 = 反扣 */
constexpr float DOWN_HOLD   = 1.50f;   /* 反扣保持这么久才判睡（秒） */
constexpr uint32_t SHAKE_WIN_MS = 800;
constexpr int      SHAKE_FLIPS  = 4;
constexpr uint32_t PICKUP_GAP_MS = 2000;

float    s_home[3] = {0, 0, -1};
bool     s_homeSet = false;
uint32_t s_stillSince = 0;
uint32_t s_moveSeenAt = 0;
uint32_t s_downSince  = 0;
uint32_t s_flipWin    = 0;
int      s_flips      = 0;
float    s_lastSign   = 0;
bool     s_wasStillLong = false;
uint32_t s_lastPickupAt = 0;
float    s_lastAcc[3] = {0, 0, 0};
float    s_gRef = 0;          /* 1g 参考：由首个样本自动识别 g / m/s² */
float    s_dev  = 0;
bool     s_still = false;
uint32_t s_reads = 0, s_updates = 0;
uint32_t s_pickups = 0, s_shakes = 0, s_downs = 0;
MotionActivity s_activity;
bool s_activityDetected=false;
}  // namespace

void motionBegin(){
  const uint32_t ms = millis();
  s_stillSince = ms;
  s_moveSeenAt = ms;
}

bool motionHomeKnown(){ return s_homeSet; }

void motionPrintHome(){
  Serial.printf("[motion] home %.2f %.2f %.2f | last acc %.2f %.2f %.2f\n",
                s_home[0], s_home[1], s_home[2],
                s_lastAcc[0], s_lastAcc[1], s_lastAcc[2]);
}

MotionEv motionPoll(uint32_t ms){
  s_activityDetected=false;
  static uint32_t lastPoll=0;
  if(powerSaveActive()&&uint32_t(ms-lastPoll)<IdleWorkProfile::IdleMotionMs)return MEV_NONE;
  lastPoll=ms;
  s_reads++;
  if (!M5.Imu.update()) return MEV_NONE;
  s_updates++;
  const auto d = M5.Imu.getImuData();
  const float x = d.accel.x, y = d.accel.y, z = d.accel.z;
  const float gyro = sqrtf(d.gyro.x*d.gyro.x+d.gyro.y*d.gyro.y+d.gyro.z*d.gyro.z);
  s_activityDetected=s_activity.update(x,y,z,gyro,ms);
  deviceMotion(x,y,z,gyro,ms);
  s_lastAcc[0] = x; s_lastAcc[1] = y; s_lastAcc[2] = z;

  const float mag = sqrtf(x * x + y * y + z * z);
  /* 单位自识别：M5Unified 文档写的是 g，但不同版本/驱动可能给 m/s²。
     首个样本看数量级即可定下参考，之后按 mag/gRef 判静止。 */
  if (s_gRef == 0 && mag > 0.05f) s_gRef = (mag > 5.0f) ? 9.81f : 1.0f;
  const float gRef = (s_gRef == 0) ? 1.0f : s_gRef;
  const float n   = (mag > 0.01f) ? mag : gRef;
  const float dev = fabsf(mag / gRef - 1.0f);
  const bool  still = (dev < STILL_DEV);
  s_dev = dev; s_still = still;

  if (still){
    if (s_stillSince == 0) s_stillSince = ms;
  } else {
    s_stillSince = 0;
    s_moveSeenAt = ms;
  }

  /* 「家」的朝向：首次持续静止时锁定（通常就是平放在桌上那次） */
  if (!s_homeSet && still && (ms - s_stillSince) > HOME_LATCH * 1000){
    s_home[0] = x / n; s_home[1] = y / n; s_home[2] = z / n;
    s_homeSet = true;
    Serial.printf("[motion] home latched: %.2f %.2f %.2f\n", s_home[0], s_home[1], s_home[2]);
  }

  /* 摇晃：横向主导轴的符号在窗口内反复翻转 */
  if (!still){
    if (ms - s_flipWin > SHAKE_WIN_MS){ s_flipWin = ms; s_flips = 0; }
    const float dom = (fabsf(x) > fabsf(y)) ? x : y;
    const float sgn = (dom >= 0) ? 1.0f : -1.0f;
    if (s_lastSign == 0) s_lastSign = sgn;
    else if (sgn != s_lastSign){ s_flips++; s_lastSign = sgn; }
    if (s_flips >= SHAKE_FLIPS && ms - s_lastPickupAt > 1500){
      s_flips = 0; s_lastPickupAt = ms; s_wasStillLong = false;
      s_shakes++;
      return MEV_SHAKE;
    }
  }

  /* 拿起：静止够久之后重新出现运动 */
  if (still && (ms - s_moveSeenAt) > 1500) s_wasStillLong = true;
  if (!still && s_wasStillLong && ms - s_lastPickupAt > PICKUP_GAP_MS){
    s_wasStillLong = false;
    s_lastPickupAt = ms;
    s_pickups++;
    return MEV_PICKUP;
  }

  /* 屏幕朝下与屏幕平面内上下倒置分别判定，home 仅保留诊断用途。 */
  if (still){
    if (devicePose() == DevicePose::FaceDown){
      if (s_downSince == 0) s_downSince = ms;
      if (ms - s_downSince > DOWN_HOLD * 1000){
        s_downSince = ms;
        s_downs++;
        return MEV_FACE_DOWN;
      }
    } else s_downSince = 0;
  } else s_downSince = 0;

  return MEV_NONE;
}

float motionStillSec(uint32_t ms){
  return (ms - s_moveSeenAt) / 1000.0f;
}
bool motionActivityDetected(){return s_activityDetected;}
bool motionActivityHealthy(uint32_t ms){return s_activity.healthy(ms);}
MotionDiag motionDiag(){
  MotionDiag g;
  g.imuEnabled = M5.Imu.isEnabled();
  g.reads = s_reads; g.updates = s_updates;
  g.ax = s_lastAcc[0]; g.ay = s_lastAcc[1]; g.az = s_lastAcc[2];
  g.mag = sqrtf(s_lastAcc[0]*s_lastAcc[0] + s_lastAcc[1]*s_lastAcc[1] + s_lastAcc[2]*s_lastAcc[2]);
  g.gRef = s_gRef; g.dev = s_dev; g.still = s_still;
  g.home = s_homeSet;
  g.homeVec[0] = s_home[0]; g.homeVec[1] = s_home[1]; g.homeVec[2] = s_home[2];
  g.pickups = s_pickups; g.shakes = s_shakes; g.downs = s_downs;
  return g;
}
