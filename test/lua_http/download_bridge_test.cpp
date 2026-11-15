#include "download_fakes.h"
#include "posix_download_files.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
struct Heap {
  size_t used=0,peak=0;
  static void* alloc(void* ctx,void* p,size_t old,size_t n) {
    auto& h=*static_cast<Heap*>(ctx);if(!p)old=0;
    if(!n){free(p);h.used-=old;return nullptr;}
    if(n>75*1024-(h.used-old))return nullptr;
    void* q=realloc(p,n);if(q){h.used=h.used-old+n;h.peak=std::max(h.used,h.peak);}return q;
  }
};
struct Context {const char* root;Wire wire;};
static void execute(void* p,Job& j) {
  auto& c=*static_cast<Context*>(p);PosixDownloadFiles files(c.root);auto io=c.wire.io();
  exchangeDownload(j,io,files);
}
static bool spawn(void(*fn)(void*),void* p){fn(p);return true;}
static const char* diskRoot=nullptr;
static const char* readAt(const char*,size_t offset,char* p,size_t count,size_t& got,size_t& total) {
  PosixDownloadFiles files(diskRoot);DownloadStore store(files);return store.readAt(offset,p,count,got,total);
}
static void script(lua_State* L,const char* s) {int rc=luaL_dostring(L,s);if(rc)fprintf(stderr,"%s\n",lua_tostring(L,-1));assert(rc==LUA_OK);}
int main(int argc,char** argv) {
  assert(argc==2);std::ifstream in(argv[1],std::ios::binary);assert(in);
  std::string archive((std::istreambuf_iterator<char>(in)),{});assert(archive.size()==1460742);
  char root[]="/root/.hermes/cache/scratch/http-file-XXXXXX";assert(mkdtemp(root));diskRoot=root;
  Context ctx{root,{}};ctx.wire.response="HTTP/1.1 200 OK\r\nContent-Length: "+std::to_string(archive.size())+"\r\n\r\n"+archive;
  Session session({&ctx,execute,spawn,readAt});session.allowed=true;assert(session.domains.add("example.com"));
  Heap heap;auto* L=lua_newstate(Heap::alloc,&heap);assert(L);luaL_openlibs(L);
  lua_newtable(L);registerApi(L,session);lua_setglobal(L,"smudge");
  script(L,"local a,b=smudge.http_download{url='https://example.com/',max_bytes=2097152};assert(not a and b=='api_required')");
  session.fileApi=true;assert(session.configureFileRoot("/.smudge/applications/connections"));
  script(L,"for _,o in ipairs({{url='https://example.com/',max_bytes=0},{url='https://example.com/',max_bytes=2097153},{url='https://example.com/',max_bytes=4,headers={Authorization='secret'}},{url='https://example.com/',max_bytes=4,path='../old'}}) do local id,e=smudge.http_download(o);assert(id==nil and e=='invalid_options') end");
  script(L,"local id,e=smudge.http_download{url='https://evil.com/',max_bytes=4};assert(not id and e=='domain_denied')");
  script(L,"id=assert(smudge.http_download{url='https://example.com/',max_bytes=2097152});local a,e=smudge.download_read(0,1);assert(not a and e=='busy');function on_http(e) assert(e.kind=='download' and e.id==id and e.status==200 and e.size==1460742 and e.body==nil and e.error==nil);got=true end;assert(smudge.http_cancel(id))");
  // Successful promotion wins late cancellation, including cancellation before dispatch.
  assert(dispatch(L,session)==LUA_OK);script(L,"assert(got);local s,n=smudge.download_read(0,1024);assert(#s==1024 and n==1460742);assert(smudge.download_read(1460742,1)=='');local a,e=smudge.download_read(1460743,1);assert(not a and e=='invalid_offset');for _,n in ipairs({0,1025,-1,1.5}) do assert(smudge.download_read(0,n)==nil) end");
  // Run the actual importer using production read-at against the native committed file.
  script(L,"local J=dofile('apps/cp-connections/json.lua');local A=dofile('apps/cp-connections/archive.lua');local s=A.new(smudge.download_read,J.decode);local done,p,e;repeat done,p,e=s:step() until done;assert(p and not e and p.date=='2026-11-15' and s.records==1248,tostring(e)..' records='..s.records..' offset='..s.offset)");
  session.close();lua_close(L);assert(heap.used==0);
  // Refuse symlinks in both root resolution and generation inspection.
  std::string link=std::string(root)+"/link";assert(symlink(root,link.c_str())==0);PosixDownloadFiles linked(link.c_str());assert(!linked.prepare());
  PosixDownloadFiles files(root);assert(files.remove("0"));assert(files.remove("1"));assert(files.remove("part"));unlink(link.c_str());
  std::string leaf=std::string(root)+"/0";assert(symlink("/etc/passwd",leaf.c_str())==0);DownloadStore store(files);assert(!store.begin());unlink(leaf.c_str());assert(rmdir(root)==0);
  printf("PASS real native disk + production bridge + full captured archive: Lua peak %zu / 76800, retained %zu; absolute seek/chunk, API gates, no body, late cancel, symlink refusal\n",heap.peak,heap.used);
}
