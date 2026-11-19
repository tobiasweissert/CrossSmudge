#include <ArduinoJson.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
struct String : std::string {
  using std::string::string;
  String(std::string s) : std::string(std::move(s)) {}
  String(int n) : std::string(std::to_string(n)) {}
  bool isEmpty() const { return empty(); }
};
inline String operator+(const String& a, const String& b) { return String(std::string(a) + std::string(b)); }
inline String operator+(const String& a, const char* b) { return String(std::string(a) + b); }
inline DeserializationError deserializeJson(JsonDocument& d, const String& s) { return deserializeJson(d, s.c_str()); }
struct CrossPointSettings {
  uint8_t raw = 5, ordinary = 0, toggle = 0, small = 0;
  uint16_t minutes = 1380;
  char secret[32] = "private-token";
  bool saveToFile() { return true; }
};
inline CrossPointSettings SETTINGS;
enum class StrId { STR_NONE_OPT, STR_STATE_OFF, STR_STATE_ON };
struct {
  const char* get(StrId) const { return "label"; }
} I18N;
// SETTING_INFO
struct DictionaryEntry {};
struct {
  const int& registry() const {
    static int n = 0;
    return n;
  }
} sdFontSystem;
struct DictionaryRegistry {
  static void discover(std::vector<DictionaryEntry>&) {}
};
std::vector<SettingInfo> settings;
const auto& getSettingsList(const int*, const std::vector<DictionaryEntry>*) { return settings; }
template <class F, class G>
void forEachSetting(const int*, const std::vector<DictionaryEntry>*, F wants, G visit) {
  for (const auto& s : settings)
    if (wants(s.key)) visit(s);
}
bool listContainsKey(const String& keys, const char* key) { return keys == key; }
struct Server {
  String body, keys, output;
  int status = 0;
  bool hasArg(const char* p) { return std::string(p) == "plain"; }
  String arg(const char* p) { return std::string(p) == "plain" ? body : keys; }
  void setContentLength(int) {}
  void send(int n, const char*, const String& s) {
    status = n;
    output = s;
  }
};
struct ChunkedReply {
  Server& s;
  explicit ChunkedReply(Server& s) : s(s) {}
  size_t write(uint8_t c) {
    s.output += char(c);
    return 1;
  }
  size_t write(const uint8_t* p, size_t n) {
    s.output.append(reinterpret_cast<const char*>(p), n);
    return n;
  }
  void write(const char* p) { s.output += p; }
  void end() {}
};
constexpr int CONTENT_LENGTH_UNKNOWN = -1;
void yield() {}
void resetTaskWatchdogIfSubscribed() {}
#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
struct CrossPointWebServer {
  std::shared_ptr<Server> server = std::make_shared<Server>();
  void handleGetSettings() const;
  void handlePostSettings();
};
// WEB_METHODS
int main() {
  auto raw = SettingInfo::Enum(StrId::STR_NONE_OPT, &CrossPointSettings::raw,
                               {StrId::STR_NONE_OPT, StrId::STR_NONE_OPT, StrId::STR_NONE_OPT}, "raw");
  raw.withEnumRawValues({0, 5, 70});
  auto ord = SettingInfo::Enum(StrId::STR_NONE_OPT, &CrossPointSettings::ordinary,
                               {StrId::STR_STATE_OFF, StrId::STR_STATE_ON}, "ordinary");
  auto minutes = SettingInfo::Value16(StrId::STR_NONE_OPT, &CrossPointSettings::minutes, {0, 1439, 1}, "minutes");
  auto small = SettingInfo::Value(StrId::STR_NONE_OPT, &CrossPointSettings::small, {0, 300, 5}, "small");
  auto secret = SettingInfo::String(StrId::STR_NONE_OPT, SETTINGS.secret, sizeof(SETTINGS.secret), "secret");
  secret.withObfuscated();
  auto hidden = SettingInfo::Toggle(StrId::STR_NONE_OPT, &CrossPointSettings::toggle, "hidden");
  hidden.webHidden = true;
  uint8_t dynamic = 64;
  auto dyn = SettingInfo::DynamicEnum(
      StrId::STR_NONE_OPT, {StrId::STR_NONE_OPT, StrId::STR_NONE_OPT}, [&]() { return dynamic; },
      [&](uint8_t v) { dynamic = v; }, "dynamic");
  dyn.withEnumRawValues({33, 64});
  settings = {raw, ord, minutes, small, secret, hidden, dyn};
  CrossPointWebServer web;
  web.handleGetSettings();
  JsonDocument result;
  assert(!deserializeJson(result, web.server->output));
  assert(result.size() == 6);
  assert(result[0]["value"].as<int>() == 1);
  assert(result[2]["value"].as<int>() == 1380);
  assert(result[4]["value"].isNull());
  assert(result[4]["hasPassword"] == true);
  assert(web.server->output.find("private-token") == std::string::npos);
  assert(result[5]["value"] == 1);
  auto post = [&](const char* p) {
    web.server->body = p;
    web.handlePostSettings();
    assert(web.server->status == 200);
  };
  post("{\"raw\":2,\"minutes\":1439,\"ordinary\":true,\"dynamic\":0,\"hidden\":1,\"secret\":null}");
  assert(SETTINGS.raw == 70 && SETTINGS.minutes == 1439 && SETTINGS.ordinary == 1 && dynamic == 33 &&
         SETTINGS.toggle == 0 && std::string(SETTINGS.secret) == "private-token");
  for (auto invalid : {"-1", "3", "70", "1.5", "\"1\"", "null", "{}", "[]"}) {
    std::string p = "{\"raw\":" + std::string(invalid) + "}";
    post(p.c_str());
    assert(SETTINGS.raw == 70);
  }
  post("{\"minutes\":65536,\"small\":260}");
  assert(SETTINGS.minutes == 1439 && SETTINGS.small == 0);
  post("{\"minutes\":12.5,\"small\":7}");
  assert(SETTINGS.minutes == 1439 && SETTINGS.small == 0);
  post("{\"minutes\":0,\"small\":250,\"secret\":\"\"}");
  assert(SETTINGS.minutes == 0 && SETTINGS.small == 250 && SETTINGS.secret[0] == 0);
  SETTINGS.raw = 4;
  web.handleGetSettings();
  result.clear();
  assert(!deserializeJson(result, web.server->output));
  assert(result[0]["value"].isNull());
  settings[0].enumRawValues = {0};
  web.handleGetSettings();
  result.clear();
  assert(!deserializeJson(result, web.server->output));
  assert(result.size() == 5);
  std::cout << "PASS actual web GET/POST: sparse raw enums, exact display membership, dynamic enums, value16 bounds, "
               "masked-null credentials, hidden fields\n";
}
