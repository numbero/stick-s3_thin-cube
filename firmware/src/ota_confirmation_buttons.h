#pragma once
#include <stdint.h>

// Each concrete image needs a fresh click after both buttons were released.
// A release/click already in flight when CONFIRM begins cannot authorize it.
struct OtaConfirmationButtons {
  enum class Action { None, Confirm, Cancel };
  uint32_t generation=0;
  bool initialized=false,released=false;
  void reset(){initialized=false;released=false;}
  Action update(uint32_t session,bool stable,bool a,bool b,bool aClicked,bool bClicked){
    if(!initialized||generation!=session||!stable){
      generation=session;initialized=true;released=false;
    }
    // An explicit cancellation stays available before the release gate opens.
    if(bClicked){released=false;return Action::Cancel;}
    if(!stable)return Action::None;
    if(!released){released=!a&&!b;return Action::None;}
    return aClicked&&!b?Action::Confirm:Action::None;
  }
};
