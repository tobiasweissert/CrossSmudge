#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// Disk transport only: production record, version, path and codec are not replaced.
inline std::map<std::string, std::string> bookmarkDisk;
struct HalFile {
  std::string path;
  size_t pos = 0;
  explicit operator bool() const { return !path.empty(); }
  int available() { return static_cast<int>(bookmarkDisk[path].size() - pos); }
  int read(void* data, size_t size) {
    size = std::min(size, bookmarkDisk[path].size() - pos);
    std::memcpy(data, bookmarkDisk[path].data() + pos, size);
    pos += size;
    return size;
  }
  size_t write(const uint8_t* data, size_t size) {
    bookmarkDisk[path].append(reinterpret_cast<const char*>(data), size);
    return size;
  }
  bool sync() { return true; }
  void close() {}
};
using FsFile = HalFile;
constexpr int O_WRONLY = 1, O_CREAT = 2, O_TRUNC = 4;
struct BookmarkDisk {
  std::string directory;
  bool mkdir(const char* path) {
    directory = path;
    return true;
  }
  bool exists(const char* path) { return bookmarkDisk.count(path); }
  bool remove(const char* path) { return bookmarkDisk.erase(path) > 0; }
  bool rename(const char* from, const char* to) {
    if (!exists(from) || exists(to)) return false;
    bookmarkDisk[to] = bookmarkDisk[from];
    bookmarkDisk.erase(from);
    return true;
  }
  HalFile open(const char* path, int) {
    bookmarkDisk[path].clear();
    return {path};
  }
  bool openFileForRead(const char*, const std::string& path, HalFile& file) {
    if (!exists(path.c_str())) return false;
    file = HalFile{path};
    return true;
  }
  bool openFileForWrite(const char*, const std::string& path, HalFile& file) {
    bookmarkDisk[path].clear();
    file = HalFile{path};
    return true;
  }
  std::vector<std::string> listFiles(const char* dir) {
    std::vector<std::string> result;
    const std::string prefix = std::string(dir) + "/";
    for (const auto& entry : bookmarkDisk) {
      if (entry.first.rfind(prefix, 0) == 0) result.push_back(entry.first.substr(prefix.size()));
    }
    return result;
  }
};
inline BookmarkDisk Storage;
