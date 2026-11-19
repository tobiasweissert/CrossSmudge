#include <fcntl.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
#define LOG_INF(...) ((void)0)
#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
struct String : std::string {
  using std::string::string;
  String(const std::string& s) : std::string(s) {}
  bool isEmpty() const { return empty(); }
  int lastIndexOf(char c) const {
    auto p = rfind(c);
    return p == npos ? -1 : int(p);
  }
  String substring(int a, int b) const { return substr(a, b - a); }
  String substring(int a) const { return substr(a); }
};
struct Node {
  bool dir = false;
  std::string bytes;
};
std::map<std::string, Node> disk;
int mutations = 0, renameCalls = 0, failRename = 0;
bool mutateFailure = false, removeOK = true, closeOK = true, writeOK = true, readOK = true, syncOK = true;
struct HalFile {
  std::string path;
  size_t pos = 0, iter = 0;
  bool valid = false;
  HalFile() = default;
  HalFile(std::string p) : path(p), valid(disk.count(p)) {}
  operator bool() const { return valid; }
  bool isDirectory() const { return valid && disk.at(path).dir; }
  bool sync() { return syncOK; }
  bool close() {
    valid = false;
    return closeOK;
  }
  size_t size() const { return disk.at(path).bytes.size(); }
  int available() const { return valid && pos < disk.at(path).bytes.size(); }
  int read(void* b, size_t n) {
    if (!readOK) return 0;
    auto& s = disk.at(path).bytes;
    n = std::min(n, s.size() - pos);
    memcpy(b, s.data() + pos, n);
    pos += n;
    return int(n);
  }
  size_t write(const uint8_t* b, size_t n) {
    if (!writeOK) return 0;
    disk[path].bytes.append((const char*)b, n);
    return n;
  }
  size_t getName(char* b, size_t n) {
    auto s = path.substr(path.rfind('/') + 1);
    if (s.size() + 1 > n) return 0;
    strcpy(b, s.c_str());
    return s.size();
  }
  HalFile openNextFile() {
    std::vector<std::string> names;
    for (auto& [p, node] : disk)
      if (p.rfind(path + "/", 0) == 0 && p.find('/', path.size() + 1) == std::string::npos) names.push_back(p);
    return iter < names.size() ? HalFile(names[iter++]) : HalFile();
  }
};
struct StorageDouble {
  bool exists(const char* p) { return disk.count(p); }
  HalFile open(const char* p) { return HalFile(p); }
  HalFile open(const char* p, int flags) {
    if ((flags & O_EXCL) && disk.count(p)) return {};
    if (flags & O_CREAT) {
      ++mutations;
      disk[p] = {false, {}};
    }
    return HalFile(p);
  }
  bool remove(const char* p) {
    ++mutations;
    return disk.erase(p) > 0;
  }
  bool mkdir(const char* p) {
    ++mutations;
    disk[p] = {true, {}};
    return true;
  }
  bool rename(const char* a, const char* b) {
    ++renameCalls;
    ++mutations;
    bool fail = renameCalls == failRename;
    if (!fail || mutateFailure) {
      auto n = disk.at(a);
      disk.erase(a);
      disk[b] = n;
    }
    return !fail;
  }
  bool removeDir(const char* p) {
    ++mutations;
    if (!removeOK) return false;
    std::string prefix = std::string(p) + "/";
    for (auto i = disk.begin(); i != disk.end();)
      if (i->first == p || i->first.rfind(prefix, 0) == 0)
        i = disk.erase(i);
      else
        ++i;
    return true;
  }
  bool rmdir(const char* p) { return removeDir(p); }
  bool openFileForWrite(const char*, const String& p, HalFile& f) {
    ++mutations;
    disk[p] = {false, {}};
    f = HalFile(p);
    return true;
  }
} Storage;
namespace FsHelpers {
bool ext(const std::string& p, const char* e) {
  return p.size() >= strlen(e) && p.compare(p.size() - strlen(e), strlen(e), e) == 0;
}
bool hasEpubExtension(const std::string& p) { return ext(p, ".epub"); }
bool hasXtcExtension(const std::string& p) { return ext(p, ".xtc"); }
bool hasTxtExtension(const std::string& p) { return ext(p, ".txt"); }
}  // namespace FsHelpers
int dirty = 0;
namespace library {
bool isLibraryBookFile(const char* p) {
  return FsHelpers::hasEpubExtension(p) || FsHelpers::hasTxtExtension(p) || FsHelpers::hasXtcExtension(p);
}
void markLibraryIndexDirty() { ++dirty; }
}  // namespace library
struct Epub {
  std::string p;
  Epub(std::string s, const char*) : p(s) {}
  std::string getCachePath() { return "/cache/epub"; }
  bool clearCache() { return Storage.removeDir(getCachePath().c_str()); }
};
struct Txt : Epub {
  using Epub::Epub;
  std::string getCachePath() { return "/cache/txt"; }
  bool clearCache() { return Storage.removeDir(getCachePath().c_str()); }
};
struct Xtc : Txt {
  using Txt::Txt;
  std::string getCachePath() { return "/cache/xtc"; }
  bool clearCache() { return Storage.removeDir(getCachePath().c_str()); }
};
int clearCalls = 0;
void clearBookCache(const std::string&) { ++clearCalls; }
void resetTaskWatchdogIfSubscribed() {}
struct WebServer {
  String path = "/src.epub", dst = "/dst.epub";
  bool overwrite = true;
  int code = 0;
  void send(int c, const char* = "", const char* = "") { code = c; }
};
enum { RAW_START, RAW_WRITE, RAW_END, RAW_ABORTED };
struct HTTPRaw {
  int status = RAW_START;
  uint8_t* buf = nullptr;
  size_t currentSize = 0, totalSize = 0;
};
namespace BookMoveUtils {
enum class RenameMigrationResult { Success, RolledBack, KeepRenamed, Uncertain };
RenameMigrationResult result = RenameMigrationResult::Success;
int calls = 0;
RenameMigrationResult moveFile(const std::string&, const std::string&) {
  ++calls;
  return result;
}
}  // namespace BookMoveUtils
namespace BookActions {
bool result = true;
int calls = 0;
bool deleteFile(const std::string&) {
  ++calls;
  return result;
}
}  // namespace BookActions
struct WebDAVHandler {
  HalFile _putFile;
  String _putPath;
  bool _putOk = false, _putExisted = false;
  String getRequestPath(WebServer& s) { return s.path; }
  String getDestinationPath(WebServer& s) { return s.dst; }
  bool getOverwrite(WebServer& s) { return s.overwrite; }
  bool isProtectedPath(const String& p) { return p.find("/.") != std::string::npos; }
  void raw(WebServer&, const String&, HTTPRaw&);
  void handlePut(WebServer&);
  void handleDelete(WebServer&);
  void handleMove(WebServer&);
  void handleCopy(WebServer&);
};
