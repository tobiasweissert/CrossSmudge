#include <DictationPrivacy.h>

#include "../../src/activities/notes/DictationSettings.h"
#include "ui.h"
volatile uint32_t dictationPrivacy::retained = 0;
int main() {
  Activity parent;
  GfxRenderer r;
  MappedInputManager m;
  auto select = [&](uint8_t index) {
    dictation::openSettings(parent, r, m);
    auto options = dynamic_cast<OptionSelectionActivity*>(parent.child.get());
    assert(options && options->labels.size() == 3);
    parent.complete({false, OptionSelectionResult{index}});
  };
  select(0);
  assert(dynamic_cast<QrDisplayActivity*>(parent.child.get())->payload == "https://platform.openai.com/api-keys");
  parent.complete({});
  select(1);
  assert(dictationPrivacy::active());
  assert(dynamic_cast<KeyboardEntryActivity*>(parent.child.get()));
  parent.complete({false, KeyboardResult{"fixture-neutral"}});
  assert(dictation::configured());
  select(1);
  parent.complete({true, KeyboardResult{"ignored"}});
  assert(dictation::configured());
  select(1);
  parent.complete({false, KeyboardResult{"bad\nkey"}});
  assert(dynamic_cast<ConfirmationActivity*>(parent.child.get()));
  parent.complete({});
  select(2);
  assert(!dictation::configured() && !posts);
  puts(
      "PASS actual Settings QR payload, empty masked input, RAM provisioning, cancel, invalid replacement, forget; "
      "zero requests");
}
