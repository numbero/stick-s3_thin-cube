#pragma once
#include <M5Unified.h>

/* 屏幕配色，与 demo/index.html 的调色板一一对应（RGB565） */
static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b){
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

namespace C {
/* UI：琥珀终端 */
constexpr uint16_t ink    = rgb565(248, 176, 0);
constexpr uint16_t glow   = rgb565(255, 201, 77);
constexpr uint16_t dim    = rgb565(176, 124, 0);
constexpr uint16_t dim2   = rgb565(107, 76, 0);
constexpr uint16_t ok     = rgb565(61, 220, 151);
constexpr uint16_t warn   = rgb565(255, 204, 0);
constexpr uint16_t err    = rgb565(255, 51, 68);
constexpr uint16_t offline= rgb565(153, 149, 139);
constexpr uint16_t panel  = rgb565(40, 32, 16);
constexpr uint16_t screen = rgb565(13, 13, 13);

/* 小爪：自己的 terracotta 色族，不掺琥珀 */
constexpr uint16_t body    = rgb565(217, 119, 87);
constexpr uint16_t bodyDim = rgb565(138, 74, 52);
constexpr uint16_t deep    = rgb565(90, 47, 34);
constexpr uint16_t lite    = rgb565(232, 161, 132);
constexpr uint16_t eye     = rgb565(20, 14, 10);
constexpr uint16_t brow    = rgb565(140, 58, 42);
}
