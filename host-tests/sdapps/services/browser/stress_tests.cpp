#include "BrowserCore.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Deterministic adversarial smoke coverage, not a substitute for a full fuzzer.
static bool utf8(std::string_view s) {
 for(std::size_t i=0;i<s.size();) {
  unsigned c=static_cast<unsigned char>(s[i++]);
  if(c<128) continue;
  unsigned n=0, cp=0, minimum=0;
  if(c>=0xc2 && c<=0xdf) { n=1; cp=c&31; minimum=128; }
  else if(c>=0xe0 && c<=0xef) { n=2; cp=c&15; minimum=2048; }
  else if(c>=0xf0 && c<=0xf4) { n=3; cp=c&7; minimum=65536; }
  else return false;
  while(n--) { if(i==s.size()) return false; unsigned d=static_cast<unsigned char>(s[i++]); if((d&192)!=128) return false; cp=cp*64+(d&63); }
  if(cp<minimum || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return false;
 }
 return true;
}
static unsigned assertions=0;
static void require(bool value) { ++assertions; if(!value) { std::fprintf(stderr,"Stress invariant failed at assertion %u\n",assertions); std::exit(1); } }
int main() {
 std::uint32_t seed=0x12345678;
 auto random=[&]() { seed^=seed<<13; seed^=seed>>17; seed^=seed<<5; return seed; };
 const char* parts[]={"<p>","</div>","<a href='../ok?x=1&amp;y=2'>","</a>","<script>","</SCRIPT>","<div hidden>","<!--", "-->","&#x1F600;", "&", "<", "' >", "<style/>","</style>","<iframe>","</iframe>","<nav>","</nav>","<title>","</title>","<form>","</form>","<br>","text "};
 for(unsigned test=0;test<5000;++test) {
  std::string html;
  unsigned count=random()%80;
  for(unsigned i=0;i<count;++i) {
   if(random()%3) html+=parts[random()%(sizeof(parts)/sizeof(parts[0]))];
   else html+=static_cast<char>(random()%256);
  }
  auto result=stickyweb::parseHtml(html,"https://example.test/a/b");
  require(utf8(result.document.text)); require(utf8(result.document.title));
  require(result.document.text.size()<=192*1024); require(result.document.links.size()<=128);
  require(result.ok ? !result.document.text.empty() && result.error.empty() : !result.error.empty() && result.document.text.empty());
  for(const auto& link:result.document.links) { require(utf8(link.label)); require(link.url.substr(0,8)=="https://"); require(stickyweb::resolveUrl("",link.url)==link.url); }
  const auto url=stickyweb::resolveUrl("https://example.test/a/b",html);
  require(url.empty() || url.substr(0,8)=="https://");
 }
 require(stickyweb::parseHtml(std::string(192*1024,'&'),"https://example.test").ok);
 require(stickyweb::parseHtml("A"+std::string(200000,' ')+"B","https://example.test").document.text=="A B");
 std::printf("PASS 5000 deterministic malformed-input cases, %u invariant assertions\n",assertions);
}
