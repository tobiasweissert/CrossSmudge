#if !defined(CROSSCUB_VERIFIED_PLUGIN_TRANSPORT) || CROSSCUB_VERIFIED_PLUGIN_TRANSPORT != 1
int main() {
  bool complete = true;
  halClock.valid = false;
  halClock.syncOk = true;
  assert(pluginhttp::networkEnabled());
  int s = exchange(
      "https://public.example/", "GET", "", {}, [](int, const auto&, long) { return true; },
      [](const uint8_t*, size_t) { return true; }, complete, 1024, nullptr);
  assert(s == -1 && !complete && created == 0 && dnsCalls == 0 && halClock.syncCalls == 0);
  assert(diagnostics == std::vector<std::string>{"stage=1"});
  puts("PASS unqualified build blocks before DNS while network permission defaults allowed");
}
#else
static void reset(std::vector<std::string> r = {"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"}) {
  assert(created == destroyed);
  created = destroyed = dnsCalls = 0;
  diagnostics.clear();
  writes.clear();
  hosts.clear();
  targets.clear();
  replies = std::move(r);
  dnsAnswers = {"8.8.8.8"};
  tick = advance = 0;
  handshake = 1;
  verifyFlags = 0;
  certificate = {};
  halClock = {};
  halClock.valid = true;
  clockSeconds = 1795080000;
  networkAllowed = true;
  fs = {};
  Storage = {};
}
static bool done;
static std::string output;
static pluginhttp::Headers received;
static int run(const std::string& url = "https://public.example/book?q=one two", const std::string& method = "GET",
               const std::string& body = "", const pluginhttp::Headers& h = {}, size_t cap = 1024,
               const std::function<bool()>& cancel = nullptr) {
  output.clear();
  received.clear();
  int s = exchange(
      url, method, body, h,
      [](int, const auto& headers, long) {
        received = headers;
        return true;
      },
      [](const uint8_t* p, size_t n) {
        output.append(reinterpret_cast<const char*>(p), n);
        return true;
      },
      done, cap, cancel);
  assert(created == destroyed);
  assert(diagnostics.size() <= 1);
  for (const auto& diagnostic : diagnostics) {
    bool allowed = false;
    for (unsigned code = 1; code <= 11; ++code) allowed = allowed || diagnostic == "stage=" + std::to_string(code);
    assert(allowed);  // No URL, header, token, body, IP or exception text.
  }
  return s;
}
int main() {
  // Per-boot HAL flag is false even when persisted Wi-Fi/clock preferences
  // suppressed Wi-Fi's first-time sync, or the radio was already connected.
  reset();
  halClock.valid = false;
  halClock.syncOk = true;
  assert(run() == 200 && done && output == "ok");
  assert(halClock.syncCalls == 1 && dnsCalls == 1 && !writes[0].empty());
  assert(timeEpoch == clockSeconds && diagnostics.empty());
  assert(run() == -1);  // Fixture response is consumed; no resync on next request.
  assert(halClock.syncCalls == 1);
  for (unsigned scenario = 0; scenario < 2; ++scenario) {
    reset();
    timeEpoch = timeMono = 0;
    halClock.valid = false;  // saved sync flags / already-connected style
    halClock.syncOk = true;
    halClock.duringSync = [] { tick += 6000; };
    assert(run() == 200 && done && output == "ok" && halClock.syncCalls == 1);
    assert(timeEpoch == clockSeconds && timeMono == 6000000);
  }
  for (unsigned stop = 0; stop < 3; ++stop) {
    reset();
    timeEpoch = timeMono = 0;
    halClock.valid = false;
    halClock.syncOk = true;
    bool cancelled = false;
    halClock.duringSync = [&] {
      if (stop == 0) networkAllowed = false;
      if (stop == 1) cancelled = true;
      if (stop == 2) tick += 30000;
    };
    assert(run("https://public.example/", "GET", "", {}, 1024, [&] { return cancelled; }) == -1);
    assert(!done && halClock.syncCalls == 1 && dnsCalls == 0 && created == 0);
    assert(timeEpoch == clockSeconds);  // anchor precedes post-NTP callbacks
    assert(diagnostics == std::vector<std::string>{"stage=10"});
  }
  reset();
  timeEpoch = timeMono = 0;
  halClock.valid = false;
  assert(run() == -1 && dnsCalls == 0 && halClock.syncCalls == 1);
  assert(diagnostics == std::vector<std::string>{"stage=2"});
  reset();
  halClock.valid = false;
  bool checkedAfterFailedNtp = false;
  assert(run("https://public.example/", "GET", "", {}, 1024, [&] {
           if (halClock.syncCalls) checkedAfterFailedNtp = true;
           return halClock.syncCalls > 0;
         }) == -1);
  assert(checkedAfterFailedNtp && dnsCalls == 0 && created == 0);
  assert(diagnostics == std::vector<std::string>{"stage=10"});
  reset();
  timeEpoch = timeMono = 0;
  halClock.valid = false;
  halClock.syncOk = true;
  halClock.duringSync = [] { clockSeconds = 1; };
  assert(run() == -1 && halClock.syncCalls == 1 && dnsCalls == 0 && created == 0);
  assert(diagnostics == std::vector<std::string>{"stage=2"});
  for (unsigned deny = 0; deny < 3; ++deny) {
    reset();
    halClock.valid = false;
    halClock.syncOk = true;
    if (deny == 0) networkAllowed = false;
    assert(run(deny == 2 ? "http://invalid/" : "https://public.example/", "GET", "", {}, 1024,
               [=] { return deny == 1; }) == -1);
    assert(dnsCalls == 0 && created == 0 && halClock.syncCalls == 0);
  }
  reset();
  timeEpoch = timeMono = 0;
  assert(run() == 200 && halClock.syncCalls == 0);
  clockSeconds = 1;
  replies.push_back("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
  assert(run() == 200 && halClock.syncCalls == 0);
  puts("PASS cold boot prepares UTC before actual HTTP exchange");
  reset();
  assert(run() == 200 && done && output == "ok");
  assert(dnsCalls == 1 && targets[0] == "8.8.8.8" && hosts[0] == "public.example");
  assert(writes[0].find("GET /book?q=one%20two HTTP/1.1") != std::string::npos);
  puts("PASS public numeric pin, original TLS identity, verified configuration, escaping");
  for (const char* s :
       {"http://public.example/", "https://u:p@public.example/", "https://[::1]/", "https://[::ffff:127.0.0.1]/",
        "https://public.example%2f@evil/", "https://public.example\\@evil/", "https://public.example:0/",
        "https://public.example:65536/", "https://public.example\r\nx: y/"}) {
    reset();
    assert(run(s) == -1 && dnsCalls == 0 && created == 0);
  }
  for (const char* s :
       {"0.0.0.0", "10.0.0.1", "127.0.0.1", "169.254.1.1", "172.16.0.1", "192.168.1.1", "100.64.0.1", "192.0.0.9",
        "192.0.2.1", "198.18.0.1", "198.51.100.1", "203.0.113.1", "224.0.0.1", "255.255.255.255"}) {
    reset();
    dnsAnswers = {s};
    assert(run() == -1 && created == 0);
  }
  reset();
  dnsAnswers = {"8.8.8.8", "127.0.0.1"};
  assert(run() == -1 && created == 0);
  assert(diagnostics == std::vector<std::string>{"stage=4"});
  reset();
  dnsAnswers = {};
  assert(run() == -1 && created == 0);
  assert(diagnostics == std::vector<std::string>{"stage=3"});
  puts("PASS URL/userinfo/IPv6 denial and all-answer public IPv4 vetting");
  reset();
  verifyFlags = 1;
  assert(run() == -1 && writes[0].empty());
  assert(diagnostics == std::vector<std::string>{"stage=6"});
  reset();
  handshake = -1;
  assert(run() == -1 && writes[0].empty());
  assert(diagnostics == std::vector<std::string>{"stage=5"});
  reset();
  certificate.valid_to.year = 2021;
  assert(run() == -1 && writes[0].empty());
  assert(diagnostics == std::vector<std::string>{"stage=7"});
  reset();
  certificate.valid_from.year = 2090;
  assert(run() == -1 && writes[0].empty());
  reset();
  halClock.valid = false;
  assert(run() == -1 && created == 0);
  reset();
  assert(run("https://public.example/", "GET", "", {}, 1024, [] { return true; }) == -1 && dnsCalls == 0);
  reset();
  handshake = 0;
  assert(run() == -1 && tick >= 30000 && writes[0].empty());
  reset();
  networkAllowed = false;
  assert(run() == -1 && dnsCalls == 0);
  reset();
  assert(run("https://public.example/", "GET", "", {}, 1024, [] { return dnsCalls > 0; }) == -1 && created == 0);
  reset();
  assert(run("https://public.example/", "GET", "", {}, 1024, [] { return created > 0; }) == -1 && destroyed == 1 &&
         writes[0].empty());
  reset();
  assert(run("https://public.example/", "GET", "", {}, 1024, [] { return !writes.empty() && !writes[0].empty(); }) ==
             -1 &&
         !done);
  reset();
  bool refusedComplete = true;
  assert(exchange(
             "https://public.example/", "GET", "", {}, [](int, const auto&, long) { return false; },
             [](const uint8_t*, size_t) { return true; }, refusedComplete, 1024, nullptr) == -1 &&
         !refusedComplete);
  reset();
  assert(exchange(
             "https://public.example/", "GET", "", {}, [](int, const auto&, long) { return true; },
             [](const uint8_t*, size_t) { return false; }, refusedComplete, 1024, nullptr) == -1 &&
         !refusedComplete);
  reset();
  pluginhttp::setNetworkEnabled(false);
  assert(!pluginhttp::networkEnabled() && run() == -1 && dnsCalls == 0);
  pluginhttp::setNetworkEnabled(true);
  reset();
  clockSeconds = 1740000000;
  certificate.valid_to = {2025, 12, 31, 23, 59, 59};
  assert(run() == -1 && writes[0].empty());
  reset();
  clockSeconds = 4000000000;
  certificate.valid_from.year = 2090;
  assert(run() == -1 && writes[0].empty());
  puts("PASS monotonic TLS time ignores later backward/forward system-clock edits");
  puts("PASS doubled TLS/chain/date/clock/cancel/elapsed/opt-out failure before HTTP");
  for (const auto& h : std::vector<pluginhttp::Headers>{{{"Host", "evil"}},
                                                        {{"Content-Length", "0"}},
                                                        {{"Transfer-Encoding", "chunked"}},
                                                        {{"Cookie", "secret\r\nX: bad"}},
                                                        {{"Bad Name", "x"}},
                                                        {{"Proxy-Authorization", "x"}}}) {
    reset();
    assert(run("https://public.example/", "POST", "payload", h) == -1 && dnsCalls == 0);
  }
  reset();
  assert(run("https://public.example/", "GET\r\nx", "", {}) == -1 && dnsCalls == 0);
  reset();
  assert(run("https://public.example/", "POST", std::string(65537, 'x'), {}) == -1 && dnsCalls == 0);
  puts("PASS request injection/framing/upload bounds");
  reset({"secret-response-body-not-a-status\r\n"});
  assert(run("https://public.example/secret-url?token=secret-query", "POST", "secret-body",
             {{"Authorization", "Bearer secret-token"}, {"Cookie", "secret-cookie"}}) == -1);
  assert(diagnostics == std::vector<std::string>{"stage=8"});
  puts("PASS bounded stage-only diagnostics omit URL/headers/token/body/IP");
  for (const char* method : {"GET", "HEAD", "POST", "PUT", "PATCH", "DELETE", "OPTIONS"}) {
    reset();
    assert(run("https://public.example/", method, "payload",
               {{"Authorization", "Bearer canary"}, {"Content-Type", "application/json"}}) == 200);
    assert(writes[0].find(std::string(method) + " / HTTP/1.1") != std::string::npos);
    assert(writes[0].find("Bearer canary") != std::string::npos);
    assert(writes[0].find("payload") != std::string::npos);
  }
  reset({"HTTP/1.1 302 Found\r\nLocation: https://other.example/\r\nContent-Length: 0\r\n\r\n"});
  assert(run("https://public.example/", "GET", "secret-body") == 302 && created == 1);
  puts("PASS no body-bearing GET redirect replay");
  reset(
      {"HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Bearer realm=x\r\nSet-Cookie: a\r\nSet-Cookie: "
       "b\r\nContent-Length: 7\r\n\r\npending"});
  assert(run() == 401 && done && output == "pending" && received.size() == 4);
  puts("PASS modern verbs/payload/headers, OAuth error bodies and duplicate response fields");
  reset({"HTTP/1.1 302 Found\r\nLocation: https://other.example/\r\n\r\n",
         "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"});
  assert(run("https://public.example/", "GET", "",
             {{"Authorization", "Bearer canary"}, {"Cookie", "cookie-canary"}, {"X-Key", "custom-canary"}}) == 200);
  assert(dnsCalls == 2 && writes[1].find("canary") == std::string::npos && hosts[1] == "other.example");
  reset({"HTTP/1.1 302 Found\r\nLocation: /next\r\n\r\n", "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok"});
  assert(run("https://public.example/", "GET", "", {{"Authorization", "Bearer canary"}}) == 200 &&
         writes[1].find("canary") != std::string::npos);
  for (const char* location : {"http://public.example/", "https://a@public.example/", "https://[::1]/"}) {
    reset({std::string("HTTP/1.1 302 Found\r\nLocation: ") + location + "\r\n\r\n"});
    assert(run() == -1 && created == 1);
  }
  reset({"HTTP/1.1 302 Found\r\nLocation: /next\r\nContent-Length: 0\r\n\r\n"});
  assert(run("https://public.example/", "POST", "mutation") == 302 && created == 1);
  puts("PASS redirect revalidation, same-origin retention, cross-origin credential stripping, no mutation replay");
  for (const char* response :
       {"HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nok",
        "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\nok",
        "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nTransfer-Encoding: chunked\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\n",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nok\r\n0\r\n",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nokXX0\r\n\r\n",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nAuthorization: evil\r\n\r\n"}) {
    reset({response});
    assert(run() == -1 && !done);
  }
  reset({"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nok\r\n0\r\n\r\n"});
  assert(run() == 200 && output == "ok" && done);
  reset({"HTTP/1.1 200 OK\r\n\r\nok"});
  assert(run() == 200 && done && output == "ok");
  reset();
  assert(run("https://public.example/", "GET", "", {}, 1) == -1 && !done);
  reset({"HTTP/1.1 200 OK\r\nX: " + std::string(4097, 'x') + "\r\n\r\n"});
  assert(run() == -1);
  reset();
  advance = 1000;
  assert(run() == -1 && !done);
  puts("PASS fixed/chunked/close framing, truncation, conflicting lengths, trailers, caps and shared elapsed budget");
  std::string chunks = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
  for (int i = 0; i < 2000; ++i) chunks += "1\r\nx\r\n";
  chunks += "0\r\n\r\n";
  reset({chunks});
  assert(run("https://public.example/", "GET", "", {}, 3000) == 200 && output.size() == 2000);
  puts("PASS bounded chunk framing supports nontrivial streamed documents");
  reset();
  String out;
  out.s = "stale";
  assert(pluginhttp::request(nullptr, "https://public.example/", "GET", "", {}, out, 10, nullptr, nullptr) == 200 &&
         out.s == "ok");
  reset();
  out.s = "stale";
  assert(pluginhttp::request(nullptr, "https://public.example/", "GET", "", {}, out, 1, nullptr, nullptr) == -1 &&
         out.s.empty());
  reset();
  assert(pluginhttp::requestToFile(nullptr, "https://public.example/", "GET", "", {}, "/book.epub", 10, nullptr) ==
             200 &&
         fs.data == "ok");
  reset();
  fs.syncOk = false;
  assert(pluginhttp::requestToFile(nullptr, "https://public.example/", "GET", "", {}, "/book.epub", 10, nullptr) ==
             -1 &&
         Storage.removed);
  reset();
  assert(pluginhttp::requestToFile(nullptr, "https://public.example/", "GET", "", {}, "/book.epub", 10,
                                   [] { return true; }) == -1 &&
         !fs.opened && !Storage.removed);
  puts("PASS real buffer/file wrappers, stale-output clearing, sync error and cancellation before file open");
}
#endif
