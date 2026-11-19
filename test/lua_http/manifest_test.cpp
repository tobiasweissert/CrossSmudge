#include <cassert>

#include "HttpManifest.h"
using namespace ink::http;
int main() {
  Domains domains;
  auto check = [&](const char* json, bool expected) {
    JsonDocument doc;
    assert(!deserializeJson(doc, json));
    assert(readNetworkDeclaration(doc["network"], domains) == expected);
  };
  check(R"({"network":{"domains":["z.example.com","a.example.com"]},"unknown":true})", true);
  assert(domains.count == 2 && std::string_view(domains.names[0]) == "a.example.com");
  check(R"({"network":{"domains":["example.com"],"private":true}})", false);
  assert(domains.count == 0);
  check(R"({"network":{"domains":["example.com",false]}})", false);
  assert(domains.count == 0);
  check(R"({"network":{"domains":["example.com","example.com"]}})", false);
  check(R"({"network":{"domains":["example.com\u0000evil"]}})", false);
  check(R"({"name":"legacy"})", true);
  assert(domains.count == 0);
}
