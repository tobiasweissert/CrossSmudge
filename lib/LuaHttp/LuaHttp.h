#pragma once
#include "HttpCore.h"
#include "AllocationBudget.h"
struct lua_State;
namespace ink::http {
struct Job;
struct Executor {
  void* context;
  void (*run)(void*, Job&);
  bool (*spawn)(void (*)(void*), void*);
};
struct Job {
  Request request;
  Result result;
  Scratch scratch;
  AllocationBudget tlsBudget;
  Executor executor;
  std::atomic<bool> cancel{false}, done{false};
  std::atomic<unsigned> references{1};
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
