#include "../src/maintenance_policy.h"
#include <cassert>
#include <cstdio>

struct Store {
  bool present=true,readable=true,clearable=true;
  MaintenanceRequestRecord saved;
  Store(){strcpy(saved.request,"one-shot-request");}
  bool exists(){return present;}
  bool read(MaintenanceRequestRecord& value){value=saved;return readable;}
  bool clear(){if(!clearable)return false;present=false;return true;}
};

int main(){
  using Action=MaintenanceBootGesture::Action;
  MaintenanceBootGesture normal(false,false,0);
  assert(normal.update(2000,false,false)==Action::Cancel);
  MaintenanceBootGesture shortPress(true,false,100);
  assert(shortPress.update(2099,false,false)==Action::Cancel);
  MaintenanceBootGesture held(true,false,100);
  assert(held.update(2099,true,false)==Action::None&&!held.ready);
  assert(held.update(2100,true,false)==Action::None&&held.ready);
  assert(held.update(2110,false,false)==Action::Enter);
  assert(held.update(2200,false,false)==Action::None);
  MaintenanceBootGesture exactRelease(true,false,100);
  assert(exactRelease.update(2100,false,false)==Action::Enter);
  MaintenanceBootGesture stuck(true,false,0);
  assert(stuck.update(9999,true,false)==Action::None);
  assert(stuck.update(10000,true,false)==Action::Cancel);
  // A+B is latched out even if B is released while A remains held.
  MaintenanceBootGesture chord(true,true,0);
  assert(chord.update(2200,true,false)==Action::Cancel);
  MaintenanceBootGesture interrupted(true,false,0);
  assert(interrupted.update(2200,true,true)==Action::Cancel);
  assert(interrupted.update(2300,false,false)==Action::None);
  MaintenanceBootGesture wrapped(true,false,0xffffff00u);
  assert(wrapped.update(1800,false,false)==Action::Enter);

  Store oneShot;
  assert(maintenanceConsumeRequest(oneShot)==MaintenanceConsume::Requested&&!oneShot.present);
  assert(maintenanceConsumeRequest(oneShot)==MaintenanceConsume::None);
  Store cannotClear;cannotClear.clearable=false;
  assert(maintenanceConsumeRequest(cannotClear)==MaintenanceConsume::StorageFailed&&cannotClear.present);
  cannotClear.clearable=true;
  assert(maintenanceConsumeRequest(cannotClear)==MaintenanceConsume::Requested&&!cannotClear.present);
  Store truncated;truncated.readable=false;
  assert(maintenanceConsumeRequest(truncated)==MaintenanceConsume::Invalid&&!truncated.present);
  Store damaged;damaged.saved.magic=0;
  assert(maintenanceConsumeRequest(damaged)==MaintenanceConsume::Invalid&&!damaged.present);
  Store invalid;invalid.saved.request[0]='\n';
  assert(maintenanceConsumeRequest(invalid)==MaintenanceConsume::Invalid&&!invalid.present);

  assert(!MaintenanceIdlePolicy::expired(300099,100,false));
  assert(MaintenanceIdlePolicy::expired(300100,100,false));
  assert(!MaintenanceIdlePolicy::expired(999999,100,true));
  assert(!MaintenanceIdlePolicy::expired(100,101,false));
  assert(MaintenanceIdlePolicy::expired(299744,0xffffff00u,false));
  assert(!MaintenanceIdlePolicy::expired(299743,0xffffff00u,false));
  MaintenanceSessionAttempts attempts;
  for(unsigned i=0;i<MaintenanceSessionAttempts::MaxFailures;++i){assert(attempts.allowed(100+i));attempts.rejected(100+i);}
  assert(!attempts.allowed(30103));
  assert(attempts.allowed(30104));
  attempts.rejected(30105);attempts.accepted();assert(!attempts.failures&&attempts.allowed(30106));
  puts("maintenance boot chords, one-shot storage recovery, protected idle timeout and session rate limit passed");
}
