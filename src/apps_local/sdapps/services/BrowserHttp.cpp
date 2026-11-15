#include "BrowserHttp.h"
#include <algorithm>
namespace stickyweb {
HttpResponse decodeResponse(std::string_view raw, bool cleanClose) {
  HttpResponse r;
  auto fail=[&](const char* why) { r.error=why; return r; };
  auto split=raw.find("\r\n\r\n");
  if(split==raw.npos || split>8192) return fail("Invalid or oversized response headers");
  auto eol=raw.find("\r\n");
  auto status=raw.substr(0,eol);
  if(status.size()<12 || (status.substr(0,9)!="HTTP/1.1 " && status.substr(0,9)!="HTTP/1.0 ") || status[9]<'1' || status[9]>'5' || status[10]<'0' || status[10]>'9' || status[11]<'0' || status[11]>'9' || (status.size()>12 && status[12]!=' ')) return fail("Invalid HTTP status");
  r.status=(status[9]-'0')*100+(status[10]-'0')*10+status[11]-'0';
  bool lengthSeen=false, typeSeen=false, locationSeen=false;
  size_t length=0;
  for(size_t pos=eol+2;pos<split;) {
    auto end=raw.find("\r\n",pos); if(end==raw.npos || end>split) return fail("Invalid header");
    auto line=raw.substr(pos,end-pos); pos=end+2;
    auto colon=line.find(':'); if(colon==line.npos || colon==0 || line.front()==' ' || line.front()=='\t') return fail("Invalid header");
    for(unsigned char c:line) if((c<32 && c!=9)||c==127) return fail("Invalid header control byte");
    std::string key(line.substr(0,colon));
    for(unsigned char c:key) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||std::string_view("!#$%&'*+-.^_`|~").find(c)!=std::string_view::npos)) return fail("Invalid header name");
    for(auto& c:key) if(c>='A' && c<='Z') c+='a'-'A';
    auto v=line.substr(colon+1); while(!v.empty() && (v.front()==' ' || v.front()=='\t')) v.remove_prefix(1);
    while(!v.empty() && (v.back()==' ' || v.back()=='\t')) v.remove_suffix(1);
    if(key=="content-length") {
      if(lengthSeen || v.empty()) return fail("Ambiguous content length");
      lengthSeen=true;
      for(char c:v) { if(c<'0'||c>'9'||length>kMaxBody) return fail("Invalid or oversized content length"); length=length*10+c-'0'; }
      if(length>kMaxBody) return fail("Page exceeds 32 KiB limit");
    } else if(key=="transfer-encoding") return fail("Transfer encoding unsupported; HTTP/1.0 required");
    else if(key=="content-encoding" && v!="identity") return fail("Compressed responses unsupported");
    else if(key=="location") { if(locationSeen || v.empty() || v.size()>2048) return fail("Invalid redirect"); locationSeen=true; r.location=std::string(v); }
    else if(key=="content-type") {
      if(typeSeen) return fail("Ambiguous content type");
      typeSeen=true;
      auto mime=v.substr(0,v.find(';'));
      if(mime!="text/html" && mime!="application/xhtml+xml" && mime!="text/plain") return fail("Only HTML or plain text is supported");
    }
  }
  if(r.status==301 || r.status==302 || r.status==303 || r.status==307 || r.status==308) {
    if(r.location.empty()) return fail("Redirect has no destination");
    r.ok=true; return r;
  }
  if(r.status!=200) return fail("Server did not return HTTP 200");
  if(!typeSeen) return fail("Missing content type");
  auto body=raw.substr(split+4);
  if(body.size()>kMaxBody) return fail("Page exceeds 32 KiB limit");
  if(lengthSeen ? body.size()!=length : !cleanClose) return fail("Incomplete response");
  r.body=std::string(body); r.ok=true; return r;
}
}
