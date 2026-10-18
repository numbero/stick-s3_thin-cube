#pragma once
#include <stdint.h>

// A fresh context never replays a button already physically held at entry.
struct WalkieKeys {
  bool active=false, heldA=false, heldB=false, armedA=false, armedB=false;
  void update(bool enabled,bool a,bool b,bool allowA=true) {
    if(!enabled){active=heldA=heldB=armedA=armedB=false;return;}
    if(!active){active=true;armedA=!a;armedB=!b;heldA=heldB=false;}
    if(!a){heldA=false;armedA=true;}
    else if(!allowA){heldA=armedA=false;}
    else if(armedA)heldA=true;
    if(!b){heldB=false;armedB=true;}else if(armedB)heldB=true;
  }
};

struct WalkieKeyEdge { uint32_t seq; bool down; char key; };
struct WalkieKeyQueue {
  WalkieKeyEdge entries[32]{};
  uint32_t sequence=0;
  unsigned read=0,count=0;
  void clear(){read=count=0;}
  const WalkieKeyEdge* front()const{return count?&entries[read]:nullptr;}
  bool push(bool down,char key){
    if(count==32)return false;
    entries[(read+count)%32]={++sequence,down,key};++count;return true;
  }
  void acknowledge(uint32_t seq){if(count&&entries[read].seq==seq){read=(read+1)%32;--count;}}
};
