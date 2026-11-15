#include "AppAbi.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
extern "C" int chat_entry(int,char**);
static int requests,records,stops,edits,lastId;
static char draft[8193];
static void* alloc(void*,size_t n){return calloc(1,n);}
static void release(void*,void* p){free(p);}
static int present(void*,const CpView* v){if(v->draft){assert(strlen(v->draft)<sizeof(draft));strcpy(draft,v->draft);}return 1;}
static int edit(void*,int32_t,const char*,const char*,size_t){++edits;return 1;}
static int request(void*,int32_t id,CpRequestKind kind,const char*,const uint8_t* b,size_t n){assert(kind==CP_PROVIDER_CHAT);assert(n && n<=32768);assert(strstr((const char*)b,"gpt-4.1-mini"));assert(strstr((const char*)b,"\"store\":false"));++requests;lastId=id;return 1;}
static int record(void*,int32_t id,int start){if(start){++records;lastId=id;}else ++stops;return 1;}
static void cancel(void*){}
int main(){
 CpHostV1 h{};h.size=sizeof(h);h.version=1;h.allocate=alloc;h.release=release;h.present=present;
 h.edit=edit;h.request=request;h.record=record;h.cancel=cancel;
 CpBootstrap b{sizeof(b),1,{}};char* argv[]={reinterpret_cast<char*>(&b)};assert(chat_entry(1,argv)==0);
 void* state=nullptr;assert(b.module.start(&h,&state));
 CpEvent e{sizeof(e),CP_HOLD_START,5,0,nullptr,0,nullptr};b.module.event(state,&e);assert(records==1);
 e.kind=CP_HOLD_END;b.module.event(state,&e);assert(stops==1 && !requests);
 e={sizeof(e),CP_HTTP_DONE,lastId,200,"review me",9,nullptr};b.module.event(state,&e);
 assert(edits==1 && requests==0 && !strcmp(draft,"review me"));
 e={sizeof(e),CP_EDITED,1,0,"reviewed",8,nullptr};b.module.event(state,&e);assert(!requests);
 e={sizeof(e),CP_SELECT,2,0,nullptr,0,nullptr};b.module.event(state,&e);assert(requests==1);
 e={sizeof(e),CP_HTTP_DONE,lastId+1,200,"stale",5,nullptr};b.module.event(state,&e);assert(!strcmp(draft,"reviewed"));
 e.id=lastId;e.text="answer";e.length=6;b.module.event(state,&e);assert(draft[0]==0);
 b.module.stop(state);
}
