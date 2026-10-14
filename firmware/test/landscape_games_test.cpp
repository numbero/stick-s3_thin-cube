#include "../src/landscape_games.h"
#include "../src/display_motion.h"
#include "../src/pose_voice.h"
#include <assert.h>
#include <stdio.h>

int main(){
  using Scene=LandscapeGames::Scene;
  {
    LandscapeGames g;
    assert(g.scene(0)==Scene::None&&g.scene(1)==Scene::Explore&&g.scene(2)==Scene::None&&g.scene(3)==Scene::Catch);
    assert(!g.begin(0,0,0,100)&&!g.begin(2,0,0,100)&&!g.begin(3,NAN,0,100));
    assert(g.begin(1,0,0,100)&&g.playing==Scene::Explore&&g.active());
    assert(!g.begin(3,0,0,200)&&!g.catchStars.active);
    g.update(1,-12,-12,true,190);assert(g.update(1,-12,-12,true,440).explore==Exploration::Event::Locked);
    assert(g.update(1,0,-12,true,1540).explore==Exploration::Event::Found&&!g.active());
    assert(g.begin(3,4,0,1600)&&g.playing==Scene::Catch&&g.active());
    assert(g.exploration.discoveries==1&&!g.exploration.active);
  }
  {
    // The same real active flag gates display orientation for the new game.
    LandscapeGames g;DisplayOrientation d;
    d.update(-1,0,0,0,1,100);d.update(-1,0,0,0,1,190);assert(d.rotation==3);
    g.begin(d.rotation,0,0,200);
    d.update(0,1,0,0,1,300,g.active());d.update(0,1,0,0,1,20000,g.active());assert(d.rotation==3&&!d.pending);
    assert(g.update(3,0,-12,true,20200).catchStars.finished&&!g.active());
    d.update(0,1,0,0,1,20200,g.active());d.update(0,1,0,0,1,20289,g.active());assert(d.rotation==3);
    d.update(0,1,0,0,1,20290,g.active());assert(d.rotation==0&&d.confirmedAfterMs==90);
  }
  {
    LandscapeGames g;LandscapeButton b;using A=LandscapeButton::Action;
    g.begin(3,0,0,100);b.update(200,true,false,g.active(),true,false);
    assert(b.update(1399,true,false,g.active(),true,false)==A::None);
    assert(b.update(1400,true,false,g.active(),true,false)==A::Exit);g.cancel();
    assert(!g.active()&&!g.catchStars.finished);
    assert(b.update(1500,false,true,g.active(),true,false)==A::None&&b.blocked);
    b.update(1510,false,false,g.active(),true,false);
    assert(b.update(1600,false,true,g.active(),true,false)==A::Enter);
    assert(g.begin(3,0,0,1600));
  }
  {
    LandscapeGames g;g.begin(3,0,0,100);g.update(3,0,-12,true,1800);
    g.update(1,0,-12,true,1900);assert(!g.active()&&g.playing==Scene::None); // Forced scene changes clean both games.
    assert(g.begin(1,0,0,2000));g.update(1,-12,-12,true,2090);g.update(1,0,-12,false,2200);
    assert(!g.active()&&!g.exploration.scanning&&!g.catchStars.active);
  }
  {
    const uint8_t rotations[]={1,3};
    for(uint8_t rotation:rotations){
      LandscapeGames g;g.begin(rotation,0,0,100);
      VoiceState v;v.allowed=v.captureReady=true;v.setPeer(true,100);
      PoseVoiceController c;c.update(v,DevicePose::Landscape,false);
      c.update(v,DevicePose::Inverted,true,g.active());assert(!v.streaming&&!v.held&&c.suppressed);
      g.cancel();c.update(v,DevicePose::Inverted,false,g.active());assert(!v.streaming&&!c.suppressed);
      c.update(v,DevicePose::Inverted,false);assert(!v.streaming);
      c.update(v,DevicePose::Landscape,false);c.update(v,DevicePose::Inverted,false);assert(v.streaming);
    }
  }
  {
    LandscapeGames g;g.begin(3,0,0,100);VoiceState v;v.allowed=v.captureReady=true;v.setPeer(true,100);
    PoseVoiceController c;c.update(v,DevicePose::FaceUp,false);assert(v.startManual(false));
    c.update(v,DevicePose::Inverted,true,g.active());assert(v.streaming&&v.manual&&!v.held);
    LandscapeButton b;b.update(200,true,false,g.active(),true,false);
    g.update(3,0,-12,true,20100);assert(!g.active());
    assert(b.update(20101,true,false,g.active(),true,false)==LandscapeButton::Action::None&&b.blocked);
    c.update(v,DevicePose::Landscape,false,g.active());assert(v.streaming&&v.manual&&!v.held);
  }
  {
    // Exploration receives pitch; the horizontal catch game ignores it.
    LandscapeGames g;assert(!g.begin(1,0,NAN,100));
    assert(g.begin(1,0,4,100));
    g.update(1,-12,4,true,190);assert(!g.exploration.near);
    g.update(1,-12,-8,true,280);assert(g.exploration.near);
    assert(g.update(1,-12,-8,true,530).explore==Exploration::Event::Locked);
    assert(g.update(1,90,90,true,1630).explore==Exploration::Event::Found);
    assert(g.begin(3,0,NAN,1700));
    g.update(3,0,90,true,1790);assert(g.catchStars.offset==0);
    g.update(3,15,-90,true,1880);assert(g.catchStars.offset==1);
    g.update(3,-15,NAN,true,1970);assert(g.catchStars.offset==-1&&g.catchStars.active);
  }
  {
    // Both-axis aiming and scanning retain the display lock until discovery.
    LandscapeGames g;DisplayOrientation d;
    d.update(1,0,0,0,1,100);d.update(1,0,0,0,1,190);assert(d.rotation==1);
    assert(g.begin(1,0,0,200));g.update(1,-12,-12,true,290);
    d.update(0,1,0,0,1,300,g.active());d.update(0,1,0,0,1,540,g.active());
    assert(d.rotation==1&&!d.pending);
    assert(g.update(1,-12,-12,true,540).explore==Exploration::Event::Locked);
    d.update(0,1,0,0,1,1640,g.active());assert(d.rotation==1);
    assert(g.update(1,0,0,true,1640).explore==Exploration::Event::Found);
    d.update(0,1,0,0,1,1640,g.active());d.update(0,1,0,0,1,1729,g.active());assert(d.rotation==1);
    d.update(0,1,0,0,1,1730,g.active());assert(d.rotation==0);
  }
  puts("8 landscape 2D routing, scene, orientation, button and voice integration scenarios passed");
}
