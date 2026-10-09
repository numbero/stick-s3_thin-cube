#pragma once
#include <stdint.h>

void uiBegin();
void uiCalibrate();                /* 扫像素标定真实字宽/字高（uiBegin 内已调用） */
void uiTick(uint32_t ms);          /* 按键 / 电池 / 状态推进 */
void uiDraw(uint32_t ms);          /* 整屏重绘（内部用 sprite，不闪） */
void uiSetOrientation(uint8_t rotation,uint32_t ms);
bool uiRotationActive();
void uiReportTiming();
void uiLog(const char* fmt, ...);  /* 往日志区推一行 */
void uiDumpMetrics();              /* 把实测字体/屏幕度量打到串口 */
void uiPoke();                     /* BtnA：逗小爪 */
void uiWakeForVoice();             /* Explicit Mac start wakes the companion. */
bool uiHealthy();
void uiUsbDebug();                 /* USB screenshot: stable idle UI, excludes pairing PIN/live/OTA */

bool uiButtonAForVoice(); // physical combo gestures consume A before voice injection
bool uiLandscapeActive(); // Either A-started game owns orientation and gestures.
