#include "../src/landscape_games.h"
#include "../src/display_motion.h"
#include "../src/pose_voice.h"
#include "../src/button_gestures.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
using Scene=LandscapeGames::Scene;
static void select(LandscapeGames& g,uint8_t rotation,Scene s){g.observe(rotation);for(unsigned i=0;i<4&&g.selected!=s;i++)g.next(rotation);assert(g.selected==s);}

int main(){
  {
    LandscapeGames g;assert(g.scene(0)==Scene::None&&g.scene(2)==Scene::None);
    assert(g.scene(3)==Scene::Explore&&g.scene(1)==Scene::Launch);
    g.observe(3);for(Scene expected:{Scene::Catch,Scene::Maze,Scene::Launch,Scene::Explore}){g.next(3);assert(g.selected==expected);}
    g.observe(1);for(Scene expected:{Scene::Maze,Scene::Catch,Scene::Explore,Scene::Launch}){g.next(1);assert(g.selected==expected);}
    assert(g.left==Scene::Explore&&g.right==Scene::Launch&&!g.preferencesDirty);
    select(g,3,Scene::Catch);assert(g.begin(3,0,NAN,100));assert(g.left==Scene::Catch&&g.right==Scene::Launch&&g.preferencesDirty);
    g.next(3);assert(g.selected==Scene::Maze&&g.paused==Scene::Catch&&!g.active());
    g.observe(1);assert(g.selected==Scene::Launch);select(g,1,Scene::Maze);assert(g.begin(1,0,0,200));
    assert(g.left==Scene::Catch&&g.right==Scene::Maze&&g.paused==Scene::None);
    g.cancel();g.observe(3);assert(g.selected==Scene::Catch);g.next(3);assert(g.selected==Scene::Maze);
    g.observe(1);assert(g.selected==Scene::Maze);g.next(1);assert(g.selected==Scene::Catch);
  }
  {
    LandscapeGames g;select(g,3,Scene::Catch);assert(g.begin(3,4,NAN,100));g.update(3,10,0,true,1400);
    const uint32_t clock=g.clock;const float y=g.catchStars.stars[0].y,x=g.catchStars.trayX();
    g.next(3);g.update(3,90,90,true,1000000);assert(g.clock==clock&&g.catchStars.stars[0].y==y);
    assert(!g.begin(3,0,NAN,1000010));assert(g.paused==Scene::Catch); // bad Maze input retains the old game
    select(g,3,Scene::Catch);assert(g.resumable());assert(g.begin(3,-13,NAN,1000020));
    g.update(3,-13,0,true,1000319);assert(g.clock==clock&&g.catchStars.trayX()==x);
    g.update(3,-13,0,true,1000320);assert(g.catchStars.stars[0].y==y);
    g.update(3,-13,0,true,1000420);assert(g.clock==clock+100&&g.catchStars.stars[0].y>y);
    assert(fabsf(g.catchStars.trayX()-x)<.001f);
    assert(g.update(3,-13,0,true,1020320).catchStars.finished&&!g.active());
    assert(g.phase==LandscapeGames::Phase::Result&&g.catchStars.leftMs(g.clock)==0);
  }
  {
    LandscapeGames g;assert(!g.begin(0,0,0,100));assert(!g.begin(3,0,NAN,100));
    assert(g.begin(3,0,0,100));g.update(3,-12,-12,true,490);
    assert(g.update(3,-12,-12,true,740).explore==Exploration::Event::Locked);
    g.next(3);g.observe(1);g.observe(0);assert(g.paused==Scene::Explore&&!g.active());
    g.observe(3);assert(g.selected==Scene::Explore&&g.resumable());
    assert(g.begin(3,20,20,40000));g.update(3,20,20,true,40300);
    assert(g.exploration.scanning);assert(g.update(3,20,20,true,41400).explore==Exploration::Event::Found);
    assert(g.result()&&!g.active()&&g.exploration.discoveries==1);
  }
  {
    LandscapeGames g;select(g,3,Scene::Maze);assert(g.begin(3,0,0,100));g.update(3,12,0,true,490);
    const float x=g.maze.x,vx=g.maze.vx;const auto clock=g.clock;
    g.next(3);g.update(3,-50,-50,true,10000);assert(g.maze.x==x&&g.maze.vx==vx&&g.clock==clock);
    select(g,1,Scene::Maze);assert(g.begin(1,32,-10,11000));g.update(1,32,-10,true,11300);
    assert(g.maze.x==x&&g.maze.vx==vx);g.update(1,32,-10,true,11320);assert(g.maze.x>x);
    g.maze.x=g.maze.goalX;g.maze.y=g.maze.goalY;g.maze.vx=g.maze.vy=0;
    assert(g.update(1,32,-10,true,11330).mazeFinished&&g.result());
    assert(g.begin(1,0,0,12000)&&g.maze.level==1);
  }
  {
    LandscapeGames g;assert(g.begin(1,0,0,100));g.update(1,0,0,true,400);
    g.launch.aim(-24,.75f);g.latchAim();g.update(1,15,15,true,490);
    assert(g.launch.angle==-24&&g.launch.power==.75f);assert(g.fire(500));assert(!g.fire(501));
    g.update(1,15,15,true,520);const float x=g.launch.x,y=g.launch.y;const auto clock=g.clock;
    g.next(1);assert(g.selected==Scene::Maze&&g.paused==Scene::Launch);g.update(1,0,0,true,9000);
    assert(g.launch.x==x&&g.launch.y==y&&g.clock==clock);
    select(g,1,Scene::Launch);assert(g.begin(1,0,0,10000));g.update(1,0,0,true,10300);
    assert(g.launch.x==x&&g.launch.y==y);
    for(uint32_t now=10310;g.active()&&now<17000;now+=10)g.update(1,0,0,true,now);
    assert(g.launch.won&&g.launch.shots==1&&g.result());
    assert(g.begin(1,0,0,18000)&&g.launch.level==1);
  }
  {
    LandscapeGames g;uint8_t record[]={0x47,1,uint8_t(Scene::Maze),uint8_t(Scene::Catch)};
    g.loadPreferences(record,4);g.observe(3);assert(g.selected==Scene::Maze);g.observe(1);assert(g.selected==Scene::Catch);
    record[2]=99;g.loadPreferences(record,4);assert(g.left==Scene::Explore&&g.right==Scene::Catch);
    record[1]=2;g.loadPreferences(record,4);assert(g.left==Scene::Explore&&g.right==Scene::Launch);
    g.loadPreferences(nullptr,0);assert(!g.preferencesDirty);
    select(g,3,Scene::Maze);assert(g.begin(3,0,0,100));g.preferences(record);
    LandscapeGames restored;restored.loadPreferences(record,4);assert(restored.left==Scene::Maze&&restored.right==Scene::Launch);
  }
  {
    LandscapeGames g;DisplayOrientation d;
    d.update(-1,0,0,0,1,100);d.update(-1,0,0,0,1,190);assert(d.rotation==3);
    assert(g.begin(3,0,0,200));d.update(0,1,0,0,1,300,g.active());d.update(0,1,0,0,1,20000,g.active());assert(d.rotation==3&&!d.pending);
    g.next(3);d.update(0,1,0,0,1,20000,g.active());d.update(0,1,0,0,1,20090,g.active());assert(d.rotation==0);
    VoiceState v;v.allowed=v.captureReady=true;v.setPeer(true,100);PoseVoiceController c;
    select(g,1,Scene::Launch);assert(g.begin(1,0,0,20100));
    c.update(v,DevicePose::Landscape,false);c.update(v,DevicePose::Inverted,true,g.active());assert(!v.streaming&&c.suppressed);
    g.cancel();c.update(v,DevicePose::Inverted,false,g.active());assert(!v.streaming&&!c.suppressed);
  }
  {
    LandscapeButton button;using A=LandscapeButton::Action;
    button.update(100,true,false,false,true,false);assert(button.update(1300,true,false,false,true,false)==A::None&&button.blocked);
    assert(button.update(1310,false,true,false,true,false)==A::None);button.update(1320,false,false,false,true,false);
    assert(button.update(1400,false,true,false,true,false)==A::Enter);
    button.update(1500,true,false,true,true,false,true);assert(button.update(1550,false,true,true,true,false,true)==A::Enter);
    button.update(1600,false,false,true,true,false,true);button.update(1700,true,false,true,true,false,true);
    assert(button.update(2900,true,false,true,true,false,true)==A::Exit);
    assert(button.update(3000,false,true,false,true,false,true)==A::None);
  }
  {
    LandscapePressOwner owner;
    assert(owner.update(true,false,3,true).valid);auto changed=owner.update(true,false,1,true);assert(changed.landscape&&!changed.valid);
    assert(!owner.update(false,true,1,true).valid);owner.update(false,false,1,true);
    assert(owner.update(true,false,1,true).valid);assert(owner.update(false,true,1,true).valid);owner.update(false,false,1,true);
    assert(!owner.update(true,false,3,false).valid);assert(!owner.update(false,true,3,true).valid); // wake only
    owner.update(false,false,3,true);owner.update(true,false,0,true);assert(!owner.update(false,true,3,true).valid);
    owner.update(false,false,3,true);owner.update(true,false,3,true);assert(owner.update(false,true,0,true).landscape); // no portrait mute
  }
  {
    // Gesture engine alone decides short B vs connection/pair; both cancel A.
    ButtonGestures b;using A=ButtonGestures::Action;
    b.update(100,false,true,false,true,true);assert(b.update(180,false,false,true,true,true)==A::Mute);
    b.update(200,false,true,false,true,true);assert(b.update(3200,false,true,false,true,true)==A::None&&b.switchReady);
    assert(b.update(3300,false,false,true,true,true)==A::Switch);
    b.update(3400,true,true,false,true,true);b.update(6300,true,false,false,true,true);
    assert(b.update(6500,false,false,true,true,true)==A::None); // cannot extend a released combo
  }
  {
    LandscapeGames g;assert(g.begin(1,0,0,0xfffffff0u));g.update(1,0,0,true,284);assert(g.clock==0);
    g.update(1,0,0,true,384);assert(g.clock==100);
    g.update(1,0,0,false,500);assert(!g.active()&&g.paused==Scene::Launch);
    g.cancel();assert(g.paused==Scene::None&&!g.launch.active&&!g.maze.active&&!g.catchStars.active&&!g.exploration.active);
  }
  puts("11 landscape scenarios: bidirectional navigation, defaults, memories, frozen pause, rebase, aim latch, ownership, voice, orientation, wraparound");
}
