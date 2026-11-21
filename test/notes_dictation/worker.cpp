#include "../../src/activities/notes/DictationDevice.h"
#include "host.h"
using namespace dictation;
int main() {
  Job j;
  assert(!j.start() && !posts);
  assert(configure("fixture-neutral"));
  assert(!configure("bad\rkey"));
  assert(j.start());
  j.cancelled = true;
  task();
  assert(!micStarts && !posts && !power && j.state == Job::Idle);
  assert(j.start());
  task();
  assert(!posts && !power && j.state == Job::Failed && strstr(j.error, "expired"));
  tick = [&] {
    if (j.state == Job::Ready) j.cancelled = true;
  };
  assert(j.start());
  task();
  assert(j.state == Job::Idle && !posts);
  tick = [&] {
    if (j.state == Job::Ready) j.send = true;
  };
  status = 401;
  assert(j.start());
  task();
  assert(j.state == Job::Failed && strstr(j.error, "Key rejected") && posts == 1);
  cert = false;
  assert(j.start());
  task();
  assert(j.state == Job::Failed && posts == 1);
  cert = true;
  status = 200;
  response.assign(4097, 'x');
  assert(j.start());
  task();
  assert(j.state == Job::Failed && strstr(j.error, "large") && posts == 2);
  response = "malformed";
  assert(j.start());
  task();
  assert(j.state == Job::Failed && posts == 3);
  response = R"({"text":"Neutral"})";
  assert(j.start());
  task();
  assert(j.state == Job::Review && j.transcript == "Neutral" && posts == 4);
  tick = [&] {
    if (j.state == Job::Ready) {
      forget();
      j.send = true;
    }
  };
  assert(j.start());
  task();
  assert(j.state == Job::Failed && posts == 4);
  assert(!configured() && !j.start() && posts == 4);
  assert(!power && frees == 9 && cleaned == 5);
  assert(configure("fixture-neutral"));
  ink::http::readOK = false;
  assert(!j.start());
  ink::http::readOK = true;
  ink::http::policy = false;
  assert(!j.start());
  ink::http::policy = true;
  auto refuse = [&] {
    wire.clear();
    assert(j.start());
    task();
    assert(j.state == Job::Failed && wire.empty());
  };
  tick = [&] {
    ink::http::policy = false;
    j.send = true;
  };
  refuse();
  ink::http::policy = true;
  tick = [&] { j.send = true; };
  halClock.synced = false;
  refuse();
  halClock.synced = true;
  halClock.skew = -86400;
  refuse();
  halClock.skew = 86400;
  refuse();
  halClock.skew = 0;
  chain.valid_to.year = 2020;
  refuse();
  chain.valid_to.year = 2100;
  chain.valid_from.year = 2099;
  refuse();
  chain.valid_from.year = 2020;
  puts(
      "PASS refusal before first HTTP byte: policy read failure/opt out, send-time opt out, no sync, skew both ways, "
      "expired/future certificate");
  puts(
      "PASS actual worker: missing key; queued and pre-send cancel; expiry; one-shot 401; rejected connection; "
      "oversized/malformed reply; fresh successful recording; forget and cleanup");
}
