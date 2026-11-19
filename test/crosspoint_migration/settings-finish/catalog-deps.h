#pragma once
#include <array>
#include <string>
// Storage callbacks are dependency doubles; catalog fields/factories use the
// real CrossPointSettings and SettingInfo definitions extracted by catalog.py.
enum class DocumentMatchMethod { Filename, Binary };
enum class KOReaderSyncBehavior { Ask, Smart };
enum class KOReaderServerType { CrossPoint, Kosync, Other };
struct Header {
  std::string name, value;
};
struct CredentialDouble {
  std::string user, password, url;
  std::array<Header, 2> headers;
  std::string getUsername() const { return user; }
  std::string getPassword() const { return password; }
  std::string getServerUrl() const { return url; }
  void setServerUrl(const std::string& s) { url = s; }
  void setCredentials(const std::string& u, const std::string& p) {
    user = u;
    password = p;
  }
  auto getCustomHeaders() const { return headers; }
  void setCustomHeader(int i, const std::string& n, const std::string& v) { headers[i] = {n, v}; }
  DocumentMatchMethod getMatchMethod() const { return DocumentMatchMethod::Filename; }
  void setMatchMethod(DocumentMatchMethod) {}
  KOReaderServerType getServerType() const { return KOReaderServerType::CrossPoint; }
  void setServerType(KOReaderServerType) {}
  KOReaderSyncBehavior getSyncBehavior() const { return KOReaderSyncBehavior::Ask; }
  void setSyncBehavior(KOReaderSyncBehavior) {}
  bool getSendMetadata() const { return false; }
  void setSendMetadata(bool) {}
  bool getSyncClippings() const { return true; }
  void setSyncClippings(bool) {}
  bool saveToFile() { return true; }
};
inline CredentialDouble KOREADER_STORE;
namespace FrontlightSchedule {
inline constexpr unsigned kUnsetTimeOfDay = 65535;
}
struct DictionaryEntry {
  std::string name, basePath;
};
namespace Dictionary {
inline std::string path;
inline std::string readDictPath() { return path; }
inline void saveGlobalDictPath(const char* p) { path = p; }
}  // namespace Dictionary
