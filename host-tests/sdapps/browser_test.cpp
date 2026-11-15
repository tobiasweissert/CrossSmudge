#include "AppAbi.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
extern "C" int browser_entry(int,char**);
static int presentations, requests, edits, releases;
static CpViewKind kind;
static std::vector<std::string> labels;
static bool acceptRequest=true;
static void* alloc(void*,size_t n){return calloc(1,n);}
static void release(void*,void* p){free(p);}
static int present(void*,const CpView* v){
 ++presentations;kind=v->kind;
 assert(v->size==sizeof(*v) && v->row_count<=CP_APP_MAX_ROWS);
 labels.clear();labels.reserve(v->row_count);
 for(uint32_t i=0;i<v->row_count;++i) {
  assert(v->rows[i].label);
  labels.emplace_back(v->rows[i].label);
 }
 return 1;
}
static int edit(void*,int32_t,const char*,const char*,size_t){++edits;return 1;}
static int request(void*,int32_t,CpRequestKind,const char*,const uint8_t*,size_t){++requests;return acceptRequest;}
static uint32_t parse(void*,const char*,const char*,size_t){return 1;}
static const char* field(void*,uint32_t,uint32_t f,uint32_t){return f==0?"Title":f==1?"Body":"https://example.com/";}
static uint32_t links(void*,uint32_t){return 1;}
static void releaseDoc(void*,uint32_t){++releases;}
static int resolve(void*,const char*,const char* u,char* o,size_t n){if(strncmp(u,"https://",8))return 0;if(strlen(u)>=n)return 0;strcpy(o,u);return 1;}
static int read(void*,const char*,void*,size_t,size_t*){return 0;}
static int write(void*,const char*,const void*,size_t){return 1;}
static int list(void*,char* o,size_t){o[0]=0;return 1;}
static int cacheWrite(void*,const char*,const char*,size_t,const char*){return 1;}
static uint32_t now(void*){return 1;}
static void cancel(void*){}
int main(){
 CpHostV1 h{};h.size=sizeof(h);h.version=1;h.allocate=alloc;h.release=release;h.present=present;h.edit=edit;
 h.request=request;h.parse_html=parse;h.document_field=field;h.document_links=links;h.release_document=releaseDoc;
 h.resolve_url=resolve;h.read_file=read;h.write_file=write;h.cache_list=list;h.cache_write=cacheWrite;h.milliseconds=now;h.cancel=cancel;
 CpBootstrap b{sizeof(b),1,{}};char* argv[]={reinterpret_cast<char*>(&b)};
 assert(browser_entry(1,argv)==0);assert(b.module.version==1);
 void* state=nullptr;assert(b.module.start(&h,&state));assert(state && presentations==1);assert(labels.at(0)=="Open URL");
 CpEvent e{sizeof(e),CP_SELECT,1,0,nullptr,0,nullptr};b.module.event(state,&e);assert(edits==1);
 e={sizeof(e),CP_EDITED,1,0,"https://example.com/",20,nullptr};b.module.event(state,&e);assert(requests==1);assert(labels.at(0)=="Loading - Back cancels");
 e={sizeof(e),CP_HTTP_DONE,1,200,"<p>Body</p>",11,nullptr};b.module.event(state,&e);assert(kind==CP_READING);
 e={sizeof(e),CP_BACK,0,0,nullptr,0,nullptr};b.module.event(state,&e);assert(kind==CP_LIST);
 // A stale completion must neither replace the document nor initiate traffic.
 e={sizeof(e),CP_HTTP_DONE,1,200,"stale",5,nullptr};b.module.event(state,&e);assert(kind==CP_LIST && requests==1);
 // Editor cancellation and rejected input leave copied rows readable.
 e={sizeof(e),CP_EDITED,1,-1,nullptr,0,nullptr};b.module.event(state,&e);
 assert(labels.at(0)=="Open URL");
 e={sizeof(e),CP_EDITED,1,0,"not a URL",9,nullptr};b.module.event(state,&e);
 assert(requests==1 && labels.at(0)=="Open URL");
 acceptRequest=false;
 e={sizeof(e),CP_EDITED,1,0,"https://example.org/",20,nullptr};b.module.event(state,&e);
 assert(requests==2 && labels.at(0)=="Open URL");
 b.module.stop(state);assert(releases==1);
}
