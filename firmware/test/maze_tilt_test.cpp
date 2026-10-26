#include "../src/landscape_games.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>

static bool close(float a,float b){return fabsf(a-b)<.002f;}
static void downhillSample(float degrees,uint8_t rotation,float& x,float& y){
  const float angle=degrees*.01745329252f;
  // Body X points up in rotation 1, down in rotation 3. Right-edge-down
  // produces +Y specific force in rotation 1, -Y in rotation 3.
  x=cosf(angle)*(rotation==1?1:-1);y=sinf(angle)*(rotation==1?1:-1);
}
int main(){
  for(uint8_t rotation:{uint8_t(1),uint8_t(3)})for(float downhill:{-12.f,12.f}){
    float x,y;downhillSample(downhill,rotation,x,y);
    const float roll=LandscapeTilt::mazeHorizontalDegrees(x,y,rotation);
    assert(close(roll,downhill));
    assert(close(LandscapeTilt::mazeHorizontalDegrees(x*9.81f,y*9.81f,rotation),downhill));
    assert(close(LandscapeTilt::tiltDegrees(x,y,rotation),-downhill)); // old mapping was opposite
    LandscapeGames g;g.observe(rotation);while(g.selected!=LandscapeGames::Scene::Maze)g.next(rotation);
    float nx,ny;downhillSample(0,rotation,nx,ny);
    assert(g.begin(rotation,LandscapeTilt::mazeHorizontalDegrees(nx,ny,rotation),0,100));
    const float initial=g.maze.x;
    for(uint32_t t=410;t<=750;t+=10)g.update(rotation,roll,0,true,t);
    assert((g.maze.x-initial)*downhill>0&&g.maze.vx*downhill>0);
    const float offset=g.offsetX,position=g.maze.x,velocity=g.maze.vx;
    g.next(rotation);g.observe(rotation==1?3:1);g.observe(rotation);assert(g.resumable());
    downhillSample(7,rotation,nx,ny);
    const float grip=LandscapeTilt::mazeHorizontalDegrees(nx,ny,rotation);
    assert(g.begin(rotation,grip,0,10000));g.update(rotation,grip,0,true,10300);
    assert(g.maze.x==position&&g.maze.vx==velocity);
    g.update(rotation,grip,0,true,10310);assert(close(g.offsetX,offset));
  }
  assert(!isfinite(LandscapeTilt::mazeHorizontalDegrees(0,0,1)));
  assert(!isfinite(LandscapeTilt::mazeHorizontalDegrees(NAN,0,3)));
  assert(!isfinite(LandscapeTilt::mazeHorizontalDegrees(1,0,0)));
  // Use physical gravity samples rather than injecting already mapped values.
  for(uint8_t rotation:{uint8_t(1),uint8_t(3)})for(float roll:{-15.f,0.f,15.f})for(float pitch:{-105.f,-95.f,-90.f,-85.f,-60.f,0.f,60.f,85.f,90.f,95.f,105.f}){
    float r=roll*.01745329252f,p=pitch*.01745329252f;
    float x=cosf(r)*cosf(p)*(rotation==1?1:-1),y=sinf(r)*cosf(p)*(rotation==1?-1:1),z=-sinf(p);
    assert(close(LandscapeTilt::mazePitchDegrees(x,y,z,rotation),pitch));
    assert(close(LandscapeTilt::mazePitchDegrees(x*9.81f,y*9.81f,z*9.81f,rotation),pitch));
  }
  assert(!isfinite(LandscapeTilt::mazePitchDegrees(0,0,0,1)));
  assert(!isfinite(LandscapeTilt::mazePitchDegrees(1,0,NAN,3)));
  assert(!isfinite(LandscapeTilt::mazePitchDegrees(1,0,0,0)));
  for(uint8_t rotation:{uint8_t(1),uint8_t(3)}){
    LandscapeGames g;g.observe(rotation);while(g.selected!=LandscapeGames::Scene::Maze)g.next(rotation);
    assert(g.begin(rotation,0,0,100));g.update(rotation,5,5,true,490);
    assert(close(g.offsetX,1.f/6)); // gain unchanged for a positive screen-coordinate angle
    assert(close(g.offsetY,3.5f/8.5f)&&g.offsetY>g.offsetX*2.4f);
    g.next(rotation);g.observe(rotation==1?3:1);g.observe(rotation);
    assert(g.selected==LandscapeGames::Scene::Maze&&g.resumable());
    const float offset=g.offsetY;assert(g.begin(rotation,12,23,10000));g.update(rotation,12,23,true,10400);
    assert(close(g.offsetY,offset)); // resume uses the matching inverse profile
    g.update(rotation,12,g.baselineY+1.5f,true,10500);assert(close(g.offsetY,0));
    g.update(rotation,12,g.baselineY+3,true,10600);assert(g.offsetY>.17f);
    g.update(rotation,12,g.baselineY+10,true,10700);assert(close(g.offsetY,1));
    g.update(rotation,12,g.baselineY-10,true,10800);assert(close(g.offsetY,-1));
  }
  LandscapeGames launch;assert(launch.begin(1,0,0,100));launch.update(1,5,5,true,490);
  assert(close(launch.offsetX,1.f/6)&&close(launch.offsetY,1.f/6)); // other games retain the old profile
  printf("Maze tilt: low-side rolling in both orientations, paused direction/rebase, 132 pitch/unit pairs, continuous face-up crossing, low-angle response and unchanged gain/launch passed\n");
}
