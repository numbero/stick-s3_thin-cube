#include "../src/display_motion.h"
#include "../src/pose_detector.h"
#include <assert.h>
#include <stdio.h>

int main(){
  {
    DisplayOrientation d;PoseDetector p;
    assert(p.calibrateUpright(0,1,0));assert(p.calibrateFaceUp(0,0,1));
    d.update(0,-1,0,70,1,100);p.update(0,-1,0,70,100);
    d.update(0,-1,0,70,1,189);assert(d.rotation==0);
    d.update(0,-1,0,70,1,190);assert(d.rotation==2&&d.confirmedAfterMs==90);
    assert(p.update(0,-1,0,70,319)==DevicePose::FaceUp);
    assert(p.update(0,-1,0,70,320)==DevicePose::Inverted);
    d.update(0,0,1,0,1,400);d.update(0,0,1,0,1,1000);assert(d.rotation==2); // Flat keeps the last screen direction.
    p.update(0,0,1,0,400);assert(p.update(0,0,1,0,559)==DevicePose::Inverted);
    assert(p.update(0,0,1,0,560)==DevicePose::FaceUp);
  }
  {
    DisplayOrientation d;
    d.update(0,-1,0,0,1,100);d.update(0,-1,0,0,1,180);assert(d.rotation==0);
    d.update(0,0,1,0,1,181); // A short tilt does not rotate the screen.
    d.update(0,-1,0,0,1,200);d.update(0,-1,0,0,1,289);assert(d.rotation==0);
    d.update(0,-1,0,0,1,290);assert(d.rotation==2);
    d.update(0,1,0,200,1,300);d.update(0,1,0,200,1,1000);assert(d.rotation==2); // Fast shaking is rejected.
    d.update(0,1,0,0,1,1100);d.update(0,1,0,0,1,1190);assert(d.rotation==0);
  }
  {
    PoseDetector p;assert(p.calibrateInverted(0,-1,0));
    // Retain a confirmed pose at the entry-angle boundary instead of flickering.
    p.update(0,-.78f,.55f,60,100);assert(p.update(0,-.78f,.55f,60,500)==DevicePose::Inverted);
    p.update(0,1,0,180,600);assert(p.update(0,1,0,180,900)==DevicePose::Other);
    p.update(0,1,0,0,1000);assert(p.update(0,1,0,0,1219)==DevicePose::Other);
    assert(p.update(0,1,0,0,1220)==DevicePose::Upright);
  }
  {
    PoseDetector p;assert(p.calibrateFaceUp(0,0,1));
    p.update(0,0,-1,0,100);assert(p.update(0,0,-1,0,1599)==DevicePose::FaceUp);
    assert(p.update(0,0,-1,0,1600)==DevicePose::FaceDown); // Rest keeps its longer deliberate hold.
  }
  {
    DisplayOrientation d;PoseDetector p;assert(p.calibrateFaceUp(0,0,1));p.uprightSign=1;
    d.update(0,-1,0,0,1,0xfffffff0u);p.update(0,-1,0,0,0xfffffff0u);
    d.update(0,-1,0,0,1,73);assert(d.rotation==0);
    d.update(0,-1,0,0,1,74);assert(d.rotation==2);
    assert(p.update(0,-1,0,0,203)==DevicePose::FaceUp);
    assert(p.update(0,-1,0,0,204)==DevicePose::Inverted);
  }
  {
    RotationTransition r;assert(!r.request(0,100));assert(r.request(2,100));
    assert(r.step(100)==0&&r.active);assert(fabsf(r.step(190)-90)<.001f);
    assert(!r.request(2,200));assert(r.step(279)<180&&r.active);
    assert(r.step(280)==180&&!r.active);
    assert(r.request(0,300));assert(r.step(480)==0&&!r.active);
  }
  {
    RotationTransition r;r.request(2,100);
    const float before=r.angle(160);assert(before>0&&before<180);
    assert(r.request(0,160));assert(fabsf(r.angle(160)-before)<.001f);
    const uint32_t end=160+r.duration;
    assert(r.step(end)==0&&!r.active); // Rapid reversal does not jump to an endpoint.
    r.request(2,0xfffffff0u);assert(r.step(164)==180&&!r.active);
  }
  {
    for(int degree=-180;degree<=180;degree++){
      const float zoom=RotationTransition::fit(float(degree),135,240);
      const float radians=degree*.01745329252f,c=fabsf(cosf(radians)),s=fabsf(sinf(radians));
      assert(zoom>0&&zoom<=1);
      assert((135*c+240*s)*zoom<=135.001f);
      assert((135*s+240*c)*zoom<=240.001f); // Rotating content fits within the display.
    }
  }
  {
    DisplayOrientation d;
    d.update(1,0,0,0,1,100);d.update(1,0,0,0,1,189);assert(d.rotation==0);
    d.update(1,0,0,0,1,190);assert(d.rotation==1);
    d.update(0,0,1,0,1,200);d.update(0,0,1,0,1,1000);assert(d.rotation==1);
    d.update(-1,0,0,0,1,1100);d.update(-1,0,0,0,1,1190);assert(d.rotation==3);
    d.update(-1,0,0,0,-1,1200);assert(!d.known);
    d.update(-1,0,0,0,-1,1290);assert(d.rotation==1); // Both calibration reference signs.
  }
  {
    DisplayOrientation d;
    d.update(.71f,.71f,0,0,1,100);d.update(.71f,.71f,0,0,1,1000);assert(!d.known);
    d.update(1,0,0,0,1,1100);d.update(0,0,1,0,1,1180);
    d.update(1,0,0,0,1,1200);d.update(1,0,0,0,1,1289);assert(!d.known);
    d.update(1,0,0,0,1,1290);assert(d.rotation==1);
  }
  {
    RotationTransition r;r.request(3,100);
    assert(r.to==-90&&r.duration==120);assert(r.step(220)==-90&&!r.active);
    r.request(0,300);assert(r.to==0);r.step(420);
    r.request(1,500);assert(r.to==90);const float a=r.angle(550);
    r.request(0,550);assert(r.angle(550)==a);assert(r.step(550+r.duration)==0);
  }
  {
    for(int src=0;src<2;src++)for(int dst=0;dst<2;dst++)for(int degree=-360;degree<=360;degree++){
      const float w=src?240:135,h=src?135:240,dw=dst?240:135,dh=dst?135:240;
      const float zoom=RotationTransition::fit(float(degree),w,h,dw,dh);
      const float a=degree*.01745329252f,c=fabsf(cosf(a)),sn=fabsf(sinf(a));
      assert((w*c+h*sn)*zoom<=dw+.001f);assert((w*sn+h*c)*zoom<=dh+.001f);
    }
  }
  {
    DisplayOrientation d;d.update(1,0,0,0,1,100);d.update(1,0,0,0,1,190);assert(d.rotation==1);
    d.update(0,-1,0,0,1,200);assert(d.pending);
    d.update(0,-1,0,0,1,250,true);assert(!d.pending&&d.rotation==1);
    d.update(-1,0,0,0,1,10000,true);assert(d.rotation==1);
    d.update(0,-1,0,0,1,10100);assert(d.rotation==1);
    d.update(0,-1,0,0,1,10189);assert(d.rotation==1);
    d.update(0,-1,0,0,1,10190);assert(d.rotation==2&&d.confirmedAfterMs==90);
  }
  {
    DisplayOrientation d;d.update(-1,0,0,0,1,100);d.update(-1,0,0,0,1,190);assert(d.rotation==3);
    d.update(0,1,0,0,1,0xfffffff0u,true);assert(d.rotation==3);
    d.update(0,1,0,0,1,0xfffffff0u);
    d.update(0,1,0,0,1,73);assert(d.rotation==3);
    d.update(0,1,0,0,1,74);assert(d.rotation==0&&d.confirmedAfterMs==90);
  }
  puts("14 display/pose latency, exploration lock and animation scenarios passed");
}
