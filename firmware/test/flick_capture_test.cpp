#include "../src/flick_capture.h"
#include <assert.h>
#include <limits>
#include <stdio.h>

int main(){
  FlickCapturePoint points[3]{};
  FlickCaptureBuffer b(points,3);
  auto point=[](uint32_t ms){return FlickCapturePoint{ms,0,-1,0,10,20,30,5,2,0,0};};
  assert(!FlickCaptureBuffer().start(0));
  assert(b.start(100)&&b.active());
  assert(b.append(point(110)));assert(!b.start(120)&&b.count==1);
  auto bad=point(120);bad.gx=std::numeric_limits<float>::quiet_NaN();
  assert(!b.append(bad)&&b.count==1);
  assert(b.append(point(140))&&b.maxGap==30);
  b.finish();b.finish();assert(!b.active()&&b.stop==FlickCaptureBuffer::Stop::Requested);
  assert(!b.append(point(150))&&b.count==2&&points[0].ms==110);
  assert(b.start(200)&&b.count==0&&b.maxGap==0);
  for(uint32_t t=210;t<=230;t+=10)assert(b.append(point(t)));
  assert(b.stop==FlickCaptureBuffer::Stop::Full&&!b.append(point(240)));
  assert(points[0].ms==210&&points[2].ms==230);
  assert(b.start(1000));assert(b.append(point(60999)));
  assert(!b.append(point(61000))&&b.stop==FlickCaptureBuffer::Stop::Timeout&&b.count==1);
  assert(b.start(0xfffffff0u));assert(b.append(point(10)));
  b.tick(59983);assert(b.active());b.tick(59984);assert(!b.active());
  puts("offline capture: bounded storage, frozen export, explicit restart, timeout, finite data and clock wrap passed");
}
