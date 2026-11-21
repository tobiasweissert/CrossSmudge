// Platform seams only. Activity and parser method bodies are extracted verbatim.
#include <ArduinoJson.h>
#include <JsonListParser.h>

#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string_view>

#include "activities/plugins/PluginCatalogPaging.h"
#include "util/FirmwareIdentity.h"
#undef LOG_ERR
#define LOG_ERR(...) ((void)0)
inline DeserializationError deserializeJson(JsonDocument& doc, String& s) { return deserializeJson(doc, s.s); }
using pluginhttp::resolvePath;
using pluginhttp::variantToString;
namespace protectedpaths {
inline bool isPluginPath(const std::string&) { return true; }
}  // namespace protectedpaths
namespace UrlUtils {
inline void resolveServerPath(std::string& s, const std::string&) { assert(s.find("https://") == 0); }
}  // namespace UrlUtils
constexpr size_t MAX_MANIFEST_SIZE = 32768, MAX_BROWSE_LIST_INDEX_RESPONSE = 16384, MAX_BROWSE_LISTS = 64;
constexpr size_t MAX_BROWSE_RESPONSE = 1024 * 1024;
constexpr int MAX_PAGE_SIZE = 16;
constexpr const char* BROWSE_TMP_PATH = "/book.epub";
enum class StrId { STR_FETCH_FEED_FAILED, STR_PLUGIN_MANIFEST_INVALID, STR_PARSE_FEED_FAILED };
struct ActivityResult {
  bool isCancelled;
};
struct ConfirmationActivity {
  std::string message;
  ConfirmationActivity(int, int, const std::string&, const std::string& m, const std::string&, const std::string&)
      : message(m) {}
};
template <class T, class... A>
std::unique_ptr<T> makeUniqueNoThrow(A&&... args) {
  return std::make_unique<T>(std::forward<A>(args)...);
}
std::string noticeMarkerPath(const std::string&, const std::string&) {
  return {};
}  // Marker persistence not under test.
bool catalogItemHasUpdate(const std::string&, const std::string&) {
  return false;
}  // Empty installed inventory fixture.
template <class Parser>
bool streamBrowseFile(Parser* parser) {
  struct Input {
    const std::string& body;
    size_t offset = 0;
  } input{fs.data};
  return parser && parser->parse(
                       [](void* p, char* out, size_t n) {
                         auto& i = *static_cast<Input*>(p);
                         n = std::min(n, i.body.size() - i.offset);
                         memcpy(out, i.body.data() + i.offset, n);
                         i.offset += n;
                         return static_cast<int>(n);
                       },
                       &input);
}
// @PRODUCTION_POLICY@
class PluginCatalogActivity {
 public:
  // @PRODUCTION_TYPES@
  Manifest manifest;
  std::vector<Item> items;
  std::string manifestPath = "/.crosspoint/plugins/pluginhub/device.json", browseCurrentUrl;
  bool listIndexLoaded = false, hasMore = false, searchActive = false;
  int currentList = -1, page = 1, renderer = 0, mappedInput = 0;
  enum class State { NO_TOKEN, ERROR, BROWSING, LIST_PICKER, LOADING } state = State::LOADING;
  std::unique_ptr<freeink::SecureHttpClient> session;
  struct Nav {
    void reset() {}
  } nav;
  std::function<void(const ActivityResult&)> noticeCompletion;
  std::string shownNotice;
  void loadConfig() { assert(manifest.configFile.empty()); }
  bool loadToken() {
    assert(manifest.tokenFile.empty());
    return true;
  }
  bool refreshCredentialToken() {
    assert(false);
    return false;
  }
  void beginAuth() { assert(false); }
  void fail(StrId) { state = State::ERROR; }
  void requestUpdate() {}
  void releaseRows() {}
  void exitCatalog() { state = State::LIST_PICKER; }
  void beginLoading() { state = State::LOADING; }
  void computeInstallStatus() {}
  std::string substituted(const std::string& s, const Item*) const {
    assert(s.find('{') == std::string::npos);
    return s;
  }
  pluginhttp::Headers substitutedHeaders(const pluginhttp::Headers& h, const Item*) const {
    assert(h.empty());
    return h;
  }
  void startActivityForResult(std::unique_ptr<ConfirmationActivity> p, std::function<void(const ActivityResult&)> cb) {
    shownNotice = p->message;
    noticeCompletion = std::move(cb);
  }
  bool loadManifest();
  bool loadBrowseListIndex(int&);
  bool wantsListPicker() const;
  void openList(int);
  void startBrowse();
  bool fetchBrowseResponse();
  void fetchPage(int);
  void retryBrowse();
  const std::string& activeBrowseUrl() const;
  const std::string& activeBrowseBody() const;
  bool parseUpdateFirstBrowseResponse(size_t);
  bool parseBrowseResponse() {
    assert(manifest.tracksInstalls());
    return parseUpdateFirstBrowseResponse(size_t(page - 1) * manifest.pageSize);
  }
};
// @PRODUCTION_METHODS@
static std::string readFixture(const char* p) {
  std::ifstream f(p);
  assert(f.good());
  return {std::istreambuf_iterator<char>(f), {}};
}
static std::string http(const std::string& body) {
  return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}
int main(int argc, char** argv) {
  assert(argc == 4);
  Storage.manifestFixture = readFixture(argv[1]);
  const auto index = readFixture(argv[2]), catalog = readFixture(argv[3]);
  struct Inventory {
    size_t raw = 0, eligible = 0;
  } inventory;
  const std::string paths[] = {"title",   "compatibility.firmware_family", "name", "compatibility.requires",
                               "version", "compatibility.plugin_api_min"};
  const std::string* fields[] = {&paths[0], &paths[1], &paths[2], &paths[3], &paths[4], &paths[5]};
  fs.data = catalog;
  JsonListParser countParser(
      "plugins", fields, "",
      [](void* p, JsonListParser::Row& row) {
        auto& count = *static_cast<Inventory*>(p);
        ++count.raw;
        if (!row.field[JsonListParser::F_TITLE].empty() &&
            catalogEntryCompatible(row.field[JsonListParser::F_AUTHOR], row.field[JsonListParser::F_BASE],
                                   row.field[JsonListParser::F_URL]))
          ++count.eligible;
      },
      &inventory);
  assert(streamBrowseFile(&countParser));
  assert(inventory.raw == 38 && inventory.eligible == 36);
  puts("PASS real SDK parser captured raw=38 eligible=36, unmodified advertised firmware capabilities");
  PluginCatalogActivity activity;
  assert(activity.loadManifest());
  assert(activity.manifest.pageSize == 16 && activity.manifest.browseLists.empty());
  replies = {http(index), http(catalog), http(catalog), http(catalog)};
  activity.startBrowse();
#if !defined(CROSSCUB_VERIFIED_PLUGIN_TRANSPORT) || CROSSCUB_VERIFIED_PLUGIN_TRANSPORT != 1
  assert(activity.state == PluginCatalogActivity::State::ERROR);
  assert(activity.shownNotice.empty() && !activity.listIndexLoaded);
  activity.retryBrowse();
  assert(dnsCalls == 0 && created == 0 && halClock.syncCalls == 0);
  puts(
      "PASS actual caller unqualified index fails before notice/NTP/DNS/TLS, retry stays closed; cannot infer "
      "installed Mac binary");
#else
  assert(activity.shownNotice == "Third-party plugins. Not vetted by Dev Team.");
  assert(activity.listIndexLoaded && activity.currentList == -1 && created == 1);
  // Accept the real notice callback; transport failure is injected after index success.
  handshake = -1;
  auto callback = std::move(activity.noticeCompletion);
  callback({false});
  assert(activity.currentList == 0 && activity.state == PluginCatalogActivity::State::ERROR);
  assert(activity.page == 1 && created == 2 && writes[1].empty());
  // Retry the actual caller; no index refetch or second notice.
  handshake = 1;
  replies.push_back(http(catalog));
  activity.retryBrowse();
  assert(activity.state == PluginCatalogActivity::State::BROWSING && activity.items.size() == 16 && activity.hasMore);
  std::set<std::string> ids;
  for (int page = 1; page <= 3; ++page) {
    if (page > 1) {
      replies.push_back(http(catalog));
      activity.fetchPage(page);
    }
    assert(activity.items.size() == (page == 3 ? 4 : 16));
    assert(activity.hasMore == (page < 3));
    for (const auto& row : activity.items) assert(ids.insert(row.id).second);
  }
  assert(ids.size() == 36);
  puts(
      "PASS actual caller index->notice acceptance->catalog failure->retry->SDK production ranking pages 16/16/4 "
      "unique eligible=36; transport doubles, not installed/device proof");
#endif
  assert(created == destroyed);
}
