#pragma once

// The completion page needs a new short click. A held button or the release
// click from the self-test page must never dismiss it as it first appears.
struct OtaBootNoticeButtons {
  bool released=false,consumed=false;
  void reset(){released=false;consumed=false;}
  bool update(bool complete,bool a,bool b,bool aClicked,bool bClicked){
    if(!complete){reset();return false;}
    if(consumed)return false;
    if(!released){
      released=!a&&!b&&!aClicked&&!bClicked;
      return false;
    }
    if(aClicked||bClicked){consumed=true;return true;}
    return false;
  }
};
