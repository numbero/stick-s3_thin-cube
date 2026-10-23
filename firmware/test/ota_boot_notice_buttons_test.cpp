#include "../src/ota_boot_notice_buttons.h"
#include <cassert>
#include <cstdio>

int main(){
  OtaBootNoticeButtons buttons;
  // Checking cannot dismiss or pre-arm the Complete page.
  assert(!buttons.update(false,false,false,false,false));
  assert(!buttons.update(false,false,false,true,true));
  assert(!buttons.update(true,false,false,true,false));
  assert(!buttons.update(true,false,false,true,false));
  assert(!buttons.update(true,false,false,false,false));
  assert(!buttons.update(true,true,false,false,false));
  assert(buttons.update(true,false,false,true,false));
  assert(!buttons.update(true,false,false,true,false));
  assert(!buttons.update(true,false,false,false,true));

  // A held across the transition, including its eventual release click,
  // cannot close Complete. A clean released frame and new click are needed.
  buttons.reset();
  assert(!buttons.update(false,true,false,false,false));
  assert(!buttons.update(true,true,false,false,false));
  assert(!buttons.update(true,false,false,true,false));
  assert(!buttons.update(true,false,false,false,false));
  assert(!buttons.update(true,true,false,false,false));
  assert(buttons.update(true,false,false,true,false));

  // B held across the boundary obeys exactly the same gate.
  buttons.reset();
  assert(!buttons.update(false,false,true,false,false));
  assert(!buttons.update(true,false,true,false,false));
  assert(!buttons.update(true,false,false,false,true));
  assert(!buttons.update(true,false,false,false,false));
  assert(!buttons.update(true,false,true,false,false));
  assert(buttons.update(true,false,false,false,true));

  // Both held/released together cannot accidentally consume the new page;
  // after the gate opens, simultaneous fresh clicks dismiss only once.
  buttons.reset();
  assert(!buttons.update(true,true,true,false,false));
  assert(!buttons.update(true,false,false,true,true));
  assert(!buttons.update(true,false,false,false,false));
  assert(!buttons.update(true,true,true,false,false));
  assert(buttons.update(true,false,false,true,true));
  assert(!buttons.update(true,false,false,true,true));

  // Leaving Complete for Checking resets even an already-open gate.
  buttons.reset();
  assert(!buttons.update(true,false,false,false,false));
  assert(!buttons.update(false,false,false,false,false));
  assert(!buttons.update(true,false,false,false,true));
  assert(!buttons.update(true,false,false,false,false));
  assert(buttons.update(true,false,false,false,true));
  // A new notice after a consumed one can be acknowledged independently.
  assert(!buttons.update(false,false,false,false,false));
  assert(!buttons.update(true,false,false,false,false));
  assert(buttons.update(true,false,false,true,false));

  // The UI calls reset on None; no stale click may close the next notice.
  buttons.reset();
  assert(!buttons.update(true,false,false,true,false));
  assert(!buttons.update(true,false,false,false,false));
  assert(buttons.update(true,false,false,true,false));
  puts("OTA boot completion requires a released clean frame then a fresh click; Checking and repeated clicks cannot dismiss");
}
