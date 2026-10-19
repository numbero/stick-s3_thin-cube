#pragma once
#include "tap_parameters.h"
#include <string.h>

// A failed write can leave half of a two-page feature configuration applied.
// Restore and verify the entire old configuration; disarm on rollback failure.
template<class Configure> const char* tapApplyTransaction(TapParameters& current,bool& verified,
    const TapParameters& next,Configure configure){
  const auto previous=current;current=next;verified=configure(next.enabled);
  if(verified)return "ok";
  current=previous;verified=configure(previous.enabled);
  if(verified)return "write_failed_restored";
  configure(false);return "readback_failed";
}

// Save only a verified sensor configuration. If storage fails (including an
// ambiguous commit), restore and verify both storage and runtime state.
template<class Configure,class Persist> const char* tapSaveTransaction(TapParameters& current,bool& verified,bool& saved,
    const TapParameters& next,Configure configure,Persist persist){
  const auto previous=current;const bool wasSaved=saved;
  const char* result=tapApplyTransaction(current,verified,next,configure);
  if(strcmp(result,"ok")){saved=wasSaved;return result;}
  if(persist(next)){saved=true;return "ok";}
  current=previous;saved=persist(previous);verified=configure(previous.enabled);
  if(!verified){configure(false);return "readback_failed";}
  return saved?"storage_failed_restored":"storage_rollback_failed";
}
