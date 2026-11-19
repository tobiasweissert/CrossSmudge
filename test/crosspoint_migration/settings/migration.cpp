#include <HalStorage.h>
#include <PersistableStore.h>

#include <cassert>
#include <cstdio>
#include <cstring>
constexpr char SETTINGS_FILE_JSON[] = "/.crosspoint/crosssmudge-settings.json";
constexpr char LEGACY_SETTINGS_FILE_JSON[] = "/.crosspoint/settings.json";
constexpr char SETTINGS_FILE_BIN[] = "/.crosspoint/settings.bin";
constexpr char SETTINGS_FILE_BAK[] = "/.crosspoint/settings.bin.bak";
class CrossPointSettings : public PersistableStoreBase {
 public:
  std::mutex _mutex;
  bool imported = false;
  std::string value;
  bool fromJson(JsonVariantConst d, bool importing) {
    imported = importing;
    value = d["value"] | "";
    return true;
  }
  bool saveToFile() const {
    JsonDocument d;
    d["value"] = value;
    return writeDocToFileAtomically(SETTINGS_FILE_JSON, d);
  }
  bool loadFromFile();
  bool migrateLanguageBinaryFile() { return false; }
  bool loadFromBinaryFile() { return false; }
};
bool restoreLegacyRtcDateSyncState(CrossPointSettings&) { return false; }
// PRODUCTION_LOADER
void seed(const char* p, const char* s) { Storage.files[p] = String(s); }
void check(bool condition, const char* name) {
  if (!condition) {
    fprintf(stderr, "FAIL: %s\n", name);
    std::exit(1);
  }
  puts(name);
}
int main() {
  Storage.reset();
  seed(SETTINGS_FILE_JSON, "{broken");
  seed(LEGACY_SETTINGS_FILE_JSON, R"({"value":"foreign"})");
  CrossPointSettings s;
  check(!s.loadFromFile() && s.value.empty() && Storage.writes == 0, "corrupt owner must not import foreign settings");
  Storage.reset();
  seed(SETTINGS_FILE_JSON, R"({"value":"owner UTF-8: Grüße 日本語"})");
  seed(LEGACY_SETTINGS_FILE_JSON, R"({"value":"foreign"})");
  check(s.loadFromFile() && !s.imported && s.value == "owner UTF-8: Grüße 日本語",
        "owner namespace wins and UTF8 survives");
  Storage.reset();
  seed(LEGACY_SETTINGS_FILE_JSON, R"({"value":"upstream"})");
  check(s.loadFromFile() && s.imported && s.value == "upstream" && Storage.exists(LEGACY_SETTINGS_FILE_JSON),
        "explicit upstream import retains original");
  Storage.reset();
  seed((std::string(SETTINGS_FILE_JSON) + ".bak").c_str(), R"({"value":"recovered"})");
  seed(LEGACY_SETTINGS_FILE_JSON, R"({"value":"foreign"})");
  check(s.loadFromFile() && !s.imported && s.value == "recovered", "owner recovery precedes foreign fallback");
  Storage.reset();
  JsonDocument old;
  old["value"] = "old";
  JsonDocument next;
  next["value"] = "next";
  check(PersistableStoreBase::writeDocToFileAtomically(SETTINGS_FILE_JSON, old), "initial checked save");
  Storage.failWrite = std::string(SETTINGS_FILE_JSON) + ".tmp";
  check(!PersistableStoreBase::writeDocToFileAtomically(SETTINGS_FILE_JSON, next),
        "temporary write failure propagates");
  JsonDocument loaded;
  check(PersistableStoreBase::readDocFromFile(SETTINGS_FILE_JSON, loaded) && loaded["value"] == "old",
        "failed save preserves old document");
  Storage.failWrite.clear();
  Storage.failRename = std::string(SETTINGS_FILE_JSON) + ".tmp";
  Storage.ambiguousRename = true;
  check(!PersistableStoreBase::writeDocToFileAtomically(SETTINGS_FILE_JSON, next),
        "ambiguous rename is not acknowledged");
  const auto retained = Storage.files;
  check(Storage.exists((std::string(SETTINGS_FILE_JSON) + ".bak").c_str()) &&
            Storage.exists((std::string(SETTINGS_FILE_JSON) + ".tmp").c_str()),
        "ambiguous failure retains both recovery roots");
  check(!PersistableStoreBase::writeDocToFileAtomically(SETTINGS_FILE_JSON, next) && Storage.files == retained,
        "uncertain publication refuses destructive retry");
  puts("PASS migration/persistence controls");
}
