#include "BrowserCore.h"
#include <utility>
namespace stickyweb {
namespace {
char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a'-'A')) : c; }
bool alpha(char c) { return (lower(c)>='a' && lower(c)<='z'); }
bool digit(char c) { return c>='0' && c<='9'; }
int hex(char c) { if (digit(c)) return c-'0'; c=lower(c); return c>='a' && c<='f' ? c-'a'+10 : -1; }
std::string lowercase(std::string_view s) { std::string r(s); for(char& c:r) c=lower(c); return r; }
bool safeUrl(std::string_view s) {
 if(s.size()>4096) return false;
 for (std::size_t i=0;i<s.size();++i) {
  unsigned char c=static_cast<unsigned char>(s[i]);
  if(c<=32 || c>=127 || c=='\\' || c=='<' || c=='>' || c=='"' || c=='`') return false;
  if(c=='%') {
   if(i+2>=s.size() || hex(s[i+1])<0 || hex(s[i+2])<0) return false;
   int decoded=hex(s[i+1])*16+hex(s[i+2]);
   if(decoded<32 || decoded==127 || decoded=='\\') return false;
   i+=2;
  }
 }
 return true;
}
std::string absolute(std::string_view ref) {
 if(!safeUrl(ref) || lowercase(ref.substr(0,8))!="https://") return {};
 auto end=ref.find_first_of("/?#",8); if(end==std::string_view::npos) end=ref.size();
 auto authority=ref.substr(8,end-8);
 if(authority.empty() || authority.find('@')!=std::string_view::npos) return {};
 auto colon=authority.find(':'); auto host=authority.substr(0,colon);
 if(host.empty() || host.size()>253) return {};
 std::size_t start=0;
 while(start<host.size()) {
  auto dot=host.find('.',start); if(dot==std::string_view::npos) dot=host.size();
  auto label=host.substr(start,dot-start);
  if(label.empty() || label.size()>63 || label.front()=='-' || label.back()=='-') return {};
  for(char c:label) if(!alpha(c) && !digit(c) && c!='-') return {};
  start=dot+1;
 }
 if(colon!=std::string_view::npos) {
  auto port=authority.substr(colon+1); unsigned n=0;
  if(port.empty() || port.size()>5) return {};
  for(char c:port) { if(!digit(c)) return {}; n=n*10+static_cast<unsigned>(c-'0'); }
  if(n==0 || n>65535) return {};
 }
 std::string result="https://"+lowercase(authority);
 if(end==ref.size() || ref[end]!='/' ) result+='/';
 result.append(ref.substr(end)); return result;
}
}
std::string resolveUrl(std::string_view base, std::string_view ref) {
 if(!safeUrl(ref)) return {};
 std::string candidate;
 auto colon=ref.find(':'); auto slash=ref.find_first_of("/?#");
 if(colon!=std::string_view::npos && (slash==std::string_view::npos || colon<slash)) candidate=absolute(ref);
 else if(ref.substr(0,2)=="//") candidate=absolute("https:"+std::string(ref));
 else {
  std::string b=absolute(base); if(b.empty()) return {};
  b.erase(b.find('#')==std::string::npos ? b.size() : b.find('#'));
  auto originEnd=b.find('/',8);
  if(ref.empty()) candidate=b;
  else if(ref.front()=='#') candidate=b+std::string(ref);
  else if(ref.front()=='?') candidate=b.substr(0,b.find('?'))+std::string(ref);
  else if(ref.front()=='/') candidate=b.substr(0,originEnd)+std::string(ref);
  else {
   b=b.substr(0,b.find('?'));
   candidate=b.substr(0,b.rfind('/')+1)+std::string(ref);
  }
 }
 if(candidate.empty() || candidate.size()>4096) return {};
 auto start=candidate.find('/',8), end=candidate.find_first_of("?#",start);
 if(end==std::string::npos) end=candidate.size();
 std::string_view path(candidate.data()+start,end-start);
 std::vector<std::string_view> segments;
 segments.reserve(path.size()+1);
 std::size_t pos=1;
 while(pos<=path.size()) {
  auto next=path.find('/',pos); if(next==std::string_view::npos) next=path.size();
  auto segment=path.substr(pos,next-pos);
  if(segment=="..") { if(!segments.empty()) segments.pop_back(); }
  else if(segment!=".") segments.push_back(segment);
  if(next==path.size()) { if(segment=="." || segment=="..") segments.push_back({}); break; }
  pos=next+1;
 }
 std::string result=candidate.substr(0,start)+"/";
 for(std::size_t i=0;i<segments.size();++i) { if(i) result+='/'; result.append(segments[i]); }
 result.append(candidate.substr(end)); return result;
}
namespace {
bool space(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='\f'; }
void codepoint(std::string& out, unsigned cp) {
 if(cp==0 || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff) || (cp<32 && cp!=9 && cp!=10 && cp!=13) || (cp>=127 && cp<=159)) cp=0xfffd;
 if(cp==160) cp=32;
 if(cp<128) out+=static_cast<char>(cp);
 else if(cp<2048) { out+=static_cast<char>(0xc0|(cp>>6)); out+=static_cast<char>(0x80|(cp&63)); }
 else if(cp<65536) { out+=static_cast<char>(0xe0|(cp>>12)); out+=static_cast<char>(0x80|((cp>>6)&63)); out+=static_cast<char>(0x80|(cp&63)); }
 else { out+=static_cast<char>(0xf0|(cp>>18)); out+=static_cast<char>(0x80|((cp>>12)&63)); out+=static_cast<char>(0x80|((cp>>6)&63)); out+=static_cast<char>(0x80|(cp&63)); }
}
// Decode directly into a consumer, avoiding a second page-sized text buffer.
template<class Emit>
void decodeEach(std::string_view text, Emit emit) {
 for(std::size_t i=0;i<text.size();) {
  if(text[i]=='&') {
   auto offset=text.substr(i+1,32).find(';');
   auto semi=offset==std::string_view::npos ? offset : i+1+offset;
   if(semi!=std::string_view::npos && semi-i<=32) {
    auto name=text.substr(i+1,semi-i-1); unsigned cp=0; bool found=false;
    if(!name.empty() && name[0]=='#') {
     std::size_t n=1; unsigned radix=10;
     if(n<name.size() && (name[n]=='x' || name[n]=='X')) { ++n; radix=16; }
     found=n<name.size();
     for(;n<name.size();++n) {
      int d=hex(name[n]); if(d<0 || static_cast<unsigned>(d)>=radix) { found=false; break; }
      if(cp<=0x110000) cp=cp*radix+static_cast<unsigned>(d);
     }
    } else {
     struct Entity { const char* name; unsigned value; };
     static constexpr Entity entities[]={{"cent",162},{"deg",176},{"times",215},{"aacute",225},{"Ouml",214},{"divide",247},{"amp",38},{"lt",60},{"gt",62},{"quot",34},{"apos",39},{"nbsp",160},{"copy",169},{"reg",174},{"trade",8482},{"mdash",8212},{"ndash",8211},{"hellip",8230},{"lsquo",8216},{"rsquo",8217},{"ldquo",8220},{"rdquo",8221},{"bull",8226},{"euro",8364},{"pound",163},{"laquo",171},{"raquo",187},{"eacute",233},{"Eacute",201},{"auml",228},{"ouml",246},{"uuml",252},{"szlig",223}};
     for(const auto& entity:entities) if(name==entity.name) { cp=entity.value; found=true; break; }
    }
    if(found) { emit(cp); i=semi+1; continue; }
   }
  }
  unsigned char c=static_cast<unsigned char>(text[i]);
  if(c<128) { emit(c); ++i; continue; }
  std::size_t length=c>=0xc2 && c<=0xdf ? 2 : c>=0xe0 && c<=0xef ? 3 : c>=0xf0 && c<=0xf4 ? 4 : 0;
  unsigned cp=length ? c & (0x7f>>length) : 0; bool valid=length && i+length<=text.size();
  if(valid) for(std::size_t j=1;j<length;++j) { unsigned char d=static_cast<unsigned char>(text[i+j]); if((d&0xc0)!=0x80) { valid=false; break; } cp=(cp<<6)|(d&63); }
  if(valid && (cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff) || (length==2 && cp<128) || (length==3 && cp<2048) || (length==4 && cp<65536))) valid=false;
  if(valid) { emit(cp); i+=length; }
  else { emit(0xfffd); ++i; }
 }
}
std::string decode(std::string_view text) {
 std::string result;
 decodeEach(text,[&](unsigned cp) { codepoint(result,cp); });
 return result;
}
void appendText(std::string& out, std::string_view raw) {
 decodeEach(raw,[&](unsigned cp) {
  if(out.size()>192*1024) return;
  if(cp==160 || (cp<128 && space(static_cast<char>(cp)))) {
   if(!out.empty() && out.back()!=' ' && out.back()!='\n') out+=' ';
  } else codepoint(out,cp);
 });
}
void trim(std::string& s) { while(!s.empty() && space(s.back())) s.pop_back(); }
void line(std::string& s) { trim(s); if(!s.empty()) s+='\n'; }
bool block(std::string_view n) {
 return n=="p" || n=="div" || n=="br" || n=="hr" || n=="li" || n=="ul" || n=="ol" || n=="section" || n=="article" || n=="blockquote" || n=="pre" || n=="tr" || (n.size()==2 && n[0]=='h' && n[1]>='1' && n[1]<='6');
}
struct Attribute { std::string name; std::string_view value; };
struct Tag { std::string name; bool closing=false; std::size_t end=0; std::vector<Attribute> attributes; };
std::string_view attribute(const Tag& tag, std::string_view name, bool* exists=nullptr) {
 for(const auto& a:tag.attributes) if(a.name==name) { if(exists) *exists=true; return a.value; }
 if(exists) *exists=false;
 return {};
}
bool readTag(std::string_view html, std::size_t pos, Tag& tag) {
 tag.attributes.reserve(6);
 auto i=pos+1;
 if(i<html.size() && html[i]=='/') { tag.closing=true; ++i; }
 if(i==html.size() || !alpha(html[i])) return false;
 auto start=i;
 while(i<html.size() && (alpha(html[i]) || digit(html[i]) || html[i]=='-' || html[i]==':')) ++i;
 tag.name=lowercase(html.substr(start,i-start));
 while(i<html.size()) {
  while(i<html.size() && (space(html[i]) || html[i]=='/')) ++i;
  if(i==html.size()) break;
  if(html[i]=='>') { tag.end=i+1; return true; }
  auto attrStart=i;
  while(i<html.size() && !space(html[i]) && html[i]!='=' && html[i]!='>' && html[i]!='/') ++i;
  if(i==attrStart) { ++i; continue; }
  auto name=lowercase(html.substr(attrStart,i-attrStart));
  while(i<html.size() && space(html[i])) ++i;
  std::string_view value;
  if(i<html.size() && html[i]=='=') {
   ++i; while(i<html.size() && space(html[i])) ++i;
   if(i<html.size() && (html[i]=='\'' || html[i]=='"')) {
    char quote=html[i++]; auto valueStart=i;
    while(i<html.size() && html[i]!=quote) ++i;
    value=html.substr(valueStart,i-valueStart); if(i<html.size()) ++i;
   } else {
    auto valueStart=i; while(i<html.size() && !space(html[i]) && html[i]!='>') ++i;
    value=html.substr(valueStart,i-valueStart);
   }
  }
  if(name=="href" || name=="hidden" || name=="inert" || name=="aria-hidden" || name=="style" || name=="class") {
   bool exists=false; attribute(tag,name,&exists);
   if(!exists) tag.attributes.push_back({name,value});
  }
 }
 tag.end=html.size(); return true;
}
bool listed(std::string_view name, std::string_view list) {
 std::string needle="|"+std::string(name)+"|";
 return list.find(needle)!=std::string_view::npos;
}
bool suppressedTag(std::string_view name) {
 return listed(name,"|script|style|form|iframe|nav|footer|object|embed|svg|math|template|noscript|canvas|video|audio|button|select|textarea|");
}
bool rawTag(std::string_view name) { return listed(name,"|script|style|iframe|textarea|noscript|"); }
bool voidTag(std::string_view name) { return listed(name,"|area|base|br|col|embed|hr|img|input|link|meta|param|source|track|wbr|"); }
bool hiddenTag(const Tag& tag) {
 // Only known icon-font containers, matched by complete class token. Ordinary
 // prose containing a ligature word remains untouched; no global ASCII filtering.
 if(tag.name=="span" || tag.name=="i") {
   auto classes=attribute(tag,"class");
   while(!classes.empty()) {
     while(!classes.empty() && space(classes.front())) classes.remove_prefix(1);
     size_t n=0; while(n<classes.size() && !space(classes[n])) ++n;
     auto token=classes.substr(0,n);
     if(token=="material-icons" || token=="material-icons-outlined" ||
        token=="material-icons-round" || token=="material-icons-sharp" ||
        token=="material-icons-two-tone" || token=="material-symbols-outlined" ||
        token=="material-symbols-rounded" || token=="material-symbols-sharp") return true;
     classes.remove_prefix(n);
   }
 }
 bool exists=false; attribute(tag,"hidden",&exists); if(exists) return true;
 attribute(tag,"inert",&exists); if(exists) return true;
 if(lowercase(decode(attribute(tag,"aria-hidden")))=="true") return true;
 std::string style=lowercase(decode(attribute(tag,"style"))), compact;
 for(char c:style) if(!space(c)) compact+=c;
 return compact.find("display:none")!=std::string::npos || compact.find("visibility:hidden")!=std::string::npos || compact.find("content-visibility:hidden")!=std::string::npos;
}
std::size_t rawEnd(std::string_view html, std::size_t pos, std::string_view name) {
 while((pos=html.find('<',pos))!=std::string_view::npos) {
  if(pos+2+name.size()<=html.size() && html[pos+1]=='/' && lowercase(html.substr(pos+2,name.size()))==name) {
   auto end=pos+2+name.size();
   if(end==html.size() || space(html[end]) || html[end]=='>' || html[end]=='/') return pos;
  }
  ++pos;
 }
 return html.size();
}
struct Frame { std::string name; bool suppressed; };
}
ParseResult parseHtml(std::string_view html, std::string_view base) {
 constexpr std::size_t textLimit=192*1024;
 if(html.size()>512*1024) return {false,"HTML input exceeds 512 KiB",{}};
 Document doc; doc.links.reserve(128); doc.spans.reserve(2048); bool title=false, head=false;
 std::vector<Frame> stack;
 stack.reserve(256);
 std::string raw;
 auto suppressed=[&]() { return !stack.empty() && stack.back().suppressed; };
 Link pending;
 std::size_t anchorStart=0; bool spanOverflow=false;
 auto finishLink=[&]() {
  if(pending.url.empty()) return;
  trim(pending.label); if(pending.label.empty()) pending.label=pending.url;
  size_t index=0; while(index<doc.links.size() && doc.links[index].url!=pending.url)++index;
  if(index==doc.links.size() && doc.links.size()<128) doc.links.push_back(pending);
  size_t start=anchorStart,end=doc.text.size();
  while(start<end && static_cast<unsigned char>(doc.text[start])<=32)++start;
  while(end>start && static_cast<unsigned char>(doc.text[end-1])<=32)--end;
  if(index<doc.links.size() && end>start) {
   if(doc.spans.size()==2048) spanOverflow=true;
   else doc.spans.push_back({uint32_t(start),uint32_t(end),uint32_t(index)});
  }
  pending={};
 };
 std::size_t pos=0;
 while(pos<html.size()) {
  if(!raw.empty()) { pos=rawEnd(html,pos,raw); raw.clear(); if(pos==html.size()) break; }
  if(html.substr(pos,4)=="<!--") { auto end=html.find("-->",pos+4); pos=end==std::string_view::npos ? html.size() : end+3; continue; }
  if(html.substr(pos,2)=="<!" || html.substr(pos,2)=="<?") { auto end=html.find('>',pos+2); pos=end==std::string_view::npos ? html.size() : end+1; continue; }
  Tag tag;
  if(html[pos]=='<' && readTag(html,pos,tag)) {
   if(!tag.closing && tag.name=="p" && !stack.empty() && stack.back().name=="p" && !stack.back().suppressed) stack.pop_back();
   bool wasSuppressed=suppressed();
   if(tag.closing) {
    for(std::size_t i=stack.size();i>0;--i) if(stack[i-1].name==tag.name) { stack.resize(i-1); break; }
   } else if(!voidTag(tag.name)) {
    if(stack.size()>=256) return {false,"HTML nesting exceeds 256",{}};
    stack.push_back({tag.name,wasSuppressed || suppressedTag(tag.name) || hiddenTag(tag)});
    if(rawTag(tag.name)) raw=tag.name;
   }
   bool visible=!wasSuppressed && !suppressed();
   if(block(tag.name) && visible && !head && !title) { line(doc.text); if(!pending.url.empty()) line(pending.label); }
   if(tag.name=="title" && visible) title=!tag.closing;
   if(tag.name=="head" && visible) head=!tag.closing;
   if(tag.name=="a" && visible) {
    finishLink();
    bool exists=false; auto href=attribute(tag,"href",&exists);
    while(!href.empty() && href.front()==' ') href.remove_prefix(1);
    while(!href.empty() && href.back()==' ') href.remove_suffix(1);
    if(!tag.closing && !title && !head && exists) {pending.url=resolveUrl(base,decode(href));anchorStart=doc.text.size();}
   }
   pos=tag.end; continue;
  }
  auto end=html.find('<',pos+1); if(end==std::string_view::npos) end=html.size();
  if(!suppressed()) {
   if(title) appendText(doc.title,html.substr(pos,end-pos));
   else if(!head) { appendText(doc.text,html.substr(pos,end-pos)); if(!pending.url.empty()) appendText(pending.label,html.substr(pos,end-pos)); }
  }
  if(doc.text.size()>textLimit || doc.title.size()>textLimit) return {false,"Extracted text exceeds 192 KiB",{}};
  pos=end;
 }
 finishLink();
 if(spanOverflow)return {false,"Page exceeds 2048 link occurrences",{}};
 trim(doc.text); trim(doc.title);
 if(doc.text.empty()) return {false,"No readable text",{}};
 return {true,{},std::move(doc)};
}
}
