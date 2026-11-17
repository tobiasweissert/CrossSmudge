#include "HttpCore.h"
#include "HttpGzip.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>

namespace ink::http {
namespace {
bool asciiAlpha(char c) { return (c >= 'a' && c <= 'z'); }
bool digit(char c) { return c >= '0' && c <= '9'; }
bool equalFold(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i=0;i<a.size();++i) {
    char c=a[i]; if(c>='A' && c<='Z') c+=32;
    char d=b[i]; if(d>='A' && d<='Z') d+=32;
    if(c!=d) return false;
  }
  return true;
}
bool decimal(std::string_view s, size_t& n, unsigned base=10) {
  if(s.empty()) return false;
  n=0;
  for(char c:s) {
    unsigned d = digit(c) ? c-'0' : c>='a' && c<='f' ? c-'a'+10 : c>='A' && c<='F' ? c-'A'+10 : 99;
    if(d>=base || n>(std::numeric_limits<size_t>::max()-d)/base) return false;
    n=n*base+d;
  }
  return true;
}
}
bool validHost(std::string_view h) {
  if(h.size()<3 || h.size()>=128 || h.find('.')==h.npos) return false;
  size_t label=0; bool alpha=false;
  for(size_t i=0;i<h.size();++i) {
    char c=h[i];
    if(c=='.') { if(!label || h[i-1]=='-') return false; label=0; }
    else { if(!asciiAlpha(c) && !digit(c) && c!='-') return false;
      if(!label && c=='-') return false;
      if(++label>63) return false;
      alpha |= asciiAlpha(c);
    }
  }
  // Alphabetic TLD excludes alternate numeric IP forms and local bare names.
  auto tld=h.substr(h.rfind('.')+1);
  for(char c:tld) if(!asciiAlpha(c)) return false;
  return label && h.back()!='-' && alpha && tld.size()>=2;
}
bool parseUrl(std::string_view s, Url& u) {
  if(s.size()>512 || s.substr(0,8)!="https://") return false;
  s.remove_prefix(8);
  auto slash=s.find_first_of("/?#");
  auto h=s.substr(0,slash);
  if(!validHost(h)) return false;
  auto p=slash==s.npos ? std::string_view("/") : s.substr(slash);
  const size_t prefix=p.front()=='/' ? 0 : 1;
  if(p.size()+prefix>=sizeof(u.path)) return false;
  for(size_t i=0;i<p.size();++i) {
    unsigned char c=p[i];
    if(c<33 || c>126 || c=='\\' || c=='#') return false;
    if(c=='%') {
      if(i+2>=p.size()) return false;
      size_t v; if(!decimal(p.substr(i+1,2),v,16) || v<32 || v==127) return false;
      i+=2;
    }
  }
  memcpy(u.host,h.data(),h.size()); u.host[h.size()]=0;
  if(prefix)u.path[0]='/';
  memcpy(u.path+prefix,p.data(),p.size()); u.path[p.size()+prefix]=0;
  return true;
}
bool resolveRedirect(const Url& base,std::string_view reference,Url& result){
  // A bounded URL scratch (<1 KiB total with path normalization) is justified
  // here to avoid a fallible heap allocation while following an untrusted hop.
  char full[513];int n=-1;
  if(reference.size()>512)return false;
  if(reference.substr(0,8)=="https://")n=snprintf(full,sizeof(full),"%.*s",int(reference.size()),reference.data());
  else if(reference.substr(0,2)=="//")n=snprintf(full,sizeof(full),"https:%.*s",int(reference.size()),reference.data());
  else if(!reference.empty() && reference.front()=='/')n=snprintf(full,sizeof(full),"https://%s%.*s",base.host,int(reference.size()),reference.data());
  else if(reference.find(':')==reference.npos || reference.find(':')>reference.find_first_of("/?#")){
    std::string_view path(base.path);
    if(!reference.empty() && reference.front()!='#'){
      path=path.substr(0,path.find('?'));
      if(reference.front()!='?')path=path.substr(0,path.rfind('/')+1);
    }
    n=snprintf(full,sizeof(full),"https://%s%.*s%.*s",base.host,int(path.size()),path.data(),int(reference.size()),reference.data());
  }
  if(n<0 || size_t(n)>=sizeof(full))return false;
  if(char* hash=strchr(full,'#'))*hash=0;
  if(!parseUrl(full,result))return false;
  char original[385];memcpy(original,result.path,sizeof(original));
  auto query=strchr(original,'?');size_t pathLength=query?size_t(query-original):strlen(original);
  std::string_view input(original,pathLength);size_t used=0;
  auto removeLast=[&](){while(used && result.path[used-1]!='/')--used;if(used)--used;};
  while(!input.empty()){
    if(input.substr(0,3)=="../")input.remove_prefix(3);
    else if(input.substr(0,2)=="./")input.remove_prefix(2);
    else if(input.substr(0,3)=="/./")input.remove_prefix(2);
    else if(input=="/."){input={};result.path[used++]='/';}
    else if(input.substr(0,4)=="/../"){input.remove_prefix(3);removeLast();}
    else if(input=="/.."){input={};removeLast();result.path[used++]='/';}
    else if(input=="." || input=="..")input={};
    else{size_t end=input.find('/',input.front()=='/'?1:0);if(end==input.npos)end=input.size();memcpy(result.path+used,input.data(),end);used+=end;input.remove_prefix(end);}
  }
  if(!used)result.path[used++]='/';
  if(query){size_t count=strlen(query);memcpy(result.path+used,query,count);used+=count;}
  result.path[used]=0;return true;
}
bool Domains::contains(const char* h) const {
  if(publicHttps && validHost(h)) return true;
  for(size_t i=0;i<count;++i) if(!strcmp(names[i],h)) return true;
  return false;
}
bool Domains::add(std::string_view h) {
  if(count>=MaxDomains || !validHost(h)) return false;
  for(size_t i=0;i<count;++i) if(h==names[i]) return false;
  memcpy(names[count],h.data(),h.size()); names[count++][h.size()]=0; return true;
}
bool publicV4(uint32_t ip) {
  const unsigned a=ip>>24,b=(ip>>16)&255;
  if(a==0 || a==10 || a==127 || a>=224 || (a==100 && b>=64 && b<=127) ||
     (a==169 && b==254) || (a==172 && b>=16 && b<=31) || (a==192 && b==168) ||
     (a==198 && (b==18 || b==19))) return false;
  // Conservatively exclude all IANA special-use 192.0/16 and documentation/relay blocks.
  if((ip&0xffff0000U)==0xc0000000U || (ip&0xffffff00U)==0xc0586300U ||
     (ip&0xffffff00U)==0xc6336400U || (ip&0xffffff00U)==0xcb007100U) return false;
  return true;
}
bool safeHeader(std::string_view n,std::string_view v) {
  if(n.empty() || n.size()>32 || v.size()>128) return false;
  if(!equalFold(n,"accept") && !equalFold(n,"content-type") && !equalFold(n,"x-request-id")) return false;
  for(unsigned char c:v) if(c<32 || c>126) return false;
  return true;
}
bool backendPath(std::string_view path) {
  for (auto allowed : {"/v1/open", "/v1/poll", "/v1/chunk", "/v1/locate", "/v1/find", "/v1/image", "/v1/cancel"})
    if (path == allowed) return true;
  return false;
}
bool backendToken(std::string_view token) {
  if (token.size()!=64) return false;
  for (char c:token) if (!((c>='0' && c<='9') || (c>='a' && c<='f'))) return false;
  return true;
}
static bool utf8Body(std::string_view s) {
 for(size_t i=0;i<s.size();){
  unsigned char c=s[i++];if(c<128)continue;
  unsigned count=0;uint32_t cp=0,min=0;
  if(c>=0xc2 && c<=0xdf){count=1;cp=c&31;min=0x80;}
  else if(c>=0xe0 && c<=0xef){count=2;cp=c&15;min=0x800;}
  else if(c>=0xf0 && c<=0xf4){count=3;cp=c&7;min=0x10000;}else return false;
  if(count>s.size()-i)return false;
  while(count--){unsigned char b=s[i++];if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&63);}
  if(cp<min || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))return false;
 }
 return true;
}
bool prepareBackend(Request& q, std::string_view path, std::string_view body) {
  if (!backendPath(path) || body.empty() || body.size()>MaxUpload || body.find('\0')!=body.npos || !utf8Body(body)) return false;
  q.backend=true;q.browser=true;q.post=true;q.headerCount=0;
  strcpy(q.url.host,BackendHost);memcpy(q.url.path,path.data(),path.size());q.url.path[path.size()]=0;
  memcpy(q.body,body.data(),body.size());q.bodySize=body.size();
  q.authorization[0]=0;
  q.limit=path=="/v1/image"?65536:49152;
  return true;
}
bool safeRelativePath(std::string_view s) {
  if(s.empty() || s.size()>240) return false;
  size_t start=0;
  for(size_t i=0;i<=s.size();++i) {
    if(i==s.size() || s[i]=='/') {
      if(i==start || s[start]=='.') return false;
      start=i+1;
    } else { unsigned char c=s[i]; if(c<32 || c>=127 || c=='\\' || c==':') return false; }
  }
  return true;
}
bool resolveAppPath(std::string_view path, std::string_view appRoot, std::string_view saveRoot, std::string& out) {
  auto relative = path;
  if (!relative.empty() && relative.front() == '/') {
    bool matched = false;
    for (auto root : {appRoot, saveRoot}) {
      if (relative.size() > root.size() && relative.substr(0, root.size()) == root && relative[root.size()] == '/') {
        relative.remove_prefix(root.size() + 1);
        matched = true;
        break;
      }
    }
    if (!matched || !safeRelativePath(relative)) return false;
    out.assign(path);
    return true;
  }
  if (!safeRelativePath(relative)) return false;
  out.assign(appRoot);
  out += '/';
  out.append(path);
  return true;
}
bool resolveAppDirectory(std::string_view path,std::string_view appRoot,std::string_view saveRoot,std::string& out){
  if(path==appRoot || path==saveRoot){out.assign(path);return true;}
  return resolveAppPath(path,appRoot,saveRoot,out);
}
static void exchangeOne(const Request& q,Result& r,Scratch& scratch,Io& io,const std::atomic<bool>& cancel, const Url& url, uint32_t timeout) {
  r.status=0; r.size=0; r.error=nullptr; r.location[0]=0; r.contentType[0]=0;
  const uint32_t start=io.millis(io.context);
  auto ok=[&]() {
    if(cancel.load()) {r.error="cancelled";return false;}
    if(uint32_t(io.millis(io.context)-start)>=timeout) {r.error="timeout";return false;}
    return true;
  };
  auto fail=[&](const char* e) {if(!r.error) r.error=e;};
  if(q.backend && (strcmp(url.host,BackendHost) || !backendPath(url.path) || !q.post || q.headerCount || q.authorization[0])){fail("backend_invalid");return;}
  if(!ok()) return;
  if(!io.connect(io.context,url.host,timeout,cancel)) {fail("tls_or_network");io.close(io.context);return;}
  struct Close {Io& io; ~Close(){io.close(io.context);}} close{io};
  auto send=[&](const char* p,size_t n) {
    while(n && ok()) {int w=io.write(io.context,p,n);if(w<=0 || size_t(w)>n){fail("write");return false;}p+=w;n-=w;}
    return n==0 && ok();
  };
  int n=snprintf(scratch.line,sizeof(scratch.line),"%s %s HTTP/1.1\r\n",q.post?"POST":"GET",url.path);
  if(n<0 || size_t(n)>=sizeof(scratch.line)) {fail("request_limit");return;}
  if(!send(scratch.line,n)) return;
  n=snprintf(scratch.line,sizeof(scratch.line),"Host: %s\r\n",url.host);
  if(!send(scratch.line,n))return;
  const char* connection=q.browser?"Connection: close\r\nAccept-Encoding: gzip, identity\r\n":"Connection: close\r\nAccept-Encoding: identity\r\n";
  if(!send(connection,strlen(connection)))return;
  if(q.backend){
    const char* accept=!strcmp(q.url.path,"/v1/image")?"Accept: image/png\r\n":"Accept: application/json\r\n";
    if(!send(accept,strlen(accept)))return;
    if(!send("Content-Type: application/json\r\n",strlen("Content-Type: application/json\r\n")))return;
  }
  for(size_t i=0;i<q.headerCount;++i) {
    n=snprintf(scratch.line,sizeof(scratch.line),"%s: %s\r\n",q.headers[i].name,q.headers[i].value);
    if(!send(scratch.line,n)) return;
  }
  n=snprintf(scratch.line,sizeof(scratch.line),"Content-Length: %zu\r\n\r\n",q.bodySize);
  if(!send(scratch.line,n) || !send(q.body,q.bodySize)) return;
  auto read=[&](char* p,size_t size) {if(!ok()) return -1; int got=io.read(io.context,p,size); if(got<0) fail("read"); return got;};
  auto line=[&]() {
    size_t pos=0;
    while(pos<sizeof(scratch.line)-1) {
      char c; if(read(&c,1)!=1) {fail("truncated");return false;}
      if(++scratch.headerBytes>4096){fail("header_limit");return false;}
      if(c=='\n') {
        if(!pos || scratch.line[pos-1]!='\r'){fail("protocol");return false;}
        scratch.line[pos-1]=0;return true;
      }
      if(c==0 || (static_cast<unsigned char>(c)<32 && c!='\r' && c!='\t')){fail("protocol");return false;}
      scratch.line[pos++]=c;
    }
    fail("header_limit");return false;
  };
  scratch.headerBytes=0;
  if(!line()) return;
  std::string_view status(scratch.line);
  if(status.size()<12 || (status.substr(0,9)!="HTTP/1.1 " && status.substr(0,9)!="HTTP/1.0 ") ||
     !digit(status[9]) || !digit(status[10]) || !digit(status[11]) || (status.size()>12 && status[12]!=' ')) {fail("protocol");return;}
  r.status=(status[9]-'0')*100+(status[10]-'0')*10+status[11]-'0';
  if(r.status<200 || r.status>599) {fail("protocol");return;}
  size_t length=0;bool hasLength=false,chunked=false,compressed=false,encodingSeen=false,contentTypeSeen=false;
  while(line()) {
    std::string_view s(scratch.line); if(s.empty()) break;
    auto colon=s.find(':'); if(colon==s.npos || colon==0 || s[0]==' ' || s[0]=='\t') {fail("protocol");return;}
    auto name=s.substr(0,colon),value=s.substr(colon+1);
    while(!value.empty() && (value.front()==' ' || value.front()=='\t'))value.remove_prefix(1);
    while(!value.empty() && (value.back()==' ' || value.back()=='\t'))value.remove_suffix(1);
    if(q.backend && (equalFold(name,"location") || equalFold(name,"set-cookie") || equalFold(name,"cookie"))){
      fail(r.status>=300 && r.status<=399?"redirect_denied":"backend_headers");return;
    }
    if(q.backend && (name.back()==' ' || name.back()=='\t')){fail("protocol");return;}
    if(equalFold(name,"content-length")) {
      if(hasLength || !decimal(value,length)){fail("protocol");return;} hasLength=true;
      if(length>(q.browser?q.limit:r.capacity)){fail("body_limit");return;}
    } else if(equalFold(name,"transfer-encoding")) {
      if(chunked || !equalFold(value,"chunked")){fail("protocol");return;}chunked=true;
    } else if(q.browser && equalFold(name,"location")) {
      if(r.location[0] || value.size()>=sizeof(r.location)){fail("redirect_invalid");return;}
      memcpy(r.location,value.data(),value.size());r.location[value.size()]=0;
    } else if(q.browser && equalFold(name,"content-type")) {
      if(q.backend && contentTypeSeen){fail("protocol");return;}contentTypeSeen=true;
      if(value.size()>=sizeof(r.contentType)){fail("header_limit");return;}
      memcpy(r.contentType,value.data(),value.size());r.contentType[value.size()]=0;
    } else if(equalFold(name,"content-encoding")) {
      if(q.browser && encodingSeen){fail("encoding");return;}encodingSeen=true;
      if(!equalFold(value,"identity")){
        if(q.browser && equalFold(value,"gzip"))compressed=true;
        else{fail("encoding");return;}
      }
    }
  }
  if(r.error) return;
  if(chunked && hasLength){fail("protocol");return;}
  if(r.status>=300 && r.status<=399) {if(!q.browser || q.backend)fail("redirect_denied");return;}
  if(q.backend){
    const bool image=!strcmp(q.url.path,"/v1/image") && r.status==200;
    const size_t cap=r.status>=400?2048:image?65536:49152;
    r.capacity=cap;
    if(hasLength && !compressed && length>cap){fail("body_limit");return;}
    // The service explicitly labels its JSON as UTF-8. Keep other parameters closed.
    const bool validType=image?!strcmp(r.contentType,"image/png"):
      equalFold(r.contentType,"application/json") || equalFold(r.contentType,"application/json; charset=utf-8");
    if(!validType){fail("content_type");return;}
    if(image && (compressed || !hasLength || chunked)){fail("protocol");return;}
  }
  if(r.status==204 || r.status==304) return;
  if(q.browser){
    if(!hasLength && !chunked){fail("protocol");return;}
    size_t remaining=hasLength?length:0,wire=0,controls=0,chunks=0;bool done=false,crlf=false;
    auto framed=[&](char* output,size_t count)->int{
      if(!ok())return -1;
      if(done)return 0;
      if(chunked && !remaining){
        if(crlf){char pair[2];if(read(pair,1)!=1 || read(pair+1,1)!=1 || pair[0]!='\r' || pair[1]!='\n'){fail("protocol");return -1;}controls+=2;}
        scratch.headerBytes=0;if(!line())return -1;controls+=scratch.headerBytes;
        if(++chunks>q.limit+1 || controls>6*q.limit+4096){fail("chunk_limit");return -1;}
        std::string_view value(scratch.line);value=value.substr(0,value.find(';'));
        if(!decimal(value,remaining,16)){fail("protocol");return -1;}
        if(remaining>q.limit-wire){fail("body_limit");return -1;}
        if(!remaining){
          scratch.headerBytes=0;
          while(line() && scratch.line[0]){
            // Backend responses have a closed trailer policy, including empty fields.
            if(q.backend){fail("backend_headers");return -1;}
            if(!strchr(scratch.line,':')){fail("protocol");return -1;}
          }
          done=true;return r.error?-1:0;
        }
        crlf=true;
      }
      if(!remaining){done=true;return 0;}
      size_t n=count<remaining?count:remaining;int got=read(output,n);
      if(got<=0 || size_t(got)>n){fail("truncated");return -1;}
      remaining-=got;wire+=got;
      if(wire>q.limit){fail("body_limit");return -1;}return got;
    };
    struct Adapter {decltype(framed)& read;decltype(ok)& check;} adapter{framed,ok};
    BodyStream stream{&adapter,[](void* p,char* bytes,size_t n){return static_cast<Adapter*>(p)->read(bytes,n);},
      [](void* p){return static_cast<Adapter*>(p)->check();}};
    if(compressed){gzip(stream,r);return;}
    for(;;){int got=framed(scratch.chunk,sizeof(scratch.chunk));if(got<0)return;if(!got)break;
      if(size_t(got)>r.capacity-r.size){fail("body_limit");return;}
      if(r.sink){if(!r.sink(r.sinkContext,scratch.chunk,got)){fail("storage_write");return;}}
      else memcpy(r.data()+r.size,scratch.chunk,got);
      r.size+=got;
    }
    ok();return;
  }
  auto bytes=[&](size_t count) {
    if(count>r.capacity-r.size){fail("body_limit");return false;}
    while(count) {int got=read(r.data()+r.size,count);if(got<=0){fail("truncated");return false;}r.size+=got;count-=got;}
    return true;
  };
  if(chunked) {
    for(;;) {
      if(!line()) return;
      size_t chunk;
      // No extensions/trailers in this deliberately narrow initial contract.
      std::string_view chunkText(scratch.line);
      if(q.browser)chunkText=chunkText.substr(0,chunkText.find(';'));
      if(!decimal(chunkText,chunk,16)){fail("protocol");return;}
      if(!chunk) {
        if(q.browser) { while(line() && scratch.line[0]) { if(!strchr(scratch.line,':')){fail("protocol");return;} } }
        else if(!line() || scratch.line[0]) fail("protocol");
        break;
      }
      if(!bytes(chunk)) return;
      char crlf[2];if(read(crlf,1)!=1 || read(crlf+1,1)!=1 || crlf[0]!='\r' || crlf[1]!='\n'){fail("protocol");return;}
    }
  } else if(hasLength) {bytes(length);}
  else { fail("protocol"); return; } // ESP TLS cannot distinguish clean close_notify from raw TCP EOF.

  ok();
}
void exchange(const Request& original,Result& r,Scratch& scratch,Io& io,const std::atomic<bool>& cancel) {
  if(!original.browser){exchangeOne(original,r,scratch,io,cancel,original.url,original.timeoutMs);return;}
  // Request scratch is job-owned: avoid copying the 3 KiB request onto the worker stack.
  // Redirects change only the URL and remain GET, with no credentials or cookies.
  scratch.current=original.url;
  const uint32_t start=io.millis(io.context);const uint32_t total=original.timeoutMs;
  for(unsigned hop=0;;++hop){
    uint32_t elapsed=io.millis(io.context)-start;
    if(elapsed>=total){r.error="timeout";return;}
    exchangeOne(original,r,scratch,io,cancel,scratch.current,total-elapsed);
    int n=snprintf(r.url,sizeof(r.url),"https://%s%s",scratch.current.host,scratch.current.path);
    if(n<0 || size_t(n)>=sizeof(r.url)){r.error="redirect_invalid";return;}
    if(r.error || r.status<300 || r.status>399)return;
    if(hop>=4){r.error="redirect_limit";return;}
    if(r.status!=301 && r.status!=302 && r.status!=303 && r.status!=307 && r.status!=308){r.error="redirect_invalid";return;}
    if(!resolveRedirect(scratch.current,r.location,scratch.current)){r.error="redirect_invalid";return;}
  }
}
} // namespace ink::http
