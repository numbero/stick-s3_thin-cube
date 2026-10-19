#include "../src/tap_parameters.h"
#include "../src/tap_transaction.h"
#include "../src/tap_diagnostic.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(){
  TapParameters p;assert(p.valid()&&p.enabled&&p.software&&p.mask()==2);
  assert(tapShortcutSource(p,true)&&!tapShortcutSource(p,false));
  p.manual=true;assert(!tapShortcutSource(p,true)&&!tapShortcutSource(p,false));p.manual=false;
  p.software=false;assert(!tapShortcutSource(p,true)&&tapShortcutSource(p,false));p.software=true;
  p.enabled=false;assert(!tapShortcutSource(p,true));p.enabled=true;
  p.enabled=true;p.doubleTap=false;assert(!p.valid());
  p.single=true;assert(p.valid());p.triple=true;assert(p.mask()==5);
  for(unsigned n=0;n<=1100;++n){p.windowMs=n;
    assert(p.valid()==(n>=250&&n<=1000&&n%5==0));}
  p.windowMs=400;
  for(unsigned n=0;n<=600;++n){p.quietMs=n;assert(p.valid()==(n>=250&&n<=500&&n%5==0));}
  p.quietMs=400;p.axis=3;assert(!p.valid());p.axis=2;
  p.sensitivity=16;assert(!p.valid());p.sensitivity=0;assert(p.valid());
  p.settleMs=5001;assert(!p.valid());p.settleMs=0;
  p.buttonGuardMs=5001;assert(!p.valid());p.buttonGuardMs=0;
  p.cooldownMs=10001;assert(!p.valid());p.cooldownMs=0;assert(p.valid());
  p.softGyroFilterMs=1001;assert(!p.valid());p.softGyroFilterMs=100;assert(p.valid());
  TapParameters current,next;next.enabled=true;next.sensitivity=0;
  bool verified=false;unsigned calls=0;
  assert(!strcmp(tapApplyTransaction(current,verified,next,[&](bool on){++calls;assert(on);return true;}),"ok"));
  assert(verified&&current.sensitivity==0&&calls==1);
  next.sensitivity=3;calls=0;
  assert(!strcmp(tapApplyTransaction(current,verified,next,[&](bool on){assert(on);return ++calls==2;}),"write_failed_restored"));
  assert(verified&&current.sensitivity==0&&calls==2);
  calls=0;
  assert(!strcmp(tapApplyTransaction(current,verified,next,[&](bool on){++calls;if(calls==3)assert(!on);return false;}),"readback_failed"));
  assert(!verified&&current.sensitivity==0&&calls==3);
  TapDiagnostic t;t.settleMs=0;t.buttonGuardMs=1000;t.cooldownMs=2000;
  t.update(UINT32_MAX-100,true,true);assert(t.event(UINT32_MAX-100)==TapDiagnostic::Reason::Button);
  t.update(0,true,false);assert(t.event(898)==TapDiagnostic::Reason::Settling);
  assert(t.event(899)==TapDiagnostic::Reason::Accepted);
  assert(t.event(2898)==TapDiagnostic::Reason::Cooldown);
  assert(t.event(2899)==TapDiagnostic::Reason::Accepted);
  assert(t.buttons==1&&t.settling==1&&t.cooldown==1&&t.eventSequence==5);
  t.cooldownMs=0;assert(t.event(2899)==TapDiagnostic::Reason::Accepted);
  puts("tap parameter limits, transactional rollback/disarm, adjustable suppression and wraparound passed");
}
