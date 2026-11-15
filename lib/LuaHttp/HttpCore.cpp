#include "HttpCore.h"
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
  auto slash=s.find('/');
  auto h=s.substr(0,slash);
  if(!validHost(h)) return false;
  auto p=slash==s.npos ? std::string_view("/") : s.substr(slash);
  if(p.size()>=sizeof(u.path)) return false;
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
  memcpy(u.path,p.data(),p.size()); u.path[p.size()]=0;
  return true;
}
bool Domains::contains(const char* h) const {
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
bool downloadRoot(std::string_view appRoot,char* out,size_t capacity) {
  if(!capacity)return false;
  out[0]=0;
  for(auto root:{"/.crosssmudge/applications/", "/applications/", "/apps/", "/.smudge/applications/", "/.crosspoint/applications/"}) {
    std::string_view prefix(root);
    if(appRoot.substr(0,prefix.size())!=prefix)continue;
    auto name=appRoot.substr(prefix.size());
    if(!safeRelativePath(name) || name.find('/')!=name.npos)return false;
    constexpr std::string_view base="/.smudge/http_data";
    if(base.size()+appRoot.size()>=capacity)return false;
    memcpy(out,base.data(),base.size());memcpy(out+base.size(),appRoot.data(),appRoot.size());
    out[base.size()+appRoot.size()]=0;return true;
  }
  return false;
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
void exchange(const Request& q,Result& r,Scratch& scratch,Io& io,const std::atomic<bool>& cancel,BodySink* sink) {
  const size_t limit=sink?sink->limit:MaxBody;
  r.status=0; r.size=0; r.error=nullptr;
  const uint32_t start=io.millis(io.context);
  auto ok=[&]() {
    if(cancel.load()) {r.error="cancelled";return false;}
    if(uint32_t(io.millis(io.context)-start)>=q.timeoutMs) {r.error="timeout";return false;}
    return true;
  };
  auto fail=[&](const char* e) {if(!r.error) r.error=e;};
  if(!ok()) return;
  if(!io.connect(io.context,q.url.host,q.timeoutMs,cancel)) {fail("tls_or_network");io.close(io.context);return;}
  struct Close {Io& io; ~Close(){io.close(io.context);}} close{io};
  auto send=[&](const char* p,size_t n) {
    while(n && ok()) {int w=io.write(io.context,p,n);if(w<=0 || size_t(w)>n){fail("write");return false;}p+=w;n-=w;}
    return n==0 && ok();
  };
  int n=snprintf(scratch.line,sizeof(scratch.line),"%s %s HTTP/1.1\r\n",q.post?"POST":"GET",q.url.path);
  if(n<0 || size_t(n)>=sizeof(scratch.line)) {fail("request_limit");return;}
  if(!send(scratch.line,n)) return;
  n=snprintf(scratch.line,sizeof(scratch.line),"Host: %s\r\n",q.url.host);
  if(!send(scratch.line,n) || !send("Connection: close\r\nAccept-Encoding: identity\r\n",46)) return;
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
  size_t length=0;bool hasLength=false,chunked=false;
  while(line()) {
    std::string_view s(scratch.line); if(s.empty()) break;
    auto colon=s.find(':'); if(colon==s.npos || colon==0 || s[0]==' ' || s[0]=='\t') {fail("protocol");return;}
    auto name=s.substr(0,colon),value=s.substr(colon+1);
    while(!value.empty() && (value.front()==' ' || value.front()=='\t'))value.remove_prefix(1);
    while(!value.empty() && (value.back()==' ' || value.back()=='\t'))value.remove_suffix(1);
    if(equalFold(name,"content-length")) {
      if(hasLength || !decimal(value,length)){fail("protocol");return;} hasLength=true;
      if(length>limit){fail("body_limit");return;}
    } else if(equalFold(name,"transfer-encoding")) {
      if(chunked || !equalFold(value,"chunked")){fail("protocol");return;}chunked=true;
    } else if(equalFold(name,"content-encoding") && !equalFold(value,"identity")) {fail("encoding");return;}
  }
  if(r.error) return;
  if(chunked && hasLength){fail("protocol");return;}
  if(r.status>=300 && r.status<=399) {fail("redirect_denied");return;}
  if(sink && r.status!=200){fail("http_status");return;}
  if(r.status==204 || r.status==304) return;
  auto bytes=[&](size_t count) {
    if(count>limit-r.size){fail("body_limit");return false;}
    while(count) {
      const size_t wanted=sink?(count>1024?1024:count):count;
      char* target=sink?r.body:r.body+r.size;
      int got=read(target,wanted);
      if(got<=0 || size_t(got)>wanted){fail("truncated");return false;}
      if(sink && !sink->append(sink->context,target,got)){fail("file_write");return false;}
      r.size+=got;count-=got;
    }
    return true;
  };
  if(chunked) {
    for(;;) {
      if(!line()) return;
      size_t chunk;
      // No extensions/trailers in this deliberately narrow initial contract.
      if(!decimal(scratch.line,chunk,16)){fail("protocol");return;}
      if(!chunk) {if(!line() || scratch.line[0]) fail("protocol");break;}
      if(!bytes(chunk)) return;
      char crlf[2];if(read(crlf,1)!=1 || read(crlf+1,1)!=1 || crlf[0]!='\r' || crlf[1]!='\n'){fail("protocol");return;}
    }
  } else if(hasLength) {bytes(length);}
  else { fail("protocol"); return; } // ESP TLS cannot distinguish clean close_notify from raw TCP EOF.

  ok();
}
} // namespace ink::http
