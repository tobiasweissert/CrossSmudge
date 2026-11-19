#pragma once

#include <Print.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>

namespace HalFileTest {
inline bool failNextWrite = false;
inline bool failNextClose = false;
}  // namespace HalFileTest

constexpr int O_WRITE = 1;
constexpr int O_CREAT = 2;
constexpr int O_TRUNC = 4;

constexpr int O_RDONLY = 0, O_RDWR = 8;
class HalFile : public Print {
 public:
  HalFile() = default;
  explicit HalFile(const char* path) { open(path, "wb"); }
  ~HalFile() { close(); }
  HalFile(HalFile&& other) noexcept : file_(std::exchange(other.file_, nullptr)) {}
  HalFile& operator=(HalFile&& other) noexcept {
    close();
    file_ = std::exchange(other.file_, nullptr);
    return *this;
  }
  bool allocationFailed() const { return false; }
  bool iterationFailed() const { return false; }
  bool isDirectory() const { return false; }
  HalFile openNextFile() { return {}; }
  void getName(char* out, size_t count) {
    if (count) out[0] = 0;
  }
  bool sync() { return flush(); }
  size_t fileSize() const { return size(); }
  uint64_t fileSize64() const { return size(); }
  bool seek64(size_t p) { return seek(p); }
  bool seekSet(size_t p) { return seek(p); }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;

  bool open(const char* path, const char* mode) {
    close();
    file_ = std::fopen(path, mode);
    return file_ != nullptr;
  }
  int available() const { return file_ ? static_cast<int>(size() - position()) : 0; }
  int read() { return file_ ? std::fgetc(file_) : -1; }
  int read(void* buffer, size_t count) { return file_ ? static_cast<int>(std::fread(buffer, 1, count, file_)) : 0; }
  size_t write(const void* buffer, size_t count) {
    if (HalFileTest::failNextWrite) {
      HalFileTest::failNextWrite = false;
      return 0;
    }
    const size_t allowed = count < writeBytesLeft_ ? count : writeBytesLeft_;
    const size_t written = file_ ? std::fwrite(buffer, 1, allowed, file_) : 0;
    writeBytesLeft_ -= written;
    return written;
  }
  size_t write(uint8_t byte) { return write(&byte, 1); }
  bool flush() { return file_ && std::fflush(file_) == 0; }
  bool seek(size_t pos) { return file_ && std::fseek(file_, static_cast<long>(pos), SEEK_SET) == 0; }
  bool seekCur(size_t offset) { return file_ && std::fseek(file_, static_cast<long>(offset), SEEK_CUR) == 0; }
  bool close() {
    if (!file_) return false;
    const bool ok = std::fclose(file_) == 0;
    file_ = nullptr;
    if (HalFileTest::failNextClose) {
      HalFileTest::failNextClose = false;
      return false;
    }
    return ok;
  }
  bool isOpen() const { return file_ != nullptr; }
  explicit operator bool() const { return isOpen(); }
  size_t position() const { return file_ ? static_cast<size_t>(std::ftell(file_)) : 0; }
  size_t size() const {
    if (!file_) return 0;
    const long offset = std::ftell(file_);
    std::fseek(file_, 0, SEEK_END);
    const long end = std::ftell(file_);
    std::fseek(file_, offset, SEEK_SET);
    return end > 0 ? static_cast<size_t>(end) : 0;
  }

  size_t write(const uint8_t* buffer, size_t count) { return write(static_cast<const void*>(buffer), count); }
  void limitWritesTo(size_t count) { writeBytesLeft_ = count; }

 private:
  std::FILE* file_ = nullptr;
  size_t writeBytesLeft_ = SIZE_MAX;
};

using FsFile = HalFile;
class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }
  bool mkdir(const char* p) {
    std::error_code e;
    std::filesystem::create_directories(p, e);
    return !e;
  }
  bool removeDir(const char* p) {
    std::error_code e;
    std::filesystem::remove_all(p, e);
    return !e;
  }
  int readOpens = 0;
  std::string dictionaryTempPath;
  std::string resolvePath(const std::string& path) const {
    return path == "/.crosspoint/dicthtml.tmp" && !dictionaryTempPath.empty() ? dictionaryTempPath : path;
  }
  HalFile open(const char* path, int mode = O_RDONLY) {
    HalFile f;
    f.open(resolvePath(path).c_str(), mode == O_RDWR ? "r+b" : mode == O_RDONLY ? "rb" : "wb");
    return f;
  }
  bool openFileForRead(const char*, const std::string& path, HalFile& file) {
    readOpens++;
    return file.open(resolvePath(path).c_str(), "rb");
  }
  bool openFileForWrite(const char*, const std::string& path, HalFile& file) { return file.open(path.c_str(), "wb"); }
  bool exists(const char* path) const {
    std::FILE* file = std::fopen(path, "rb");
    if (!file) return false;
    std::fclose(file);
    return true;
  }
  bool remove(const std::string& path) { return std::remove(resolvePath(path).c_str()) == 0; }
  bool rename(const char* from, const char* to) { return std::rename(from, to) == 0; }
};

#define Storage HalStorage::getInstance()

inline uint32_t millis() { return 0; }
inline void delay(uint32_t) {}
