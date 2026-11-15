#include "BrowserCache.h"
#include "BrowserPolicy.h"
#include "BrowserHttp.h"
#include <limits>
namespace stickyweb {
namespace {
std::string pathFor(int slot) { return "/.stickyweb-v1/"+std::to_string(slot)+".page"; }
uint32_t crc(std::string_view s) {
 uint32_t c=0xffffffff;
 for(unsigned char b:s) { c^=b; for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u & (0u-(c&1))); }
 return ~c;
}
void put(std::string& s,uint64_t n,int count) { for(int i=0;i<count;++i) {s.push_back(n&255);n>>=8;} }
uint64_t get(std::string_view s,size_t p,int count) {
 uint64_t n=0;for(int i=count-1;i>=0;--i)n=(n<<8)|static_cast<unsigned char>(s[p+i]);return n;
}
bool decode(const std::string& raw,CachePage& p) {
 if(raw.size()<28||raw.size()>kMaxCacheRecord||raw.substr(0,8)!="STKWEB01")return false;
 size_t u=get(raw,8,4), h=get(raw,12,4);
 if(!u||u>2048||!h||h>kMaxBody||raw.size()!=28+u+h||get(raw,raw.size()-4,4)!=crc(std::string_view(raw).substr(0,raw.size()-4)))return false;
 Destination d; auto url=raw.substr(24,u);
 if(!parseDestination(url,d))return false;
 p={std::move(url),raw.substr(24+u,h),get(raw,16,8)}; return true;
}
}
bool BrowserCache::readSlot(int slot,CachePage& page) {
 std::string raw; const auto p=pathFor(slot);
 if(disk.read(p,raw)&&decode(raw,page))return true;
 raw.clear(); return disk.read(p+".old",raw)&&decode(raw,page);
}
bool BrowserCache::load(const std::string& url,CachePage& page) {
 for(int i=0;i<256;++i) if(readSlot(i,page)&&page.url==url)return true;
 page={};return false;
}
std::vector<std::string> BrowserCache::list() {
 std::vector<std::string> urls;urls.reserve(256);CachePage p;
 for(int i=0;i<256;++i)if(readSlot(i,p))urls.push_back(p.url);
 return urls;
}
bool BrowserCache::save(const std::string& url,const std::string& html,uint64_t retrieved,const std::string& protectedUrl,const std::function<bool()>& cancelled) {
 Destination d;if(!parseDestination(url,d)||html.empty()||html.size()>kMaxBody)return false;
 int target=-1,empty=-1,oldest=-1;uint64_t time=std::numeric_limits<uint64_t>::max();CachePage page;
 for(int i=0;i<256;++i) {
  if(cancelled && cancelled())return false;
  if(!readSlot(i,page)){if(empty<0)empty=i;continue;}
  if(page.url==url){target=i;break;}
  if(page.url!=protectedUrl&&page.retrieved<time){time=page.retrieved;oldest=i;}
 }
 if(target<0)target=empty>=0?empty:oldest;
 if(target<0)return false;
 const auto p=pathFor(target),tmp=p+".new",old=p+".old";
 // Recover the authoritative fallback even when a corrupt primary exists.
 // Validate before deleting anything: readSlot may have selected .old, and
 // rotating an unchecked primary over it would destroy the sole valid record.
 if(disk.exists(old)) {
  std::string previous; CachePage complete;
  const bool primaryValid=disk.read(p,previous)&&decode(previous,complete);
  if(!primaryValid) {
   previous.clear();
   if(!disk.read(old,previous)||!decode(previous,complete))return false;
   if(disk.exists(p)&&!disk.remove(p))return false;
   if(!disk.rename(old,p))return false;
  }
 }
 std::string raw="STKWEB01";put(raw,url.size(),4);put(raw,html.size(),4);put(raw,retrieved,8);raw+=url;raw+=html;put(raw,crc(raw),4);
 if(cancelled && cancelled())return false;
 if(!disk.write(tmp,raw)) {disk.remove(tmp);return false;}
 std::string verify;CachePage verified;
 if(!disk.read(tmp,verify)||!decode(verify,verified)||verify!=raw){disk.remove(tmp);return false;}
 if(cancelled && cancelled()){disk.remove(tmp);return false;}
 if(disk.exists(old)&&!disk.remove(old)){disk.remove(tmp);return false;}
 const bool hadOld=disk.exists(p);
 if(hadOld&&!disk.rename(p,old)){disk.remove(tmp);return false;}
 if((cancelled && cancelled()) || !disk.rename(tmp,p)){if(hadOld)disk.rename(old,p);disk.remove(tmp);return false;}
 disk.remove(old);return true;
}
bool BrowserCache::clear() {
 bool ok=true;for(int i=0;i<256;++i) for(const char* suffix:{"",".new",".old"}) {
  auto p=pathFor(i)+suffix;if(disk.exists(p)&&!disk.remove(p))ok=false;
 }return ok;
}
}
#if defined(FREEINK_DEVICE_STICKY)
#include <HalStorage.h>
namespace stickyweb {
namespace {
class SdCacheDisk final : public CacheDisk {
 public:
 bool read(const std::string& p,std::string& bytes) override {
  auto f=Storage.open(p.c_str(),O_RDONLY);if(!f||f.isDirectory()||f.fileSize()>kMaxCacheRecord)return false;
  bytes.resize(f.fileSize()); const bool ok=f.read(bytes.data(),bytes.size())==static_cast<int>(bytes.size());return ok;
 }
 bool write(const std::string& p,const std::string& bytes) override {
  if(!Storage.ready()||!Storage.ensureDirectoryExists("/.stickyweb-v1"))return false;
  auto f=Storage.open(p.c_str(),O_WRONLY|O_CREAT|O_TRUNC);if(!f)return false;
  const bool ok=f.write(bytes.data(),bytes.size())==bytes.size();return ok;
 }
 bool exists(const std::string& p)override{return Storage.exists(p.c_str());}
 bool remove(const std::string& p)override{return Storage.remove(p.c_str());}
 bool rename(const std::string& a,const std::string& b)override{return Storage.rename(a.c_str(),b.c_str());}
};
}
BrowserCache& deviceCache(){static SdCacheDisk disk;static BrowserCache cache(disk);return cache;}
}
#endif
