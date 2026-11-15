#include "AppAbi.h"
#include "../common/Text.h"
#define URL_CAP 2049
#define HISTORY 16
#define BOOKMARKS 32
#define CACHED 256

typedef struct { uint32_t document; char url[URL_CAP]; } Visit;
typedef struct {
  const CpHostV1* host;
  CpRow rows[CP_APP_MAX_ROWS];
  Visit history[HISTORY];
  char bookmarks[BOOKMARKS][URL_CAP];
  char cached[CACHED][URL_CAP];
  char scratch[CACHED * URL_CAP + 1];
  char pending[URL_CAP];
  char status[160];
  unsigned count,index,bookmark_count,cache_count,redirects;
  int32_t sequence,active;
  int reading,confirm_clear;
} Browser;
static const char* field(Browser* s,unsigned type,unsigned index) {
  if(!s->count)return "";
  return s->host->document_field(s->host->context,s->history[s->index].document,type,index);
}
static void add(Browser* s,unsigned* count,int id,const char* label) {
  if(*count>=CP_APP_MAX_ROWS)return;
  s->rows[*count]=(CpRow){id,label,0};++*count;
}
static void show(Browser* s) {
  if(s->host->version==CP_APP_ABI_V2 && !s->confirm_clear && (s->reading || s->active)) {
    CpPresentation v={0};v.size=sizeof(v);v.kind=CP_BROWSER_V2;
    v.document=s->count?s->history[s->index].document:0;v.identity=v.document;
    v.title="Browser";v.address=s->count?s->history[s->index].url:s->pending;
    v.status=s->status;v.busy=s->active!=0;v.back=4;v.address_action=1;v.menu=10;v.link_base=1000;
    ((const CpHostV2*)s->host)->present_v2(s->host->context,&v);return;
  }
  unsigned n=0;
  if(s->confirm_clear) {
    add(s,&n,8,"Delete browser data");add(s,&n,9,"Keep browser data");
  } else {
    add(s,&n,1,s->active?"Loading - Back cancels":"Open URL");add(s,&n,2,"Wi-Fi connection");
    add(s,&n,3,"Read current page");add(s,&n,4,"Previous page");add(s,&n,5,"Forward");
    add(s,&n,6,"Add / remove current bookmark");add(s,&n,7,"Clear browser data");
    if(s->count) {
      unsigned links=s->host->document_links(s->host->context,s->history[s->index].document);
      if(links>128)links=128;
      for(unsigned i=0;i<links;++i)add(s,&n,1000+(int)i,field(s,2,i));
    }
    if(s->host->version==CP_APP_ABI_V2)for(unsigned i=0;i<s->count;++i)add(s,&n,4000+(int)i,s->history[i].url);
    for(unsigned i=0;i<s->bookmark_count;++i)add(s,&n,2000+(int)i,s->bookmarks[i]);
    for(unsigned i=0;i<s->cache_count;++i)add(s,&n,3000+(int)i,s->cached[i]);
  }
  CpView view={sizeof(view),s->reading?CP_READING:CP_LIST,s->reading?field(s,0,0):"Browser",s->status,
               s->reading?field(s,1,0):"",0,0,s->rows,n};
  s->host->present(s->host->context,&view);
}
static void status(Browser* s,const char* message) { if(!cp_copy(s->status,sizeof(s->status),message))cp_copy(s->status,sizeof(s->status),"Operation failed"); }
static void refresh_cache(Browser* s) {
  s->cache_count=0;s->scratch[0]=0;
  if(!s->host->cache_list(s->host->context,s->scratch,sizeof(s->scratch)))return;
  size_t offset=0;
  while(offset<sizeof(s->scratch) && s->scratch[offset] && s->cache_count<CACHED) {
    size_t begin=offset;
    while(offset<sizeof(s->scratch) && s->scratch[offset] && s->scratch[offset]!='\n')++offset;
    if(offset==sizeof(s->scratch) || offset-begin>=URL_CAP)break;
    int more=s->scratch[offset]=='\n';s->scratch[offset]=0;
    if(s->host->resolve_url(s->host->context,"",s->scratch+begin,s->cached[s->cache_count],URL_CAP))++s->cache_count;
    if(!more)break;
    ++offset;
  }
}
static void publish(Browser* s,uint32_t document,const char* url) {
  while(s->count && s->count>s->index+1) {
    --s->count;s->host->release_document(s->host->context,s->history[s->count].document);
  }
  if(s->count==HISTORY) {
    s->host->release_document(s->host->context,s->history[0].document);
    memmove(s->history,s->history+1,sizeof(Visit)*(HISTORY-1));--s->count;
  }
  s->index=s->count++;
  s->history[s->index].document=document;
  cp_copy(s->history[s->index].url,URL_CAP,url);
  s->reading=1;
}
static void navigate(Browser* s,const char* url) {
  if(s->active)return;
  if(!s->host->resolve_url(s->host->context,"",url,s->pending,sizeof(s->pending))) {
    status(s,"Only public HTTPS URLs are supported");show(s);return;
  }
  if(s->sequence==INT32_MAX)s->sequence=0;
  s->active=++s->sequence;s->redirects=0;s->reading=s->host->version==CP_APP_ABI_V2;
  status(s,"Verifying TLS and downloading (32 KiB maximum)");
  if(!s->host->request(s->host->context,s->active,CP_GET,s->pending,0,0)) {
    s->active=0;status(s,"Could not start download; connect Wi-Fi and retry");
  }
  show(s);
}
static void bookmark(Browser* s) {
  if(!s->count)return;
  const char* url=s->history[s->index].url;
  unsigned i=0;while(i<s->bookmark_count && strcmp(s->bookmarks[i],url))++i;
  if(i<s->bookmark_count) {
    memmove(s->bookmarks[i],s->bookmarks[i+1],(s->bookmark_count-i-1)*URL_CAP);
    --s->bookmark_count;memset(s->bookmarks[s->bookmark_count],0,URL_CAP);
  } else if(s->bookmark_count<BOOKMARKS) {
    cp_copy(s->bookmarks[s->bookmark_count++],URL_CAP,url);
  } else {status(s,"Bookmarks full (32); remove one first");return;}
  status(s,s->host->write_file(s->host->context,"bookmarks.bin",s->bookmarks,sizeof(s->bookmarks))?
         "Bookmarks saved":"Bookmark write failed; changes are only in RAM");
}
static void clear(Browser* s) {
  memset(s->bookmarks,0,sizeof(s->bookmarks));s->bookmark_count=0;
  int ok=s->host->write_file(s->host->context,"bookmarks.bin",s->bookmarks,sizeof(s->bookmarks));
  ok=s->host->cache_clear(s->host->context)&&ok;
  for(unsigned i=0;i<s->count;++i)s->host->release_document(s->host->context,s->history[i].document);
  s->count=s->index=0;s->reading=s->confirm_clear=0;refresh_cache(s);
  status(s,ok?"Browser data cleared (not secure erasure)":"Some data could not be cleared; check storage");
}
static int start(const CpHostV1* h,void** out) {
  if(!h || !((h->size==sizeof(*h) && h->version==CP_APP_ABI_V1) || (h->size==sizeof(CpHostV2) && h->version==CP_APP_ABI_V2)) || !out)return 0;
  Browser* s=h->allocate(h->context,sizeof(*s));if(!s)return 0;
  memset(s,0,sizeof(*s));s->host=h;*out=s;
  size_t size=0;
  if(h->read_file(h->context,"bookmarks.bin",s->bookmarks,sizeof(s->bookmarks),&size) && size==sizeof(s->bookmarks)) {
    for(unsigned i=0;i<BOOKMARKS;++i) {
      if(cp_length(s->bookmarks[i],URL_CAP)==URL_CAP)continue;
      if(h->resolve_url(h->context,"",s->bookmarks[i],s->pending,URL_CAP))
        cp_copy(s->bookmarks[s->bookmark_count++],URL_CAP,s->pending);
    }
  }
  for(unsigned i=s->bookmark_count;i<BOOKMARKS;++i)memset(s->bookmarks[i],0,URL_CAP);
  refresh_cache(s);status(s,"Public HTTPS; bounded text pages, no scripts");s->reading=h->version==CP_APP_ABI_V2;show(s);return 1;
}
static void event(void* state,const CpEvent* e) {
  Browser* s=state;const CpHostV1* h=s->host;
  if(!e || e->size!=sizeof(*e))return;
  if(e->kind==CP_BACK || e->kind==CP_CANCELLED) {
    if(s->active){h->cancel(h->context);s->active=0;s->reading=s->host->version==CP_APP_ABI_V2;status(s,"Cancelled - previous page preserved");}
    else if(s->reading)s->reading=0;
    else if(s->confirm_clear)s->confirm_clear=0;
    else if(s->index)--s->index;
    else {h->leave(h->context);return;}
    show(s);return;
  }
  if(e->kind==CP_EDITED && e->id==1 && e->status==0) {navigate(s,e->text);return;}
  if(e->kind==CP_WIFI_DONE){status(s,e->status?"Wi-Fi connected":"Wi-Fi not connected");show(s);return;}
  if(e->kind==CP_HTTP_DONE) {
    if(!s->active || e->id!=s->active)return;
    if(e->status>=300 && e->status<400 && e->location && s->redirects<3) {
      if(h->resolve_url(h->context,s->pending,e->location,s->scratch,sizeof(s->scratch)) &&
         cp_copy(s->pending,sizeof(s->pending),s->scratch)) {
        ++s->redirects;
        if(h->request(h->context,s->active,CP_GET,s->pending,0,0))return;
      }
    }
    s->active=0;
    if(e->status!=200 || !e->text || e->length>32768) {status(s,"Download failed - previous page preserved");show(s);return;}
    uint32_t doc=h->parse_html(h->context,s->pending,e->text,e->length);
    if(!doc){status(s,"Unsupported page or navigation memory budget exceeded");show(s);return;}
    const char* protected_url=s->count?s->history[s->index].url:"";
    int saved=h->cache_write(h->context,s->pending,e->text,e->length,protected_url);
    publish(s,doc,s->pending);refresh_cache(s);status(s,saved?"Page loaded":"Page loaded; SD cache write failed");show(s);return;
  }
  if(e->kind!=CP_SELECT || s->active)return;
  switch(e->id) {
    case 1:h->edit(h->context,1,"HTTPS URL",s->count?s->history[s->index].url:"https://",2048);break;
    case 2:h->wifi(h->context);break;
    case 3:if(s->count)s->reading=1;break;
    case 4:if(s->index)--s->index;s->reading=h->version==CP_APP_ABI_V2;break;
    case 10:s->reading=0;break;
    case 5:if(s->index+1<s->count)++s->index;break;
    case 6:bookmark(s);break;
    case 7:s->confirm_clear=1;status(s,"Delete cache, bookmarks and history? Not secure erasure.");break;
    case 8:if(s->confirm_clear)clear(s);break;
    case 9:s->confirm_clear=0;break;
    default:
      if(e->id>=4000 && (unsigned)(e->id-4000)<s->count){s->index=e->id-4000;s->reading=1;}
      else if(e->id>=1000 && e->id<1128 && s->count && (unsigned)(e->id-1000)<h->document_links(h->context,s->history[s->index].document))
        navigate(s,field(s,3,e->id-1000));
      else if(e->id>=2000 && (unsigned)(e->id-2000)<s->bookmark_count)navigate(s,s->bookmarks[e->id-2000]);
      else if(e->id>=3000 && (unsigned)(e->id-3000)<s->cache_count) {
        size_t length=0;const char* url=s->cached[e->id-3000];
        if(h->cache_read(h->context,url,s->scratch,sizeof(s->scratch),&length)) {
          uint32_t doc=h->parse_html(h->context,url,s->scratch,length);
          if(doc){publish(s,doc,url);status(s,"Offline SD copy - may be stale");}
          else status(s,"Offline copy invalid; no network request made");
        } else status(s,"Offline copy missing or corrupt; no network request made");
      }
      break;
  }
  show(s);
}
static void stop(void* state) {
  Browser* s=state;const CpHostV1* h=s->host;
  for(unsigned i=0;i<s->count;++i)h->release_document(h->context,s->history[i].document);
  h->release(h->context,s);
}
int app_main(int argc,char** argv) {
  if(argc!=1 || !argv || !argv[0])return -1;
  CpBootstrap* b=(CpBootstrap*)argv[0];
  if(b->size!=sizeof(*b) || (b->version!=CP_APP_ABI_V1 && b->version!=CP_APP_ABI_V2))return -1;
  b->module=(CpModuleV1){sizeof(CpModuleV1),b->version,start,event,stop};return 0;
}
