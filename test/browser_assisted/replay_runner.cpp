// Test-only numeric loopback transport. Production TLS/HAL is unchanged.
#define main ordinaryRunnerMain
#include "../browser/runner.cpp"
#undef main
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <fstream>
#ifdef COMBINED_CODEC_HOST
void registerCombinedImages(lua_State*);
#endif
static ink::http::Session* replaySession;
static int port=0;
struct Socket {int fd=-1;};
static uint32_t milliseconds(void*){using namespace std::chrono;return uint32_t(duration_cast<std::chrono::milliseconds>(steady_clock::now().time_since_epoch()).count());}
static void execute(void*,ink::http::Job& j){
 // Test-only framed pipes to an actual Go net/http server. No JSON responder here.
 if(getenv("REPLAY_SERVICE_IPC")){
  struct Bridge {std::string sent,received;size_t at=0;bool loaded=false;} bridge;
  ink::http::Io io{&bridge,[](void*,const char* host,uint32_t,const std::atomic<bool>&){return !strcmp(host,ink::http::BackendHost);},
   [](void* p,char* bytes,size_t count){auto& b=*(Bridge*)p;
    if(!b.loaded){
     auto full=[](int fd,void* data,size_t n,bool writing){auto* p=(char*)data;while(n){ssize_t k=writing ? ::write(fd,p,n) : ::read(fd,p,n);if(k<=0)return false;p+=k;n-=k;}return true;};
     uint32_t length=htonl(b.sent.size());if(!full(4,&length,4,true)||!full(4,b.sent.data(),b.sent.size(),true)||!full(3,&length,4,false))return -1;
     length=ntohl(length);if(length>70000)return -1;b.received.resize(length);if(!full(3,b.received.data(),length,false))return -1;b.loaded=true;
    }
    count=std::min(count,b.received.size()-b.at);memcpy(bytes,b.received.data()+b.at,count);b.at+=count;return int(count);},
   [](void* p,const char* bytes,size_t count){((Bridge*)p)->sent.append(bytes,count);return int(count);},[](void*){},milliseconds};
  ink::http::exchange(j.request,j.result,j.scratch,io,j.cancel);return;
 }
 if(port==0){
  std::ifstream in(getenv("REPLAY_HTTP_RESPONSE"),std::ios::binary);assert(in);struct Fixture{std::string body,sent;size_t at=0;} f{std::string((std::istreambuf_iterator<char>(in)),{})};assert(f.body.size()<=65536);
  ink::http::Io io{&f,[](void*,const char* host,uint32_t,const std::atomic<bool>&){return !strcmp(host,ink::http::BackendHost);},[](void* p,char* b,size_t n){auto& f=*(Fixture*)p;n=std::min(n,f.body.size()-f.at);memcpy(b,f.body.data()+f.at,n);f.at+=n;return int(n);},[](void* p,const char* b,size_t n){((Fixture*)p)->sent.append(b,n);return int(n);},[](void*){},milliseconds};
  ink::http::exchange(j.request,j.result,j.scratch,io,j.cancel);assert(f.sent.find("Authorization: Bearer "+std::string(64,'a'))!=f.sent.npos);return;
 }
 Socket socket;
 ink::http::Io io{&socket,[](void* p,const char* host,uint32_t,const std::atomic<bool>&){assert(!strcmp(host,ink::http::BackendHost));auto& s=*(Socket*)p;s.fd=::socket(AF_INET,SOCK_STREAM,0);if(s.fd<0)return false;timeval tv{5,0};setsockopt(s.fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);return ::connect(s.fd,(sockaddr*)&a,sizeof(a))==0;},[](void* p,char* b,size_t n){return int(::recv(((Socket*)p)->fd,b,n,0));},[](void* p,const char* b,size_t n){return int(::send(((Socket*)p)->fd,b,n,MSG_NOSIGNAL));},[](void* p){auto& s=*(Socket*)p;if(s.fd>=0)::close(s.fd);s.fd=-1;},milliseconds};
 ink::http::exchange(j.request,j.result,j.scratch,io,j.cancel);
}
static bool start(void(*fn)(void*),void* a){fn(a);return true;}
static bool token(void*,char (&out)[65]){memset(out,'a',64);out[64]=0;return true;}
static bool entropy(void*,unsigned char* p,size_t n){static unsigned counter=0;++counter;for(size_t i=0;i<n;++i)p[i]=(counter>>(i%4*8))^i;return true;}
int main(int argc,char** argv){assert(argc==4);port=atoi(argv[2]);assert(port>=0&&port<65536);Heap heap;lua_State* L=lua_newstate(Heap::alloc,&heap);assert(ink::runtime::openEssential(L)==LUA_OK);assert(ink::runtime::openNetwork(L,true)==LUA_OK);assert(ink::browser::openBrowserWork(L)==LUA_OK);
#ifdef BROWSER_FONT_HOST
 registerFontHost(L);
#endif
#ifdef COMBINED_CODEC_HOST
 registerCombinedImages(L);
#endif
 lua_pushstring(L,argv[3]);lua_setglobal(L,"native_resource_root");
 ink::http::Session session({nullptr,execute,start});replaySession=&session;session.allowed=true;session.cooperative=true;session.domains.publicHttps=true;session.backendEnabled=true;session.backendAccess={nullptr,token,nullptr,entropy};snprintf(session.resourceRoot,sizeof(session.resourceRoot),"%s",argv[3]);
 lua_newtable(L);ink::http::registerApi(L,session);ink::browser::registerWork(L);lua_setglobal(L,"native_api");
 lua_pushcfunction(L,[](lua_State* L){int status=ink::http::dispatch(L,*replaySession);if(status!=LUA_OK)return lua_error(L);return 0;});lua_setglobal(L,"native_dispatch");
 lua_pushcfunction(L,[](lua_State* L){lua_pushinteger(L,milliseconds(nullptr));return 1;});lua_setglobal(L,"native_millis");
 lua_pushcfunction(L,[](lua_State*){std::this_thread::sleep_for(std::chrono::milliseconds(10));return 0;});lua_setglobal(L,"native_pause");
 lua_register(L,"test_read",testRead);lua_register(L,"test_size",testSize);
 int status=luaL_dofile(L,argv[1]);if(status)fprintf(stderr,"%s\n",lua_tostring(L,-1));session.close();lua_close(L);assert(!heap.used);assert(!ink::http::Session::busy());printf("replay Lua peak=%zu int=%zu float=%zu\n",heap.peak,sizeof(lua_Integer),sizeof(lua_Number));return status?1:0;
}
