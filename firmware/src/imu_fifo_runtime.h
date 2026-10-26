#pragma once
#include "imu_fifo_capture.h"

// Two consumers share one hardware FIFO. Releasing one must not interrupt the
// other. A partial register transaction disarms both and retains restoration
// state, so a later acquisition cannot overwrite the original register image.
template<class Bus> class ImuFifoRuntime {
 public:
  bool software=false,recording=false,restored=true;
  bool setSoftware(Bus& bus,bool enabled){return update(bus,enabled,recording);}
  bool setRecording(Bus& bus,bool enabled){return update(bus,software,enabled);}
 private:
  ImuFifoCapture<Bus> capture;
  bool update(Bus& bus,bool nextSoftware,bool nextRecording){
    const bool owned=software||recording;
    if(owned&&(nextSoftware||nextRecording)){
      software=nextSoftware;recording=nextRecording;return true;
    }
    software=recording=false;
    if(!nextSoftware&&!nextRecording){restored=capture.restore(bus);return restored;}
    if(!restored&&!capture.restore(bus))return false;
    if(!capture.begin(bus)){restored=capture.restore(bus);return false;}
    restored=false;software=nextSoftware;recording=nextRecording;return true;
  }
};
