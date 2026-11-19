#include "OpdsServerStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

#include <algorithm>
#include <cstring>
#include <utility>

#include "CrossPointSettings.h"

namespace {
constexpr char FILENAME_FORMAT_AUTHOR_TITLE[] = "author_title";
constexpr char FILENAME_FORMAT_TITLE_AUTHOR[] = "title_author";
}  // namespace

const char* opdsFilenameFormatToJson(const OpdsFilenameFormat format) {
  switch (format) {
    case OpdsFilenameFormat::TitleOnly:
      return "title";
    case OpdsFilenameFormat::TitleAuthor:
      return FILENAME_FORMAT_TITLE_AUTHOR;
    case OpdsFilenameFormat::AuthorTitle:
    default:
      return FILENAME_FORMAT_AUTHOR_TITLE;
  }
}

OpdsFilenameFormat opdsFilenameFormatFromJson(const char* value) {
  if (value && strcmp(value, FILENAME_FORMAT_TITLE_AUTHOR) == 0) {
    return OpdsFilenameFormat::TitleAuthor;
  }
  if (value && strcmp(value, "title") == 0) return OpdsFilenameFormat::TitleOnly;
  return OpdsFilenameFormat::AuthorTitle;
}

void OpdsServerStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["servers"].to<JsonArray>();
  for (const auto& server : servers) {
    JsonObject obj = arr.add<JsonObject>();
    obj["name"] = server.name;
    obj["url"] = server.url;
    obj["username"] = server.username;
    obj["password_obf"] = obfuscation::obfuscateToBase64(server.password);
    JsonArray headers = obj["headers"].to<JsonArray>();
    for (const auto& header : server.customHeaders) {
      JsonObject h = headers.add<JsonObject>();
      h["name"] = header.name;
      h["value_obf"] = obfuscation::obfuscateToBase64(header.value);
    }
    obj["filenameFormat"] = opdsFilenameFormatToJson(server.filenameFormat);
  }
}

bool OpdsServerStore::fromJson(JsonVariantConst doc) {
  // Tolerate a missing/invalid 'servers' key (treat as empty list); only a
  // JSON parse error is fatal. A null JsonArray iterates zero times.
  servers.clear();
  JsonArrayConst arr = doc["servers"].as<JsonArrayConst>();
  servers.reserve(std::min(arr.size(), MAX_SERVERS));
  bool needsResave = false;

  for (JsonObjectConst obj : arr) {
    if (servers.size() >= OpdsServerStore::MAX_SERVERS) break;
    OpdsServer server;
    server.name = obj["name"] | "";
    server.url = obj["url"] | "";
    server.username = obj["username"] | "";
    server.filenameFormat = opdsFilenameFormatFromJson(obj["filenameFormat"] | "");
    server.password = extractPassword(obj, needsResave);
    size_t headerIndex = 0;
    for (JsonObjectConst h : obj["headers"].as<JsonArrayConst>()) {
      if (headerIndex >= OpdsServer::MAX_CUSTOM_HEADERS) break;
      server.customHeaders[headerIndex].name = h["name"] | "";
      server.customHeaders[headerIndex++].value = obfuscation::deobfuscateFromBase64(h["value_obf"] | "");
    }
    servers.push_back(std::move(server));
  }

  if (needsResave) {
    LOG_DBG("OPS", "Resaving JSON with obfuscated passwords");
    requestResave();
  }

  return true;
}

bool OpdsServerStore::loadFromFile() {
  servers.clear();
  loaded_ = true;
  const bool hasStoreFile = Storage.exists(getFilePath()) ||
                            Storage.exists((std::string(getFilePath()) + ".bak").c_str()) ||
                            Storage.exists((std::string(getFilePath()) + ".tmp").c_str());
  if (PersistableStore<OpdsServerStore>::loadFromFile()) {
    return true;
  }
  if (hasStoreFile) {
    return false;
  }

  if (migrateFromSettings()) {
    LOG_DBG("OPS", "Migrated legacy OPDS settings");
    return true;
  }

  return false;
}

void OpdsServerStore::ensureLoaded() const {
  if (loaded_) return;
  const_cast<OpdsServerStore*>(this)->loadFromFile();
}

void OpdsServerStore::release() {
  std::vector<OpdsServer>().swap(servers);
  loaded_ = false;
}

bool OpdsServerStore::migrateFromSettings() {
  if (strlen(SETTINGS.opdsServerUrl) == 0) {
    return false;
  }

  OpdsServer server;
  server.name = "OPDS Server";
  server.url = SETTINGS.opdsServerUrl;
  server.username = SETTINGS.opdsUsername;
  server.password = SETTINGS.opdsPassword;
  servers.push_back(std::move(server));

  if (saveToFile()) {
    // Clear legacy fields so migration won't run again on next boot.
    SETTINGS.opdsServerUrl[0] = '\0';
    SETTINGS.opdsUsername[0] = '\0';
    SETTINGS.opdsPassword[0] = '\0';
    SETTINGS.saveToFile();
    LOG_DBG("OPS", "Migrated single-server OPDS config to opds.json");
    return true;
  }

  // Save failed; roll back in-memory state so callers don't see a partial migration.
  servers.clear();
  return false;
}

bool OpdsServerStore::addServer(const OpdsServer& server) {
  ensureLoaded();
  if (servers.size() >= MAX_SERVERS) {
    LOG_DBG("OPS", "Cannot add more servers, limit of %zu reached", MAX_SERVERS);
    return false;
  }

  servers.push_back(server);
  return saveToFile();
}

bool OpdsServerStore::updateServer(size_t index, const OpdsServer& server) {
  ensureLoaded();
  if (index >= servers.size()) {
    return false;
  }

  servers[index] = server;
  return saveToFile();
}

bool OpdsServerStore::removeServer(size_t index) {
  ensureLoaded();
  if (index >= servers.size()) {
    return false;
  }

  servers.erase(servers.begin() + static_cast<ptrdiff_t>(index));
  return saveToFile();
}

const OpdsServer* OpdsServerStore::getServer(size_t index) const {
  ensureLoaded();
  if (index >= servers.size()) {
    return nullptr;
  }
  return &servers[index];
}

std::vector<HttpDownloader::Header> OpdsServer::activeCustomHeaders() const {
  std::vector<HttpDownloader::Header> active;
  active.reserve(MAX_CUSTOM_HEADERS);
  for (const auto& header : customHeaders) {
    if (!header.name.empty()) active.emplace_back(header.name, header.value);
  }
  return active;
}
