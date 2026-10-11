#include "clawd_stories.h"
#include "clawd_story_render.h"
#include "theme.h"

static_assert(ClawdStoryRaster::Palette::body==C::body&&ClawdStoryRaster::Palette::eye==C::eye&&
              ClawdStoryRaster::Palette::screen==C::screen,"Story palette must match the normal character");

void clawdStoryDraw(LGFX_Sprite& canvas,ClawdStory::Id id,uint32_t elapsedMs){
  if(!ClawdStory::valid(id))return;
  int32_t x,y,w,h;canvas.getClipRect(&x,&y,&w,&h);canvas.setClipRect(0,60,135,110);
  ClawdStoryRaster::Painter<LGFX_Sprite>(canvas,id,std::min(elapsedMs,ClawdStory::duration(id))*.001f).draw();
  canvas.setClipRect(x,y,w,h);
}
