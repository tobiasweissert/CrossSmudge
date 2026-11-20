#include "FindMySound.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <type_traits>
using namespace FindMySound;
struct Fake {
  int starts = 0, stops = 0;
  uint32_t duration = 0;
  bool authorized = true, startOk = true, stopOk = true;
  static bool start(void* p, uint32_t ms) { auto& f = *static_cast<Fake*>(p); ++f.starts; f.duration = ms; return f.startOk; }
  static bool stop(void* p) { auto& f = *static_cast<Fake*>(p); ++f.stops; return f.stopOk; }
  static bool auth(void* p) { return static_cast<Fake*>(p)->authorized; }
  Driver driver() { return {this, start, stop, auth}; }
};
int main() {
  static_assert(!std::is_copy_constructible<Controller>::value);
  static_assert(!std::is_move_constructible<Controller>::value);
  for (unsigned mask = 0; mask < 7; ++mask) {
    Fake missing;
    Driver d = missing.driver();
    if (!(mask & 1)) d.start = nullptr;
    if (!(mask & 2)) d.stop = nullptr;
    if (!(mask & 4)) d.localAuthorized = nullptr;
    Controller partial(d);
    assert(!partial.available() && !partial.setEnabled(true));
    assert(partial.start(Source::LocalManual, 1, 0) == Result::Disabled);
    assert(partial.stop() && partial.setEnabled(false));
    partial.tick(10);
    assert(missing.starts == 0 && missing.stops == 0);
  }
  Fake f;
  Controller c(f.driver());
  assert(c.available());
  assert(c.setEnabled(true));
  assert(c.enabled());
  for (Source s : {Source::NativeOwner, Source::NonOwner, Source::Unknown, static_cast<Source>(255)}) {
    assert(c.start(s, 100, 0) == Result::UnsupportedSource);
  }
  f.authorized = false;
  assert(c.start(Source::LocalManual, 100, 0) == Result::Unauthorized);
  f.authorized = true;
  assert(c.start(Source::LocalManual, 100, 0) == Result::Started);
  assert(f.starts == 1 && f.duration == 100);
  assert(!Controller({}).setEnabled(true));
  Fake bounded;
  Controller b(bounded.driver());
  assert(b.setEnabled(true));
  assert(b.start(Source::LocalManual, 0, 0) == Result::InvalidDuration);
  assert(b.start(Source::LocalManual, 5001, 0) == Result::InvalidDuration);
  assert(b.start(Source::LocalManual, UINT32_MAX, 0) == Result::InvalidDuration);
  assert(bounded.starts == 0);
  assert(b.start(Source::LocalManual, 5000, UINT32_MAX - 10) == Result::Started);
  assert(b.active());
  assert(b.start(Source::LocalManual, 5000, 0) == Result::Busy);
  b.tick(4988);
  assert(b.active() && bounded.stops == 0);
  b.tick(4989);
  assert(!b.active() && bounded.stops == 1);
  assert(b.start(Source::LocalManual, 1, 5000) == Result::Started);
  assert(b.setEnabled(false));
  assert(!b.active() && bounded.stops == 2 && !b.enabled());
  Fake revoked;
  Controller r(revoked.driver());
  assert(r.setEnabled(true));
  assert(r.start(Source::LocalManual, 1000, 0) == Result::Started);
  revoked.authorized = false;
  r.tick(1);
  assert(!r.active() && revoked.stops == 1);
  assert(r.stop());
  assert(revoked.stops == 1);
  assert(r.start(Source::LocalManual, 100, 2) == Result::Unauthorized);
  revoked.authorized = true;
  assert(r.start(Source::LocalManual, 100, 2) == Result::Started);
  assert(r.stop());
  assert(!r.active() && revoked.stops == 2);
  Fake failed;
  Controller e(failed.driver());
  assert(e.setEnabled(true));
  failed.startOk = false;
  failed.stopOk = false;
  assert(e.start(Source::LocalManual, 100, 0) == Result::DriverFailure);
  assert(e.active() && !e.enabled() && e.faulted());
  assert(failed.stops == 1);
  assert(!e.setEnabled(true));
  assert(!e.stop());
  assert(e.active() && e.faulted());
  assert(!e.setEnabled(false));
  failed.stopOk = true;
  assert(e.stop());
  assert(!e.active() && !e.faulted() && !e.enabled());
  failed.startOk = true;
  assert(e.setEnabled(true));
  assert(e.start(Source::LocalManual, 100, 0) == Result::Started);
  failed.stopOk = false;
  e.tick(100);
  assert(e.active() && e.faulted() && !e.enabled());
  failed.stopOk = true;
  e.tick(101);
  assert(!e.active() && !e.faulted());
  Fake revokeFailure;
  Controller rf(revokeFailure.driver());
  assert(rf.setEnabled(true));
  assert(rf.start(Source::LocalManual, 5000, 0) == Result::Started);
  revokeFailure.authorized = false;
  revokeFailure.stopOk = false;
  rf.tick(1);
  assert(rf.faulted() && rf.active() && !rf.enabled());
  revokeFailure.authorized = true;
  revokeFailure.stopOk = true;
  rf.tick(2);
  assert(!rf.faulted() && !rf.active() && !rf.enabled());
  Fake cleanup;
  {
    Controller scoped(cleanup.driver());
    assert(scoped.setEnabled(true));
    assert(scoped.start(Source::LocalManual, 100, 0) == Result::Started);
  }
  assert(cleanup.stops == 1);
  Controller absent({});
  assert(!absent.available());
  assert(!absent.enabled());
  assert(absent.start(Source::LocalManual, 100, 0) == Result::Disabled);
  std::puts("sound: PASS");
}
