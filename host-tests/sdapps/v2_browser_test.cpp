#define main legacy_browser_test
#include "browser_test.cpp"
#undef main
#include "PresentationValidation.h"
static uint32_t documentId;static bool busy;static std::string address;
static int typed(void*,const CpPresentation* v){assert(sdapps::presentation::validPresentation(v));assert(v->kind==CP_BROWSER_V2);documentId=v->document;busy=v->busy;address=v->address?v->address:"";return 1;}
int main(){
 CpHostV2 h{};h.v1.size=sizeof(h);h.v1.version=2;h.v1.allocate=alloc;h.v1.release=release;h.v1.present=present;h.present_v2=typed;h.v1.edit=edit;
 h.v1.request=request;h.v1.parse_html=parse;h.v1.document_field=field;h.v1.document_links=links;h.v1.release_document=releaseDoc;
 h.v1.resolve_url=resolve;h.v1.read_file=read;h.v1.write_file=write;h.v1.cache_list=list;h.v1.cache_write=cacheWrite;h.v1.milliseconds=now;h.v1.cancel=cancel;
 CpBootstrap b{sizeof(b),2,{}};char* argv[]={reinterpret_cast<char*>(&b)};assert(browser_entry(1,argv)==0);void* state=nullptr;assert(b.module.start(&h.v1,&state));assert(!busy&&!documentId);
 auto event=[&](CpEventKind kind,int id,int status,const char* text){CpEvent e{sizeof(e),kind,id,status,text,text?strlen(text):0,nullptr};b.module.event(state,&e);};
 event(CP_EDITED,1,0,"https://example.com/");assert(busy);event(CP_HTTP_DONE,1,200,"<p>Body</p>");assert(!busy&&documentId==1&&address=="https://example.com/");
 event(CP_SELECT,10,0,nullptr);assert(kind==CP_LIST);
 event(CP_EDITED,1,0,"https://example.org/");assert(busy);event(CP_HTTP_DONE,2,500,"error");assert(!busy&&documentId==1&&address=="https://example.com/");
 event(CP_SELECT,1000,0,nullptr);assert(busy);event(CP_BACK,0,0,nullptr);assert(!busy&&documentId==1);
 event(CP_HTTP_DONE,3,200,"stale");assert(documentId==1&&!busy);b.module.stop(state);
 return 0;
}
