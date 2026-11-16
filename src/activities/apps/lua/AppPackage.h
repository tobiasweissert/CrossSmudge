#pragma once

#include <ArduinoJson.h>
#include <HalStorage.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "Logging.h"
#include "Memory.h"

struct AppPackage {
  std::string id;           // Directory name, e.g. "counter"
  std::string name;         // Display name, e.g. "Tally Counter"
  std::string version;      // e.g. "1.0.0"
  std::string author;       // e.g. "Community"
  std::string description;  // e.g. "Simple e-ink tally counter"
  std::string entryScript;  // default "main.lua"
  std::string path;         // e.g. "/.crosssmudge/applications/counter"

  // 32x32 1-bit icon (32 * 32 / 8 = 128 bytes)
  bool hasIcon = false;
  uint8_t iconData[128] = {0};
  bool preventSleep = false;
  int minApi = 0;

  enum class BrowserResolveError : uint8_t { None, Missing, InvalidManifest, Incompatible, InvalidEntry, StorageIo, Allocation };

  // Same package precedence as the ordinary launcher, narrowed to the trusted browser
  // folder/id. An earlier installed-but-stale Browser blocks fallback to another copy.
  static bool resolveInstalledBrowser(AppPackage& output, BrowserResolveError& error, int supportedApi = 6) {
    struct Root {
      const char* parent;
      const char* name;
      const char* path;
    };
    static constexpr Root roots[] = {
        {"/.crosssmudge", "applications", "/.crosssmudge/applications"},
        {"/", "applications", "/applications"},
        {"/", "apps", "/apps"},
        {"/.smudge", "applications", "/.smudge/applications"},
        {"/.crosspoint", "applications", "/.crosspoint/applications"},
    };
    enum class Presence : uint8_t { Missing, File, Directory, IoError };
    constexpr size_t nameBytes = 768;  // Complete 255 UTF-16-unit FAT names need up to 765 UTF-8 bytes.
    constexpr size_t maxDirectoryEntries = 2048;
    size_t remainingEntries = 8192;
    // One bounded cold-launch buffer, reused across all scans. A full UTF-8 FAT
    // name does not belong on the activity stack; no per-entry heap allocation.
    auto entryName = makeUniqueNoThrow<char[]>(nameBytes);
    if (!entryName) {
      LOG_ERR("APP", "Browser directory-name allocation failed");
      error = BrowserResolveError::Allocation;
      return false;
    }
    const auto sameName = [](const char* a, const char* b) {
      for (size_t i = 0;; ++i) {
        unsigned char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return false;
        if (!x) return true;
      }
    };
    const auto findChild = [&](const char* parent, const char* wanted) {
      HalFile directory = Storage.open(parent);
      if (!directory || !directory.isDirectory()) {
        directory.close();
        return Presence::IoError;
      }
      // A final EOF probe admits an exactly-full directory, but not one extra
      // entry. Missing means healthy iteration and checked closes, never exists(false).
      for (size_t count = 0; count <= maxDirectoryEntries; ++count) {
        HalFile child = directory.openNextFile();
        if (!child) {
          const bool healthy = !directory.iterationFailed() && !directory.allocationFailed() &&
                               !child.allocationFailed();
          const bool childClosed = child.close();
          const bool directoryClosed = directory.close();
          return healthy && childClosed && directoryClosed ? Presence::Missing : Presence::IoError;
        }
        if (count == maxDirectoryEntries || !remainingEntries) {
          child.close();
          directory.close();
          return Presence::IoError;
        }
        --remainingEntries;
        const size_t length = child.getName(entryName.get(), nameBytes);
        const bool matches = length && length < nameBytes && sameName(entryName.get(), wanted);
        const Presence kind = child.isDirectory() ? Presence::Directory : Presence::File;
        const bool closed = child.close();
        if (!length || length >= nameBytes || !closed) {
          directory.close();
          return Presence::IoError;
        }
        if (matches) return directory.close() ? kind : Presence::IoError;
        yield();
      }
      directory.close();
      return Presence::IoError;
    };
    const auto storageFailure = [&]() {
      LOG_ERR("APP", "Installed Browser storage is unreadable or exceeds directory scan bounds");
      error = BrowserResolveError::StorageIo;
      return false;
    };
    error = BrowserResolveError::Missing;
    for (const Root& root : roots) {
      if (strcmp(root.parent, "/") != 0) {
        const Presence ancestor = findChild("/", root.parent + 1);
        if (ancestor == Presence::Missing) continue;
        if (ancestor != Presence::Directory) return storageFailure();
      }
      const Presence base = findChild(root.parent, root.name);
      if (base == Presence::Missing) continue;
      if (base != Presence::Directory) return storageFailure();
      const Presence candidate = findChild(root.path, "browser");
      if (candidate == Presence::Missing) continue;
      if (candidate == Presence::IoError) return storageFailure();
      if (candidate != Presence::Directory) {
        LOG_ERR("APP", "Installed Browser path is not a directory");
        error = BrowserResolveError::InvalidManifest;
        return false;
      }
      const std::string folder = std::string(root.path) + "/browser";
      const std::string manifestPath = folder + "/manifest.json";
      const Presence manifestPresence = findChild(folder.c_str(), "manifest.json");
      if (manifestPresence == Presence::IoError) return storageFailure();
      if (manifestPresence != Presence::File) {
        LOG_ERR("APP", "Installed Browser has no regular manifest");
        error = BrowserResolveError::InvalidManifest;
        return false;
      }
      HalFile manifest = Storage.open(manifestPath.c_str());
      if (!manifest || manifest.isDirectory()) {
        manifest.close();
        return storageFailure();
      }
      const uint64_t size = manifest.fileSize64();
      if (!size || size > 4096) {
        const bool closed = manifest.close();
        LOG_ERR("APP", "Installed Browser manifest size rejected");
        error = closed ? BrowserResolveError::InvalidManifest : BrowserResolveError::StorageIo;
        return false;
      }
      // One bounded cold launch allocation; a 4 KiB manifest does not belong on the activity stack.
      auto bytes = makeUniqueNoThrow<char[]>(static_cast<size_t>(size) + 1);
      if (!bytes) {
        const bool closed = manifest.close();
        LOG_ERR("APP", "Browser manifest allocation failed");
        error = closed ? BrowserResolveError::Allocation : BrowserResolveError::StorageIo;
        return false;
      }
      const bool read = static_cast<uint64_t>(manifest.read(bytes.get(), static_cast<size_t>(size))) == size;
      const bool manifestClosed = manifest.close();
      if (!read || !manifestClosed) {
        LOG_ERR("APP", "Browser manifest read or close failed");
        error = BrowserResolveError::StorageIo;
        return false;
      }
      bytes[static_cast<size_t>(size)] = 0;
      JsonDocument declaration;
      if (deserializeJson(declaration, bytes.get(), static_cast<size_t>(size)) ||
          !declaration["id"].is<const char*>() || declaration["id"].as<JsonString>().size() != 7 ||
          strcmp(declaration["id"].as<const char*>(), "browser") != 0) {
        LOG_ERR("APP", "Installed Browser manifest parse or id rejected");
        error = BrowserResolveError::InvalidManifest;
        return false;
      }
      if (!declaration["browser"].is<bool>() || !declaration["browser"].as<bool>() ||
          !declaration["requires_psram"].is<bool>() || !declaration["requires_psram"].as<bool>() ||
          !declaration["min_api"].is<int>() || declaration["min_api"].as<int>() < 6 ||
          declaration["min_api"].as<int>() > supportedApi) {
        LOG_ERR("APP", "Installed Browser capability or API rejected");
        error = BrowserResolveError::Incompatible;
        return false;
      }
#if !defined(BOARD_HAS_PSRAM) && !defined(SIMULATOR)
      LOG_ERR("APP", "Browser launch requires PSRAM");
      error = BrowserResolveError::Incompatible;
      return false;
#endif
      if (!declaration["entry"].isNull() && !declaration["entry"].is<const char*>()) {
        LOG_ERR("APP", "Installed Browser entry type rejected");
        error = BrowserResolveError::InvalidEntry;
        return false;
      }
      const bool declaredEntry = declaration["entry"].is<const char*>();
      const char* entry = declaredEntry ? declaration["entry"].as<const char*>() : "main.lua";
      const size_t length = strlen(entry);
      if (!length || length > 120 || (declaredEntry && declaration["entry"].as<JsonString>().size() != length) ||
          entry[0] == '.' || strchr(entry, '/') || strchr(entry, '\\') || length < 4 ||
          strcmp(entry + length - 4, ".lua") != 0) {
        LOG_ERR("APP", "Installed Browser entry path rejected");
        error = BrowserResolveError::InvalidEntry;
        return false;
      }
      for (size_t i = 0; i < length; ++i) {
        const unsigned char c = entry[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-')) {
          LOG_ERR("APP", "Installed Browser entry path rejected");
          error = BrowserResolveError::InvalidEntry;
          return false;
        }
      }
      if (findChild(folder.c_str(), entry) != Presence::File) return storageFailure();
      HalFile script = Storage.open((folder + "/" + entry).c_str());
      if (!script || script.isDirectory()) {
        script.close();
        return storageFailure();
      }
      if (!script.close()) return storageFailure();
      output.id = "browser";
      output.name = "Browser";
      output.entryScript = entry;
      output.path = folder;
      output.minApi = declaration["min_api"].as<int>();
      if (declaration["name"].is<const char*>()) output.name = declaration["name"].as<const char*>();
      if (declaration["version"].is<const char*>()) output.version = declaration["version"].as<const char*>();
      error = BrowserResolveError::None;
      return true;
    }
    LOG_INF("APP", "No installed Browser package found");
    return false;
  }

  static std::vector<AppPackage> scanApplications() {
    std::vector<AppPackage> apps;
    const char* searchDirs[] = {"/.crosssmudge/applications", "/applications", "/apps", "/.smudge/applications",
                                "/.crosspoint/applications"};

    for (const char* baseDir : searchDirs) {
      if (!Storage.exists(baseDir)) {
        continue;
      }

      HalFile dir = Storage.open(baseDir);
      if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        continue;
      }

      char folderName[128];
      for (HalFile entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
        folderName[0] = '\0';
        entry.getName(folderName, sizeof(folderName));
        bool isDir = entry.isDirectory();
        // Close entry handle immediately so SdFat allows child file operations
        entry.close();

        // Strip trailing slashes or backslashes
        size_t len = strlen(folderName);
        while (len > 0 && (folderName[len - 1] == '/' || folderName[len - 1] == '\\')) {
          folderName[--len] = '\0';
        }

        if (folderName[0] == '\0' || strcmp(folderName, ".") == 0 || strcmp(folderName, "..") == 0) {
          continue;
        }

        // Check if already found in an earlier search dir
        bool alreadyFound = false;
        for (const auto& existing : apps) {
          if (existing.id == folderName) {
            alreadyFound = true;
            break;
          }
        }
        if (alreadyFound) continue;

        std::string appDir = std::string(baseDir) + "/" + folderName;
        std::string manifestPath = appDir + "/manifest.json";
        std::string mainLuaPath = appDir + "/main.lua";

        bool hasManifest = Storage.exists(manifestPath.c_str());
        bool hasMainLua = Storage.exists(mainLuaPath.c_str());

        // An application folder must contain either manifest.json or main.lua
        if (!hasManifest && !hasMainLua) {
          continue;
        }

        AppPackage pkg;
        pkg.id = folderName;
        pkg.name = folderName;
        pkg.version = "1.0.0";
        pkg.author = "Community";
        pkg.description = "";
        pkg.entryScript = "main.lua";
        pkg.path = appDir;

        if (hasManifest) {
          String jsonStr = Storage.readFile(manifestPath.c_str());
          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, jsonStr);
          if (!err) {
            if (doc["id"].is<const char*>()) pkg.id = doc["id"].as<const char*>();
            if (doc["name"].is<const char*>()) pkg.name = doc["name"].as<const char*>();
            if (doc["version"].is<const char*>()) pkg.version = doc["version"].as<const char*>();
            if (doc["author"].is<const char*>()) pkg.author = doc["author"].as<const char*>();
            if (doc["description"].is<const char*>()) pkg.description = doc["description"].as<const char*>();
            if (doc["entry"].is<const char*>()) pkg.entryScript = doc["entry"].as<const char*>();
            if (doc["prevent_sleep"].is<bool>()) pkg.preventSleep = doc["prevent_sleep"].as<bool>();
            if (doc["min_api"].is<int>()) pkg.minApi = doc["min_api"].as<int>();
          } else {
            LOG_ERR("APP", "Failed to parse manifest for %s: %s", folderName, err.c_str());
          }
        }

        // Check for 32x32 raw icon
        std::string iconPath = appDir + "/icon.raw";
        if (Storage.exists(iconPath.c_str())) {
          HalFile iconFile;
          if (Storage.openFileForRead("LUA", iconPath.c_str(), iconFile)) {
            if (iconFile.read(pkg.iconData, sizeof(pkg.iconData)) == sizeof(pkg.iconData)) {
              pkg.hasIcon = true;
            }
            iconFile.close();
          }
        }

        LOG_INF("APP", "Discovered package: %s (%s) at %s", pkg.name.c_str(), pkg.id.c_str(), appDir.c_str());
        apps.push_back(std::move(pkg));
        yield();
      }
      dir.close();
    }
    return apps;
  }
};
