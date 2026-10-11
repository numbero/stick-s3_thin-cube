#pragma once
#include <M5Unified.h>
#include "clawd_story_policy.h"

// Draw only the existing 135x110 character area at y=60. No heap buffers.
void clawdStoryDraw(LGFX_Sprite& canvas,ClawdStory::Id id,uint32_t elapsedMs);
