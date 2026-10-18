#pragma once
#include <stdint.h>
void flickTick(uint32_t now);
void flickReport();
bool flickCommand(const char* command);
struct FlickSnapshot {bool enabled;uint32_t starts,hits,rejected;const char* stage;};
FlickSnapshot flickSnapshot();
