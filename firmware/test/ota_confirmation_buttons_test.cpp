#include "../src/ota_confirmation_buttons.h"
#include <cassert>
#include <cstdio>

int main(){
  using Action=OtaConfirmationButtons::Action;
  OtaConfirmationButtons buttons;
  // A press started before the manifest cannot confirm on its release.
  assert(buttons.update(1,true,true,false,false,false)==Action::None);
  assert(buttons.update(1,true,false,false,true,false)==Action::None);
  assert(buttons.update(1,true,true,false,false,false)==Action::None);
  assert(buttons.update(1,true,false,false,true,false)==Action::Confirm);

  // Cancel/retry may publish another CONFIRM between two UI frames.
  assert(buttons.update(2,true,false,false,true,false)==Action::None);
  assert(buttons.update(2,true,false,false,true,false)==Action::Confirm);
  // B is effective before the release gate opens and wins simultaneous clicks.
  assert(buttons.update(3,true,true,false,true,true)==Action::Cancel);
  assert(buttons.update(3,true,false,true,true,false)==Action::None);
  assert(buttons.update(3,true,false,false,false,true)==Action::Cancel);
  assert(buttons.update(3,true,false,false,true,false)==Action::None);
  assert(buttons.update(3,true,false,false,true,false)==Action::Confirm);

  // A mixed generation/stage snapshot must discard the in-flight click.
  assert(buttons.update(3,false,false,false,true,false)==Action::None);
  assert(buttons.update(3,true,false,false,true,false)==Action::None);
  assert(buttons.update(3,true,false,false,true,false)==Action::Confirm);
  assert(buttons.update(0xffffffffu,true,false,false,true,false)==Action::None);
  assert(buttons.update(0,true,false,false,true,false)==Action::None);
  buttons.reset();
  assert(buttons.update(0,true,false,false,true,false)==Action::None);
  assert(buttons.update(0,true,false,false,true,false)==Action::Confirm);
  puts("OTA confirmation requires a fresh click per image; cancellation and mixed-session snapshots stay safe");
}
