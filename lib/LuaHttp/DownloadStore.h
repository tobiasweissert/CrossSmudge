#pragma once
#include <cstddef>
#include <cstdint>
#include <atomic>

namespace ink::http {
constexpr size_t MaxDownload = 2 * 1024 * 1024;
constexpr size_t DownloadChunk = 1024;
// Backend is confined to a firmware-provided root and the fixed names 0, 1, part.
// rename must fail without changing either file if destination exists or on failure.
class DownloadFiles {
 public:
  virtual ~DownloadFiles() = default;
  virtual bool prepare() = 0;
  virtual int exists(const char*) = 0; // -1 IO error, 0 proven absent, 1 present
  virtual bool open(const char* leaf, bool create) = 0;
  virtual size_t size() = 0;
  virtual bool seek(size_t) = 0;
  virtual int read(char*, size_t) = 0;
  virtual size_t write(const char*, size_t) = 0;
  virtual bool sync() = 0;
  virtual bool close() = 0;
  virtual bool remove(const char*) = 0; // absent is success
  virtual bool promote(const char* leaf) = 0; // part -> absent leaf
};
class DownloadStore {
 public:
  explicit DownloadStore(DownloadFiles& files): files_(files) {}
  ~DownloadStore() { abort(); }
  bool begin();
  bool append(const char*, size_t);
  bool commit(const std::atomic<bool>& cancel, bool (*canPromote)(void*) = nullptr, void* context = nullptr);
  void abort();
  const char* readAt(size_t offset, char* out, size_t count, size_t& got, size_t& total);
 private:
  struct Record { uint32_t generation=0; size_t size=0; };
  Record inspect(const char*);
  Record latest();
  DownloadFiles& files_;
  bool writing_=false, healthy_=true;
  unsigned selected_=0;
  uint32_t generation_=0;
  size_t size_=0;
};
}
