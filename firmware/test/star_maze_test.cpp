#include "../src/star_maze.h"
#include <assert.h>
#include <stdio.h>

int main(){
  {
    // Hold into a vertical face, then slide up it. Before the fix the zero-Y
    // tick teleported the ball from y=99 to y=112.0045, trapping it forever.
    for(unsigned level=0;level<3;level++){
      StarMaze wall;wall.begin(level,0);const float sx=level==1?-1:1,sy=level==2?-1:1;
      auto valid=[&](){
        assert(wall.x>=wall.Left+wall.Radius-.001f&&wall.x<=wall.Right-wall.Radius+.001f);
        assert(wall.y>=wall.Top+wall.Radius-.001f&&wall.y<=wall.Bottom-wall.Radius+.001f);
        for(const auto& w:wall.walls){
          float dx=wall.x-wall.clamp(wall.x,w.x,w.x+w.w),dy=wall.y-wall.clamp(wall.y,w.y,w.y+w.h);
          assert(hypotf(dx,dy)>=wall.Radius-.002f);
        }
      };
      for(uint32_t t=10;t<=1000;t+=10){wall.update(sx,0,t);valid();}
      const float y=wall.y;assert(fabsf(y-(level==2?47.f:99.f))<.001f);
      for(uint32_t t=1010;t<=2100;t+=10){wall.update(sx,-sy,t);valid();}
      assert((wall.y-y)*sy<-25); // into-wall pressure must not stop sliding
    }
    // Sliding along the bottom of a wall must also work with Y pressure held.
    StarMaze wall;wall.begin(0,0);wall.x=106;
    for(uint32_t t=10;t<=400;t+=10)wall.update(0,-1,t);
    const float y=wall.y,x=wall.x;assert(y>92&&y<93);
    for(uint32_t t=410;t<=1100;t+=10)wall.update(1,-1,t);
    assert(wall.x>x+10&&wall.y<y+1);
  }
  // Follow the alternating gates in each reflected version of the maze.
  const float route[][2]={{96,99},{118,99},{118,45},{148,45},{148,99},{177,99},{177,45},{218,45}};
  for(unsigned level=0;level<3;level++){
    StarMaze m;m.begin(level,0);uint32_t now=0;
    for(const auto& point:route){
      const float gx=level==1?306-point[0]:point[0],gy=level==2?146-point[1]:point[1];
      for(unsigned tick=0;tick<800&&m.active;tick++){
        float tx=StarMaze::clamp((gx-m.x)*.2f-m.vx*.07f,-1,1),ty=StarMaze::clamp((gy-m.y)*.2f-m.vy*.07f,-1,1);
        now+=10;m.update(tx,ty,now);
        for(const auto& w:m.walls){
          const float dx=m.x-StarMaze::clamp(m.x,w.x,w.x+w.w),dy=m.y-StarMaze::clamp(m.y,w.y,w.y+w.h);
          assert(hypotf(dx,dy)>=StarMaze::Radius-.001f);
        }
        if(hypotf(m.x-gx,m.y-gy)<1.5f&&hypotf(m.vx,m.vy)<3)break;
      }
    }
    assert(m.finished&&!m.active);assert(m.vx==0&&m.vy==0);
  }
  StarMaze m;m.begin(0,0);
  for(uint32_t now=10;now<2000;now+=10)m.update(0,-1,now);
  assert(m.y>=StarMaze::Top+StarMaze::Radius-.001f);
  const float x=m.x;m.update(1,0,100000);assert(m.x-x<7); // bounded catch-up
  m.update(NAN,NAN,100010);assert(isfinite(m.x)&&isfinite(m.y));
  m.cancel();assert(!m.active&&!m.finished);
  puts("Maze: three routes reachable, tangent sliding on both axes, no edge teleport, walls solid, inertia bounded, invalid input safe");
}
