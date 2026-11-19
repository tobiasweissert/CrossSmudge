#include "PersistableStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>
#include <cstring>
#include <limits>

bool PersistableStoreBase::writeDocToFile(const char* path, const JsonDocument& doc) {
  Storage.mkdir("/.crosspoint");
  String json;
  serializeJson(doc, json);
  if (!Storage.writeFile(path, json)) {
    LOG_ERR("PERSIST", "Failed to write %s", path);
    return false;
  }
  return true;
}

bool PersistableStoreBase::writeDocToFileAtomically(const char* path, const JsonDocument& doc) {
  Storage.mkdir("/.crosspoint");
  String json;
  serializeJson(doc, json);

  const std::string tempPath = std::string(path) + ".tmp";
  const std::string backupPath = std::string(path) + ".bak";
  // A prior unacknowledged operation may have left FAT aliases. Never truncate,
  // unlink or rotate those roots during a retry (including after reboot).
  if (Storage.exists(tempPath.c_str()) || Storage.exists(backupPath.c_str())) {
    LOG_ERR("PERSIST", "Retained recovery roots block write to %s", path);
    return false;
  }
  if (!Storage.writeFile(tempPath.c_str(), json) || Storage.readFile(tempPath.c_str()) != json) {
    LOG_ERR("PERSIST", "Temporary write/readback failed for %s", path);
    return false;
  }

  const bool hadOriginal = Storage.exists(path);
  if (hadOriginal) {
    if (!Storage.rename(path, backupPath.c_str())) {
      LOG_ERR("PERSIST", "Failed to back up %s", path);
      // Keep both names: a negative acknowledgement need not mean no mutation.
      return false;
    }
  }

  if (!Storage.rename(tempPath.c_str(), path)) {
    LOG_ERR("PERSIST", "Failed to replace %s", path);
    // Do not roll back or remove either name after an ambiguous rename.
    return false;
  }

  if (Storage.readFile(path) != json) {
    LOG_ERR("PERSIST", "Published readback failed for %s", path);
    return false;
  }
  if (hadOriginal && Storage.exists(backupPath.c_str()) && !Storage.remove(backupPath.c_str())) {
    LOG_ERR("PERSIST", "Failed to remove completed backup %s", backupPath.c_str());
  }
  return true;
}

bool PersistableStoreBase::readDocFromFile(const char* path, JsonDocument& doc) {
  const std::string backup = std::string(path) + ".bak";
  const std::string temporary = std::string(path) + ".tmp";
  auto read = [&](const char* candidate) {
    if (!Storage.exists(candidate)) return false;
    String json = Storage.readFile(candidate);
    if (json.isEmpty()) return false;
    doc.clear();
    return !deserializeJson(doc, json);
  };
  // Retained temp+backup means publication was not acknowledged. Prefer the
  // complete old snapshot, without renaming/unlinking possible FAT aliases.
  if (Storage.exists(temporary.c_str()) && Storage.exists(backup.c_str())) {
    return read(backup.c_str());
  }
  if (read(path)) return true;
  return read(backup.c_str());
}

std::string PersistableStoreBase::extractPassword(JsonVariantConst doc, bool& needsResave) {
  bool valid = false;
  return extractPassword(doc, needsResave, std::numeric_limits<size_t>::max(), valid);
}

std::string PersistableStoreBase::extractPassword(JsonVariantConst doc, bool& needsResave, const size_t maxLength,
                                                  bool& valid) {
  valid = true;
  bool ok = false;
  bool tooLong = false;
  std::string pass = obfuscation::deobfuscateFromBase64(doc["password_obf"] | "", maxLength, &ok, &tooLong);
  if (tooLong) {
    valid = false;
    return "";
  }
  if (!ok) {
    // Deobfuscation failed — fall back to legacy plaintext password.
    const char* legacyPassword = doc["password"] | "";
    const size_t legacyLength = strlen(legacyPassword);
    if (legacyLength > maxLength) {
      valid = false;
      return "";
    }
    pass.assign(legacyPassword, legacyLength);
    if (!pass.empty()) needsResave = true;
  }
  // A successfully decoded empty string is a legitimate value; preserve as-is.
  return pass;
}
