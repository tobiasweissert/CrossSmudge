#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <string>

namespace ink::http {
constexpr size_t MaxBody = 8192;
constexpr size_t BrowserBody = 1024 * 1024;
constexpr size_t MaxUpload = 2048;
constexpr size_t MaxDomains = 4;
struct Url { char host[128]{}; char path[385]{}; };
struct Header { char name[33]{}; char value[129]{}; };
struct Request {
  Url url;
  size_t limit = BrowserBody;
  bool browser = false;
  bool post = false;
  uint32_t timeoutMs = 15000;
  size_t bodySize = 0;
  char body[MaxUpload]{};
  Header headers[4];
  size_t headerCount = 0;
};
struct Result {
  uint32_t checksum=1;
  int status = 0;
  const char* error = nullptr;
  size_t size = 0;
  char body[MaxBody]{};
  char* extended = nullptr; // Job-owned PSRAM on browser hosts; API3 never sets this.
  size_t capacity = MaxBody;
  void* sinkContext=nullptr;
  bool (*sink)(void*,const char*,size_t)=nullptr;
  char location[513]{}, url[513]{}, contentType[129]{};
  char* data() { return extended ? extended : body; }
  const char* data() const { return extended ? extended : body; }
};
struct Domains {
  bool publicHttps = false;
  char names[MaxDomains][128]{};
  size_t count = 0;
  bool contains(const char* host) const;
  bool add(std::string_view host);
};
bool resolveRedirect(const Url& base,std::string_view reference,Url& result);
bool parseUrl(std::string_view text, Url& url);
bool validHost(std::string_view host);
bool publicV4(uint32_t hostOrder);
bool safeHeader(std::string_view name, std::string_view value);
bool safeRelativePath(std::string_view path);
bool resolveAppDirectory(std::string_view path, std::string_view appRoot, std::string_view saveRoot, std::string& out);
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
struct Scratch { char line[513]{}; Url current; char chunk[2048]{}; size_t headerBytes = 0; };
void exchange(const Request&, Result&, Scratch&, Io&, const std::atomic<bool>& cancel);
}  // namespace ink::http
