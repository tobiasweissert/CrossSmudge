#include "AppAbi.h"
#include "PresentationValidation.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
extern "C" int chat_entry(int,char**);
static unsigned messages=0,requests=0,records=0;static int32_t active=0;static std::string draft,last;
static int present(void*,const CpPresentation* v){assert(sdapps::presentation::validPresentation(v));messages=v->message_count;draft=v->draft?v->draft:"";if(messages)last=v->messages[messages-1].text;return 1;}
int main(){
 CpHostV2 h{};h.v1.size=sizeof(h);h.v1.version=2;h.v1.allocate=[](void*,size_t n){return calloc(1,n);};h.v1.release=[](void*,void* p){free(p);};h.present_v2=present;
 h.v1.request=[](void*,int32_t id,CpRequestKind kind,const char*,const uint8_t*,size_t){assert(kind==CP_PROVIDER_CHAT);++requests;active=id;return 1;};
 h.v1.record=[](void*,int32_t id,int start){active=id;if(start)++records;return 1;};h.v1.edit=[](void*,int32_t,const char*,const char*,size_t){return 1;};
 CpBootstrap b{sizeof(b),2,{}};char* argv[]={reinterpret_cast<char*>(&b)};assert(chat_entry(1,argv)==0&&b.module.version==2);
 void* state=nullptr;assert(b.module.start(&h.v1,&state));assert(messages==0);
 auto event=[&](CpEventKind kind,int id,int status,const char* text){CpEvent e{sizeof(e),kind,id,status,text,text?strlen(text):0,nullptr};b.module.event(state,&e);};
 event(CP_HOLD_START,5,0,nullptr);event(CP_HOLD_END,5,0,nullptr);assert(records==1&&requests==0);
 event(CP_HTTP_DONE,active,200,"spoken draft");assert(draft=="spoken draft"&&requests==0);
 event(CP_SELECT,2,0,nullptr);assert(requests==1&&messages==1&&last=="spoken draft");
 event(CP_HTTP_DONE,active,200,"first reply");assert(messages==2&&last=="first reply"&&draft.empty());
 event(CP_EDITED,1,0,"second question");event(CP_SELECT,2,0,nullptr);assert(messages==3&&requests==2);
 event(CP_HTTP_DONE,active,200,"second reply");assert(messages==4&&last=="second reply");
 b.module.stop(state);
 CpPresentation v{};v.size=sizeof(v);v.kind=CP_CHAT_V2;assert(sdapps::presentation::validPresentation(&v));
 v.draft="\xc0\x80";assert(!sdapps::presentation::validPresentation(&v));v.draft="";v.kind=(CpPresentationKind)99;assert(!sdapps::presentation::validPresentation(&v));
}
