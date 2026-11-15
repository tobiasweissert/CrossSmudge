#include "WriteAdmission.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>
using namespace recoverable;
int main() {
  auto p = admitWrite(Recovery{}, 0, false);
  assert(p.state == AdmissionState::ready && p.slot == 0 && p.generation == 1);
  std::puts("PASS empty recovery begins generation one");
  Recovery r{RecoveryState::selected, 0, {SlotState::valid, 41, 7, 9}};
  p = admitWrite(r, MaxPayload, false);
  assert(p.state == AdmissionState::ready && p.slot == 1 && p.generation == 42);
  r.slot = 1; p = admitWrite(r, 1, false);
  assert(p.state == AdmissionState::ready && p.slot == 0 && p.generation == 42);
  std::puts("PASS inactive slot preserves selected generation");
  r.record.generation = std::numeric_limits<uint64_t>::max();
  assert(admitWrite(r, 1, false).state == AdmissionState::generation_exhausted);
  std::puts("PASS generation exhaustion never wraps");
  for (auto state : {RecoveryState::uncertain, RecoveryState::conflict}) {
    r.state = state;
    assert(admitWrite(r, 1, false).state == AdmissionState::recovery_required);
  }
  std::puts("PASS uncertain and conflicting recovery deny writes");
  r.state = RecoveryState::selected; r.record.generation = 3;
  assert(admitWrite(r, MaxPayload + 1, false).state == AdmissionState::invalid);
  assert(admitWrite(r, 1, true).state == AdmissionState::cancelled);
  std::puts("PASS size and cancellation deny writes");
  r.slot = 2;
  assert(admitWrite(r, 1, false).state == AdmissionState::invalid);
  r.slot = 0; r.record.generation = 0;
  assert(admitWrite(r, 1, false).state == AdmissionState::invalid);
  r.record.generation = 1; r.record.state = SlotState::invalid;
  assert(admitWrite(r, 1, false).state == AdmissionState::invalid);
  std::puts("PASS malformed selected recovery denied");
}
