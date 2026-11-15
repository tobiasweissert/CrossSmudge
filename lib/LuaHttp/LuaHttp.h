#pragma once
#include "HttpCore.h"
#include "AllocationBudget.h"
#include "DownloadStore.h"
struct lua_State;
namespace ink::http {
struct Job;
struct Executor {
  void* context;
  void (*run)(void*, Job&);
  bool (*spawn)(void (*)(void*), void*);
  const char* (*readFile)(const char* root, size_t offset, char*, size_t count, size_t& got, size_t& total) = nullptr;
};
struct Job {
  Request request;
  Result result;
  Scratch scratch;
  AllocationBudget tlsBudget;
  Executor executor;
  std::atomic<bool> cancel{false}, done{false};
  std::atomic<unsigned> references{1};
  bool download=false, committed=false;
  size_t maxBytes=0;
  char fileRoot[256]{};
  int id=0;
  void release();
};
class Session {
 public:
  explicit Session(Executor executor):executor_(executor){}
  ~Session(){close();}
  Session(const Session&)=delete;
  Session& operator=(const Session&)=delete;
  Domains domains;
  bool allowed=false;
  bool fileApi=false;
  char fileRoot[256]{};
  char readBuffer[DownloadChunk]{}; // session-owned: no Lua OOM can strand a file or heap buffer
  bool configureFileRoot(std::string_view appRoot);
  const char* readAt(size_t offset, size_t count, size_t& got, size_t& total);
  Job* reserve();
  bool start();
  void discard();
  void close();
  bool ready()const{return job_ && job_->done.load(std::memory_order_acquire);}
  Job* job()const{return job_;}
  static bool busy();
 private:
  Executor executor_;
  Job* job_=nullptr;
};
// Appends functions to the table at stack top; production and host tests share registration.
void registerApi(lua_State*, Session&);
void restrictStandardLibraries(lua_State*);
// Call only from LuaRunner::onUpdate while its Lua mutex is held. Protected against Lua OOM.
int dispatch(lua_State*,Session&);
Executor platformExecutor();
} // namespace ink::http
