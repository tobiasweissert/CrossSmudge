#include "BrowserPolicy.h"
#include <algorithm>
namespace stickyweb {
bool publicIpv4(uint32_t a) {
  struct Range { uint32_t net, mask; };
  static constexpr Range denied[] = {
    {0x00000000,0xff000000}, {0x0a000000,0xff000000}, {0x64400000,0xffc00000},
    {0x7f000000,0xff000000}, {0xa9fe0000,0xffff0000}, {0xac100000,0xfff00000},
    {0xc0000000,0xffffff00}, {0xc0000200,0xffffff00}, {0xc0586300,0xffffff00},
    {0xc0a80000,0xffff0000}, {0xc6120000,0xfffe0000}, {0xc6336400,0xffffff00},
    {0xcb007100,0xffffff00}, {0xe0000000,0xe0000000}
  };
  for (const auto& r : denied) if ((a & r.mask)==r.net) return false;
  return true;
}
bool parseDestination(std::string_view url, Destination& out) {
  out = {};
  if (url.size() > 2048 || url.substr(0, 8) != "https://") return false;
  for (unsigned char c : url) if (c <= 32 || c >= 127 || c == '\\') return false;
  for (size_t i = 0; i < url.size(); ++i) {
    if (url[i] != '%') continue;
    if(i+2>=url.size()) return false;
    auto hex = [](char c) { if(c >= '0' && c <= '9') return c-'0'; if(c >= 'a' && c <= 'f') return c-'a'+10; if(c >= 'A' && c <= 'F') return c-'A'+10; return -1; };
    int a=hex(url[i+1]), b=hex(url[i+2]);
    if (a < 0 || b < 0 || a*16+b <= 32 || a*16+b == 127 || a*16+b == 92) return false;
    i += 2;
  }
  const auto end = url.find_first_of("/?#", 8);
  auto authority = url.substr(8, end == url.npos ? url.size()-8 : end-8);
  if (authority.size() > 4 && authority.substr(authority.size()-4) == ":443") authority.remove_suffix(4);
  if (authority.empty() || authority.size() > 253) return false;
  std::string host(authority);
  for (auto& c : host) if (c >= 'A' && c <= 'Z') c += 'a'-'A';
  bool letter=false;
  size_t label=0;
  for (size_t i=0; i<host.size(); ++i) {
    char c=host[i];
    if (c=='.') { if(!label || label>63 || host[i-1]=='-') return false; label=0; continue; }
    if (!((c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='-') || (!label && c=='-')) return false;
    letter |= c>='a'&&c<='z'; ++label;
  }
  if (!letter || !label || label>63 || host.back()=='-' || host.find('.')==host.npos) return false;
  const auto suffix=host.substr(host.rfind('.')+1);
  if (suffix=="local" || suffix=="localhost" || suffix=="internal" || suffix=="test" || suffix=="invalid" || suffix=="onion") return false;
  out.host=host;
  if (end != url.npos) {
    auto target=url.substr(end, url.find('#',end)==url.npos ? url.size()-end : url.find('#',end)-end);
    out.target=std::string(target);
  }
  if (out.target.empty() || out.target.front()!='/') out.target.insert(0,"/");
  return true;
}
}
