// Host-only IO boundary for the actual BookMove checked intent helper.
// Store codecs and physical FAT atomicity are not modeled here.
#include <fcntl.h>
struct FileHandle {
  bool valid = false, dir = false;
  std::string* bytes = nullptr;
  size_t position = 0;
  explicit operator bool() const { return valid; }
  bool isDirectory() const { return dir; }
  bool close() { const bool ok = valid; valid = false; return ok; }
  bool sync() { assert(valid && bytes); return true; }
  size_t size() const { assert(valid && bytes); return bytes->size(); }
  size_t write(const uint8_t* p, size_t n) {
    assert(valid && bytes && !dir);
    bytes->append(reinterpret_cast<const char*>(p), n);
    return n;
  }
  int read(char* p, size_t n) {
    assert(valid && bytes && !dir);
    const auto count = std::min(n, bytes->size() - position);
    if (count) std::memcpy(p, bytes->data() + position, count);
    position += count;
    return static_cast<int>(count);
  }
};
using HalFile = FileHandle;
namespace library {
inline unsigned dirtyCalls = 0;
inline void markLibraryIndexDirty() { ++dirtyCalls; }
}
struct StorageAdapter {
  bool removeOk = true;
  int renameFailure = 0, renames = 0, renameFailureEffect = 0;
  std::map<std::string, bool> paths;
  std::map<std::string, std::string> journalBytes;
  std::vector<std::pair<std::string, std::string>> moves;
  static bool isIntent(const char* p) { return std::string(p) == "/.crosspoint/book-move.pending"; }
  bool remove(const char* p) {
    events.push_back("remove");
    if (!removeOk) return false;
    paths.erase(p);
    journalBytes.erase(p);
    return true;
  }
  FileHandle open(const char* p) const {
    const auto i = paths.find(p);
    return {i != paths.end(), i != paths.end() && i->second, nullptr, 0};
  }
  FileHandle open(const char* p, int flags) {
    assert(isIntent(p) && flags == (O_WRONLY | O_CREAT | O_EXCL));
    events.push_back("intent-create");
    if (paths.count(p)) return {};
    paths[p] = false;
    journalBytes[p].clear();
    return {true, false, &journalBytes[p], 0};
  }
  bool openFileForRead(const char* tag, const char* p, HalFile& f) {
    assert(std::string(tag) == "BookMove" && isIntent(p));
    if (!paths.count(p) || !journalBytes.count(p)) return false;
    f = {true, false, &journalBytes.at(p), 0};
    return true;
  }
  bool exists(const char* p) const { return paths.count(p); }
  bool rename(const char* a, const char* b) {
    events.push_back("rename");
    moves.push_back({a, b});
    ++renames;
    if (renames == renameFailure) {
      if (renameFailureEffect && paths.count(a)) {
        paths[b] = paths[a];
        if (renameFailureEffect == 2) paths.erase(a);
      }
      return false;
    }
    if (!paths.count(a) || paths.count(b)) return false;
    paths[b] = paths[a];
    paths.erase(a);
    return true;
  }
} Storage;
