#pragma once
#include <stdint.h>

/* 姿态输入：拿起唤醒 / 摇晃 / 屏幕朝下。
   home 仅用于诊断；机身 Y/Z 轴经上位机校准后由 PoseDetector 区分
   上下倒置（语音）与屏幕朝下（睡眠），不再用 home 的反向判断睡眠。 */
enum MotionEv { MEV_NONE = 0, MEV_PICKUP, MEV_SHAKE, MEV_FACE_DOWN };

void     motionBegin();
MotionEv motionPoll(uint32_t ms);   /* 主循环里每帧调 */
float    motionStillSec(uint32_t ms);  /* 已持续静止多少秒 */
bool     motionHomeKnown();
void     motionPrintHome();         /* 串口打一次 home 向量与最近一次原始三轴 */

/* 诊断快照：屏上诊断页与串口都用它 */
struct MotionDiag {
  bool     imuEnabled;
  uint32_t reads;      /* update() 被调用次数 */
  uint32_t updates;    /* 其中拿到新数据的次数 */
  float    ax, ay, az; /* 最近一次原始三轴 */
  float    mag;        /* 合加速度 */
  float    gRef;       /* 1g 参考（自动识别 g / m/s²） */
  float    dev;        /* |mag/gRef - 1|，越小越静止 */
  bool     still;
  bool     home;
  float    homeVec[3];
  uint32_t pickups, shakes, downs;
};
MotionDiag motionDiag();
