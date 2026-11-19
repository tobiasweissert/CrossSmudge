#pragma once
#include <ArduinoJson.h>
#include <HttpHeader.h>
#include <PersistableStore.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "network/HttpDownloader.h"
#include "util/OpdsFilename.h"

const char* opdsFilenameFormatToJson(OpdsFilenameFormat format);
OpdsFilenameFormat opdsFilenameFormatFromJson(const char* value);

struct OpdsServer {
  std::string name;
  std::string url;
  std::string username;
  std::string password;  // Plaintext in memory; obfuscated with hardware key on disk
  static constexpr size_t MAX_CUSTOM_HEADERS = 2;
  std::array<HttpHeader, MAX_CUSTOM_HEADERS> customHeaders;
  std::vector<HttpDownloader::Header> activeCustomHeaders() const;
  OpdsFilenameFormat filenameFormat = OpdsFilenameFormat::AuthorTitle;
};

/**
 * Singleton class for storing OPDS server configurations on the SD card.
 * Passwords are XOR-obfuscated with the device's unique hardware MAC address
 * and base64-encoded before writing to JSON.
 */
class OpdsServerStore : public PersistableStore<OpdsServerStore> {
 private:
  std::vector<OpdsServer> servers;
  bool loaded_ = false;

  static constexpr size_t MAX_SERVERS = 8;

  OpdsServerStore() = default;
  bool migrateFromSettings();

  friend class PersistableStore<OpdsServerStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/opds.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);
  bool loadFromFile();
  void ensureLoaded() const;
  void release();

  bool addServer(const OpdsServer& server);
  bool updateServer(size_t index, const OpdsServer& server);
  bool removeServer(size_t index);

  const std::vector<OpdsServer>& getServers() const {
    ensureLoaded();
    return servers;
  }
  const OpdsServer* getServer(size_t index) const;
  size_t getCount() const {
    ensureLoaded();
    return servers.size();
  }
  bool hasServers() const {
    ensureLoaded();
    return !servers.empty();
  }
};

#define OPDS_STORE OpdsServerStore::getInstance()
