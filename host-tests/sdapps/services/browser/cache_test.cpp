#include "BrowserCache.h"
#include <cassert>
#include <iostream>
#include <map>
using namespace stickyweb;
struct Disk final : CacheDisk {
 std::map<std::string,std::string> files;
 bool failWrite=false, failRename=false;
 std::function<bool(const std::string&,const std::string&,const std::string&)> fail;
 bool primaryRenamed=false, tempWritten=false, tempRead=false;
 bool blocked(const std::string& op,const std::string& a,const std::string& b="") { return fail && fail(op,a,b); }
 bool read(const std::string& p,std::string& bytes) override { if(blocked("read",p))return false; auto i=files.find(p); if(i==files.end()) return false; bytes=i->second; if(p.find(".new")!=std::string::npos)tempRead=true; return true; }
 bool write(const std::string& p,const std::string& bytes) override { if(failWrite||blocked("write",p))return false; files[p]=bytes;tempWritten=true; return true; }
 bool exists(const std::string& p) override { return files.count(p); }
 bool remove(const std::string& p) override { if(blocked("remove",p))return false; files.erase(p); return true; }
 bool rename(const std::string& a,const std::string& b) override { if(failRename||blocked("rename",a,b) || !files.count(a)||files.count(b))return false; files[b]=files[a];files.erase(a);if(b==a+".old")primaryRenamed=true;return true; }
};
void fallbackCancelRegression() {
 Disk disk; BrowserCache cache(disk); CachePage page;
 const std::string path="/.stickyweb-v1/0.page",url="https://example.org/recovery";
 assert(cache.save(url,"complete old",1,""));
 disk.files[path+".old"]=disk.files[path]; disk.files[path]="corrupt primary";
 assert(cache.load(url,page)&&page.html=="complete old");
 disk.primaryRenamed=false;
 assert(!cache.save(url,"replacement",2,"",[&](){return disk.primaryRenamed;}));
 assert(cache.load(url,page)&&page.html=="complete old");
 std::cout<<"Fallback survives cancellation after primary rename\n";
}
void fallbackBarrierRegressions() {
 const std::string path="/.stickyweb-v1/0.page",url="https://example.org/barriers";
 const std::vector<std::string> modes={"cancel-scan","cancel-before-write","cancel-after-write",
  "cancel-after-verify","cancel-after-primary-rename","write","read-temp","corrupt-temp",
  "remove-corrupt-primary","recover-rename","read-old","publish-rename","primary-rename",
  "publish-and-rollback","success","cleanup-old"};
 for(const bool missingPrimary:{false,true}) for(const auto& mode:modes) {
  Disk disk; BrowserCache cache(disk); CachePage page;
  assert(cache.save(url,"authoritative old",10,""));
  const std::string complete=disk.files[path];
  disk.files[path+".old"]=complete;
  if(missingPrimary)disk.files.erase(path);else disk.files[path]="corrupt primary";
  assert(cache.load(url,page)&&page.html=="authoritative old");
  disk.primaryRenamed=false; disk.tempWritten=false;disk.tempRead=false;
  int oldReads=0,polls=0;
  disk.fail=[&](const std::string& op,const std::string& a,const std::string&) {
   if(op=="read"&&a==path+".old")++oldReads;
   if(mode=="read-old"&&op=="read"&&a==path+".old"&&oldReads==2)return true;
   if(mode=="remove-corrupt-primary"&&op=="remove"&&a==path)return true;
   if(mode=="recover-rename"&&op=="rename"&&a==path+".old")return true;
   if(mode=="write"&&op=="write")return true;
   if(mode=="read-temp"&&op=="read"&&a==path+".new")return true;
   if(mode=="corrupt-temp"&&op=="read"&&a==path+".new")disk.files[a].back()^=1;
   if(mode=="primary-rename"&&op=="rename"&&a==path)return true;
   if((mode=="publish-rename"||mode=="publish-and-rollback")&&op=="rename"&&a==path+".new")return true;
   if(mode=="publish-and-rollback"&&disk.primaryRenamed&&op=="rename"&&a==path+".old")return true;
   if(mode=="cleanup-old"&&disk.primaryRenamed&&op=="remove"&&a==path+".old")return true;
   return false;
  };
  const bool saved=cache.save(url,"new complete",20,"",[&]() {
   ++polls;
   return (mode=="cancel-scan"&&polls==1)||(mode=="cancel-before-write"&&polls==2)||
    (mode=="cancel-after-write"&&disk.tempWritten)||(mode=="cancel-after-verify"&&disk.tempRead)||
    (mode=="cancel-after-primary-rename"&&disk.primaryRenamed);
  });
  const bool success=mode=="success"||mode=="cleanup-old"||(missingPrimary&&mode=="remove-corrupt-primary");
  if(saved!=success) {std::cerr<<"Unexpected save result: "<<mode<<" missing="<<missingPrimary<<'\n';std::abort();}
  disk.fail={};
  // Reopen through the real reader, not the transaction's in-memory state.
  BrowserCache reopened(disk);
  assert(reopened.load(url,page));
  assert(page.html==(success?"new complete":"authoritative old"));
  assert(page.retrieved==(success?20u:10u));
  if(!success)assert((disk.files.count(path)&&disk.files[path]==complete)||
                    (disk.files.count(path+".old")&&disk.files[path+".old"]==complete));
 }
 // A valid primary is authoritative over a stale complete fallback; failures
 // before rotating that primary must still preserve it.
 for(const bool failRemove:{false,true}) {
  Disk disk;BrowserCache cache(disk);CachePage page;
  assert(cache.save(url,"stale",1,""));auto stale=disk.files[path];
  assert(cache.save(url,"current",2,""));disk.files[path+".old"]=stale;
  disk.fail=[&](const std::string& op,const std::string& a,const std::string&) {
   return failRemove&&op=="remove"&&a==path+".old";
  };
  assert(cache.save(url,"replacement",3,"")==!failRemove);
  disk.fail={};assert(cache.load(url,page)&&page.html==(failRemove?"current":"replacement"));
 }
 std::cout<<"Fallback recovery: 32 missing/corrupt-primary barrier cases and 2 valid-primary authority cases passed\n";
}
int main(){
 fallbackCancelRegression();
 fallbackBarrierRegressions();
 Disk disk; BrowserCache cache(disk); CachePage page;
 assert(cache.save("https://example.org/a","<p>one</p>",123,""));
 assert(cache.load("https://example.org/a",page)&&page.html=="<p>one</p>"&&page.retrieved==123);
 assert(cache.list().size()==1);
 disk.failWrite=true;
 assert(!cache.save("https://example.org/a","<p>two</p>",124,""));
 assert(cache.load("https://example.org/a",page)&&page.html=="<p>one</p>");
 disk.failWrite=false; disk.failRename=true;
 assert(!cache.save("https://example.org/a","<p>two</p>",124,""));
 assert(cache.load("https://example.org/a",page)&&page.html=="<p>one</p>");
 disk.failRename=false;
 assert(!cache.save("https://127.0.0.1/","x",124,""));
 assert(!cache.save("https://example.org/a",std::string(32769,'x'),124,""));
 assert(cache.save("https://example.org/a","<p>two</p>",124,""));
 assert(cache.load("https://example.org/a",page)&&page.html=="<p>two</p>");
 assert(!cache.save("https://example.org/a","<p>cancelled</p>",125,"",[&](){return disk.exists("/.stickyweb-v1/0.page.new");}));
 assert(cache.load("https://example.org/a",page)&&page.html=="<p>two</p>");
 // Simulate reset after moving the complete old record aside.
 auto path=std::string("/.stickyweb-v1/0.page");
 disk.files[path+".old"]=disk.files[path]; disk.files.erase(path);
 assert(cache.load("https://example.org/a",page)&&page.html=="<p>two</p>");
 disk.files[path+".old"].back()^=1;
 assert(!cache.load("https://example.org/a",page));
 assert(cache.clear()&&disk.files.empty());
 for(int i=0;i<256;++i) assert(cache.save("https://example.org/"+std::to_string(i),"<p>body</p>",i+1,""));
 assert(cache.list().size()==256);
 assert(cache.save("https://example.org/new","<p>new</p>",1000,"https://example.org/0"));
 assert(cache.load("https://example.org/0",page));
 assert(!cache.load("https://example.org/1",page));
 assert(cache.load("https://example.org/new",page));
 for(const auto& pair:disk.files) assert(pair.first.find("/.stickyweb-v1/")==0);
 assert(cache.clear());
 std::cout<<"Cache roundtrip, bounds, corruption, interrupted publish, eviction and clear tests passed\n";
}
