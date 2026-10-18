#include "../src/flick_detector.h"
#include "../src/pose_voice.h"
#include <assert.h>
#include <stdio.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Captured on 2026-10-18 with diag3 already installed, before any changes
// based on these trials. These are logged snapshots, not a FIFO recording.
static void replay(const char* name,unsigned starts,unsigned hits){
  const auto path=std::filesystem::path(__FILE__).parent_path()/"fixtures"/name;
  std::ifstream input(path);assert(input.good());std::string line;std::getline(input,line);
  FlickDetector d;unsigned count=0;
  PoseDetector pose;
  assert(pose.calibrateFaceUp(0,0,1));assert(pose.calibrateInverted(0,-1,0));
  VoiceState voice;voice.allowed=voice.peer=voice.captureReady=true;
  PoseVoiceController controller;
  controller.update(voice,DevicePose::FaceUp,false);
  controller.update(voice,DevicePose::Inverted,false);
  while(std::getline(input,line)){
    for(char& c:line)if(c==',')c=' ';
    std::istringstream row(line);uint32_t ms;float ax,ay,az,gx,gy,gz;
    assert(bool(row>>ms>>ax>>ay>>az>>gx>>gy>>gz));
    pose.update(ax,ay,az,sqrtf(gx*gx+gy*gy+gz*gz),ms,voice.poseExitGuarded());
    controller.update(voice,pose.stable,false);
    assert(voice.streaming&&voice.inverted); // Upright fast exit must not cut off these recorded flicks.
    d.update(ms,ax,ay,az,gx,gy,gz,voice.streaming&&voice.inverted,false);++count;
  }
  assert(count>100&&d.starts==starts&&d.hits==hits);
}
int main(){
  replay("flick_handheld_double.csv",5,5);
  replay("flick_handheld_single.csv",3,0);
  {
    // Calibration recording: the operator confirmed five natural pairs, then
    // two extra pairs. Retain its measured context, button and sensor flags.
    const auto path=std::filesystem::path(__FILE__).parent_path()/"fixtures"/"flick_handheld_natural.csv";
    std::ifstream input(path);assert(input.good());std::string line;std::getline(input,line);
    FlickDetector d;unsigned count=0;
    while(std::getline(input,line)){
      for(char& c:line)if(c==',')c=' ';
      std::istringstream row(line);uint32_t ms;float ax,ay,az,gx,gy,gz;unsigned flags;
      assert(bool(row>>ms>>ax>>ay>>az>>gx>>gy>>gz>>flags));
      const auto result=d.update(ms,ax,ay,az,gx,gy,gz,(flags&5)==5,flags&2);
      if(result.event==FlickDetector::Event::Recognized)assert(flags==5);
      ++count;
    }
    assert(count==3550&&d.starts==7&&d.hits==7&&d.rejected==0);
  }
  puts("recorded handheld snapshots: five double impulses / five events, three singles / no events passed");
  puts("natural handheld calibration: seven pairs / seven events with recorded context passed");
}
