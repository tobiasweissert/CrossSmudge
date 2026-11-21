#include "../../src/activities/notes/DictationSettings.h"
#include "ui.h"
volatile uint32_t dictationPrivacy::retained = 0;
int main() {
  Activity parent;
  GfxRenderer r;
  MappedInputManager m;
  dictation::openSettings(parent, r, m);
  parent.complete({false, OptionSelectionResult{1}});
  assert(dynamic_cast<ConfirmationActivity*>(parent.child.get()) && !dictationPrivacy::active() &&
         !dictation::configure("fixture-neutral"));
  puts("PASS missing no-dump policy blocks private keyboard and credential acceptance");
}
