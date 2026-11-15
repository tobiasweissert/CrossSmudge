#include "LuaHttp.h"
#include <cstring>
#include <new>
#include <initializer_list>
#ifdef ARDUINO_ARCH_ESP32
#include <Logging.h>
#endif
extern "C" {
#include "lauxlib.h"
#include "lua.h"
}
namespace ink::http {
namespace {
static_assert(sizeof(Job) <= 16 * 1024, "HTTP native job exceeds its budget");
std::atomic<bool> occupied{false};
std::atomic<unsigned> nextId{0};
void worker(void* arg) {
  auto* j=static_cast<Job*>(arg);
  j->executor.run(j->executor.context,*j);
  if(j->cancel.load()) { j->result.error="cancelled";j->result.size=0; }
  j->done.store(true,std::memory_order_release);
  j->release();
}
Session& session(lua_State* L){return *static_cast<Session*>(lua_touserdata(L,lua_upvalueindex(1)));}
int error(lua_State* L,const char* message){lua_pushnil(L);lua_pushstring(L,message);return 2;}
std::string_view string(lua_State* L,int idx) {
  if(lua_type(L,idx)!=LUA_TSTRING)return {};
  size_t n;const char* p=lua_tolstring(L,idx,&n);return {p,n};
}
void rawField(lua_State* L,int index,const char* key){lua_pushstring(L,key);lua_rawget(L,index);}
int requestInner(lua_State* L) {
  auto& s=session(L);
  if(!s.allowed) return error(L,"permission_denied");
  if(!lua_istable(L,1)) return error(L,"invalid_options");
  // Validate all Lua types and limits before reserving native ownership; no luaL_error across C++ resources.
  rawField(L,1,"url");auto url=string(L,-1);
  if(url.empty() || url.find('\0')!=url.npos)return error(L,"invalid_url");
  Job* j=s.reserve();if(!j)return error(L,Session::busy()?"busy":"out_of_memory");
  const char* failure=nullptr;
  if(!parseUrl(url,j->request.url))failure="invalid_url";
  else if(!s.domains.contains(j->request.url.host))failure="domain_denied";
  lua_pop(L,1);
  rawField(L,1,"method");auto method=string(L,-1);
  if(!lua_isnil(L,-1) && method!="GET" && method!="POST")failure="invalid_method";
  j->request.post=method=="POST";lua_pop(L,1);
  rawField(L,1,"body");auto body=string(L,-1);
  if((!lua_isnil(L,-1) && lua_type(L,-1)!=LUA_TSTRING) || body.size()>MaxUpload || (!j->request.post && !body.empty()))failure="invalid_body";
  else if(!body.empty()){memcpy(j->request.body,body.data(),body.size());j->request.bodySize=body.size();}
  lua_pop(L,1);
  rawField(L,1,"timeout_ms");
  if(!lua_isnil(L,-1)) {
    if(!lua_isinteger(L,-1) || lua_tointeger(L,-1)<1000 || lua_tointeger(L,-1)>30000)failure="invalid_timeout";
    else j->request.timeoutMs=lua_tointeger(L,-1);
  }
  lua_pop(L,1);
  rawField(L,1,"headers");
  if(!lua_isnil(L,-1)) {
    if(!lua_istable(L,-1))failure="invalid_headers";
    else {
      lua_pushnil(L);
      while(lua_next(L,-2)) {
        auto name=string(L,-2),value=string(L,-1);
        if(j->request.headerCount==4 || lua_type(L,-1)!=LUA_TSTRING || !safeHeader(name,value))failure="invalid_headers";
        else {auto& h=j->request.headers[j->request.headerCount++];memcpy(h.name,name.data(),name.size());memcpy(h.value,value.data(),value.size());}
        lua_pop(L,1);
        if(failure){lua_pop(L,1);break;}
      }
    }
  }
  lua_pop(L,1);
  // Reject unknown option keys so callers cannot mistakenly believe a security/download option worked.
  lua_pushnil(L);while(lua_next(L,1)) {
    auto key=string(L,-2);
    if(key!="url" && key!="method" && key!="body" && key!="headers" && key!="timeout_ms")failure="invalid_options";
    lua_pop(L,1);
    if(failure){lua_pop(L,1);break;}
  }
  if(failure){s.discard();return error(L,failure);}
  int id=j->id;
  if(!s.start()){s.discard();return error(L,"worker_failed");}
  lua_pushinteger(L,id);return 1;
}
int request(lua_State* L) {
  auto& s=session(L);
  // Reserve ownership is cleaned even if Lua allocation fails during parsing.
  if(s.job())return error(L,"busy");
  lua_pushlightuserdata(L,&s);lua_pushcclosure(L,requestInner,1);lua_pushvalue(L,1);
  int status=lua_pcall(L,1,2,0);
  if(status!=LUA_OK){s.discard();return lua_error(L);}
  return 2;
}
int cancel(lua_State* L) {
  auto& s=session(L);auto* j=s.job();
  bool found=j && lua_isinteger(L,1) && lua_tointeger(L,1)==j->id;
  if(found)j->cancel.store(true);
  lua_pushboolean(L,found);return 1;
}
int textOnlyLoad(lua_State* L) {
  const int args = lua_gettop(L) >= 4 ? 4 : 3;
  lua_settop(L, args);
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_insert(L, 1);
  lua_pushliteral(L, "t");
  lua_replace(L, 4);
  lua_call(L, args, LUA_MULTRET);
  return lua_gettop(L);
}
int deliver(lua_State* L) {
  auto* j=static_cast<Job*>(lua_touserdata(L,1));
  lua_getglobal(L,"on_http");if(!lua_isfunction(L,-1))return 0;
  lua_createtable(L,0,4);
  lua_pushinteger(L,j->id);lua_setfield(L,-2,"id");
  lua_pushinteger(L,j->result.status);lua_setfield(L,-2,"status");
  if(j->result.error){lua_pushstring(L,j->result.error);lua_setfield(L,-2,"error");}
  else {lua_pushlstring(L,j->result.body,j->result.size);lua_setfield(L,-2,"body");}
  lua_call(L,1,0);return 0;
}
}
void Job::release(){if(references.fetch_sub(1,std::memory_order_acq_rel)==1){delete this;occupied.store(false);}}
bool Session::busy(){return occupied.load();}
Job* Session::reserve(){
  if(job_ || occupied.exchange(true))return nullptr;
  // One bounded ~12 KiB job outlives the activity while cancellation drains. The worker owns a reference.
  job_=new(std::nothrow) Job{};
  if(!job_){
#ifdef ARDUINO_ARCH_ESP32
    LOG_ERR("LHTTP", "Job allocation failed (%zu bytes)", sizeof(Job));
#endif
    occupied.store(false);return nullptr;
  }
  job_->executor=executor_;
  job_->id=int(nextId.fetch_add(1)%0x3fffffff)+1;
  return job_;
}
bool Session::start(){
  job_->references.fetch_add(1);
  if(!executor_.spawn(worker,job_)){job_->release();return false;}return true;
}
void Session::discard(){if(job_){job_->release();job_=nullptr;}}
void Session::close(){allowed=false;if(job_)job_->cancel.store(true);discard();}
void restrictStandardLibraries(lua_State* L) {
  for (const char* key : {"loadfile", "io", "package", "require", "debug"}) {
    lua_pushnil(L); lua_setglobal(L, key);
  }
  lua_getglobal(L, "os");
  if (lua_istable(L, -1)) {
    for (const char* key : {"execute", "remove", "rename", "tmpname", "getenv", "exit", "setlocale"}) {
      lua_pushnil(L); lua_setfield(L, -2, key);
    }
  }
  lua_pop(L, 1);
  lua_getglobal(L, "load"); lua_pushcclosure(L, textOnlyLoad, 1); lua_setglobal(L, "load");
}
void registerApi(lua_State* L,Session& s){
  lua_pushlightuserdata(L,&s);lua_pushcclosure(L,request,1);lua_setfield(L,-2,"http_request");
  lua_pushlightuserdata(L,&s);lua_pushcclosure(L,cancel,1);lua_setfield(L,-2,"http_cancel");
}
int dispatch(lua_State* L,Session& s){
  if(!s.ready())return LUA_OK;
  // The completed slot stays owned through callback/OOM cleanup. Submit the next request in on_update.
  lua_pushcfunction(L,deliver);lua_pushlightuserdata(L,s.job());
  int status=lua_pcall(L,1,0,0);s.discard();return status;
}
} // namespace ink::http
