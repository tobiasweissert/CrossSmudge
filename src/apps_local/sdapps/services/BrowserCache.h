#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace stickyweb {
// All paths are fixed slot names, never remote URL components. The disk
// implementation must bound read allocation to kMaxCacheRecord bytes.
constexpr size_t kMaxCacheRecord=34844;
struct CacheDisk {
 virtual ~CacheDisk()=default;
 virtual bool read(const std::string&,std::string&)=0;
 virtual bool write(const std::string&,const std::string&)=0;
 virtual bool exists(const std::string&)=0;
 virtual bool remove(const std::string&)=0;
 virtual bool rename(const std::string&,const std::string&)=0;
};
struct CachePage { std::string url,html; uint64_t retrieved=0; };
class BrowserCache {
 CacheDisk& disk;
 bool readSlot(int slot,CachePage& page);
 public:
 explicit BrowserCache(CacheDisk& d):disk(d){}
 bool load(const std::string& url,CachePage& page);
 std::vector<std::string> list();
 bool save(const std::string& url,const std::string& html,uint64_t retrieved,const std::string& protectedUrl,const std::function<bool()>& cancelled={});
 bool clear();
};
#if defined(FREEINK_DEVICE_STICKY)
BrowserCache& deviceCache();
#endif
}
