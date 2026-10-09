#pragma once
#include <stdint.h>
#include <M5Unified.h>      /* LGFX_Sprite / fonts */

/* 小爪：块状像素生物
   几何取自 Claude Code 吉祥物（vibe-notch 的 ClaudeCrabIcon），单位空间 66×52：
   身体 54×39、两侧 6×13 触角、四条 6×13 腿（x=6/18/42/54）、两颗 6×6.5 方眼睛。
   屏幕放大量 1.35 → 89×70 逻辑像素。 */
enum PetState { PET_IDLE, PET_CURIOUS, PET_EXCITED, PET_UPSET, PET_CHARGING, PET_DIZZY, PET_SLEEP, PET_WAKE };

void petSetState(PetState st);
PetState petState();
void petPoke();                       /* 被逗一下：兴奋 2.6s */
void petBlinkNow();                   /* 立刻眨眼一次 */
void petSetScale(float s);            /* 缩放：单位空间 52 高 → 52*s 逻辑像素 */
float petHeight();                    /* 当前缩放下的整体高度（含腿） */
/* 必须画到 sprite 上：整屏推送时直接画屏的内容会被覆盖（会闪） */
void petDraw(LGFX_Sprite* g, int originY, uint32_t ms, int originX=-1, float attention=0);
