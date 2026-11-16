#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <string>

namespace ink::http {
constexpr size_t MaxBody = 8192;
constexpr size_t MaxUpload = 2048;
constexpr size_t MaxDomains = 4;
struct Url { char host[128]{}; char path[385]{}; };
struct Header { char name[33]{}; char value[129]{}; };
struct Request {
  Url url;
  bool post = false;
  uint32_t timeoutMs = 15000;
  size_t bodySize = 0;
  char body[MaxUpload]{};
  Header headers[4];
  size_t headerCount = 0;
};
struct Result {
  int status = 0;
  const char* error = nullptr;
  size_t size = 0;
  char body[MaxBody]{};
};
struct Domains {
  char names[MaxDomains][128]{};
  size_t count = 0;
  bool contains(const char* host) const;
  bool add(std::string_view host);
};
bool parseUrl(std::string_view text, Url& url);
bool validHost(std::string_view host);
bool publicV4(uint32_t hostOrder);
bool safeHeader(std::string_view name, std::string_view value);
bool safeRelativePath(std::string_view path);
bool resolveAppPath(std::string_view path, std::string_view appRoot, std::string_view saveRoot, std::string& out);
// Transport owns TLS/DNS/time verification. No bytes may be written before connect succeeds.
struct Io {
  void* context;
  bool (*connect)(void*, const char* host, uint32_t timeoutMs, const std::atomic<bool>& cancel);
  int (*read)(void*, char*, size_t);
  int (*write)(void*, const char*, size_t);
  void (*close)(void*);
  uint32_t (*millis)(void*);
};
// Fixed scratch space belongs to the job, never the small worker stack.
struct Scratch { char line[513]{}; size_t headerBytes = 0; };
void exchange(const Request&, Result&, Scratch&, Io&, const std::atomic<bool>& cancel);
}  // namespace ink::http
