// Tests private admission in the same extracted production translation unit.
int main() {
  assert(pluginhttp::networkEnabled());
  const std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
  auto resetProbe = [&] {
    assert(created == destroyed);
    created = destroyed = dnsCalls = 0;
    writes.clear();
    hosts.clear();
    targets.clear();
    diagnostics.clear();
    replies = {response};
    dnsAnswers = {"8.8.8.8"};
    handshake = 1;
    verifyFlags = 0;
    certificate = {};
    halClock = {};
    tick = advance = 0;
    timeEpoch = timeMono = 0;
    networkAllowed = true;
    writeLimit = SIZE_MAX;
    writeFailAfter = SIZE_MAX;
  };
  auto invoke = [&](const std::string& url, const std::string& method, const std::string& body,
                    const pluginhttp::Headers& headers, Probe& probe, Admission lane = Admission::Diagnostic) {
    bool complete = false;
    int status = exchange(
        url, method, body, headers, [](int, const auto&, long) { return true; },
        [](const uint8_t*, size_t) { return true; }, complete, 32768, nullptr, lane, &probe);
    assert(created == destroyed);
    return status;
  };
  resetProbe();
  Probe generic;
  assert(invoke(diagnosticTarget(0), "GET", "", {}, generic, Admission::Generic) == -1);
  assert(dnsCalls == 0 && created == 0 && halClock.syncCalls == 0);
  for (const char* bad : {"https://public.example/", "https://expired.badssl.com/?x=1",
                          "https://expired.badssl.com:443/", "https://u:p@expired.badssl.com/"}) {
    resetProbe();
    Probe p;
    assert(invoke(bad, "GET", "", {}, p) == -1);
    assert(dnsCalls == 0 && created == 0 && halClock.syncCalls == 0);
  }
  for (unsigned kind = 0; kind < 3; ++kind) {
    resetProbe();
    Probe p;
    assert(invoke(diagnosticTarget(0), kind == 0 ? "POST" : "GET", kind == 1 ? "x" : "",
                  kind == 2 ? pluginhttp::Headers{{"Authorization", "secret"}} : pluginhttp::Headers{}, p) == -1);
    assert(dnsCalls == 0 && created == 0 && halClock.syncCalls == 0);
  }
  resetProbe();
  Probe oversized;
  bool complete = false;
  assert(exchange(
             diagnosticTarget(0), "GET", "", {}, [](int, const auto&, long) { return true; },
             [](const uint8_t*, size_t) { return true; }, complete, 32769, nullptr, Admission::Diagnostic,
             &oversized) == -1);
  assert(!complete && dnsCalls == 0 && created == 0 && halClock.syncCalls == 0);
  resetProbe();
  Probe positive;
  assert(invoke(diagnosticTarget(0), "GET", "", {}, positive) == 200);
  assert(positive.httpBytes == writes[0].size() && positive.httpWrites > 0);
  for (unsigned bad = 0; bad < 4; ++bad) {
    resetProbe();
    Probe p;
    if (bad == 0) handshake = -1;
    if (bad == 1) verifyFlags = 1;
    if (bad == 2) certificate.valid_to.year = 2021;
    if (bad == 3) certificate.valid_from.year = 2090;
    assert(invoke(diagnosticTarget(2), "GET", "", {}, p) == -1);
    assert(p.httpBytes == 0 && p.httpWrites == 0 && writes[0].empty());
  }
  resetProbe();
  Probe partial;
  writeLimit = 7;
  writeFailAfter = 1;
  assert(invoke(diagnosticTarget(0), "GET", "", {}, partial) == -1);
  assert(partial.httpBytes == 7 && partial.httpWrites == 1 && writes[0].size() == 7);
  resetProbe();
  Probe noDns;
  dnsAnswers.clear();
  assert(invoke(diagnosticTarget(2), "GET", "", {}, noDns) == -1);
  assert(noDns.stage == ProbeStage::Dns && noDns.httpBytes == 0);
  resetProbe();
  Probe timeout;
  handshake = 0;
  assert(invoke(diagnosticTarget(2), "GET", "", {}, timeout) == -1);
  assert(timeout.stage == ProbeStage::Tls && timeout.httpBytes == 0);
  resetProbe();
  Probe redirect;
  replies = {"HTTP/1.1 302 Found\r\nLocation: https://public.example/\r\nContent-Length: 0\r\n\r\n"};
  assert(invoke(diagnosticTarget(0), "GET", "", {}, redirect) == -1 && dnsCalls == 1);
  resetProbe();
  Probe denied;
  networkAllowed = false;
  assert(invoke(diagnosticTarget(0), "GET", "", {}, denied) == -1 && dnsCalls == 0);
  resetProbe();
  Probe cold;
  halClock.valid = false;
  halClock.syncOk = true;
  assert(invoke(diagnosticTarget(0), "GET", "", {}, cold) == 200 && halClock.syncCalls == 1);
  resetProbe();
  std::vector<std::string> records;
  pluginhttp::runTransportDiagnostics([&](const char* line) { records.emplace_back(line); });
  assert(records.size() == 7);
  for (const auto& line : records) {
    assert(line.find("\"qualification\":\"UNQUALIFIED\"") != std::string::npos);
    assert(line.find("\"http_bytes\":") != std::string::npos);
    assert(line.find("https:") == std::string::npos && line.find("secret") == std::string::npos);
  }
  resetProbe();
  serialCommand("CMD:PLUGIN_DIAGNOSTICS");
  assert(logSerial.records.empty() && dnsCalls == 0 && halClock.syncCalls == 0);
  fileTransferAllowed = true;
  serialCommand("CMD:PLUGIN_DIAGNOSTICS https://public.example/");
  assert(logSerial.records.empty() && dnsCalls == 0);
  serialCommand("CMD:PLUGIN_DIAGNOSTICS");
  assert(logSerial.records.size() == 7);
  for (const auto& row : logSerial.records) puts(row.c_str());
  puts(
      "PASS exact production serial line dispatch is explicit, Home-only, rejects arbitrary trailing URL, emits "
      "bounded redacted JSON");
  puts(
      "PASS private fixed anonymous diagnostic admission; generic remains closed; badcert zero writes; partial SSL "
      "writes counted; DNS/timeout distinct; redirect denied");
}
