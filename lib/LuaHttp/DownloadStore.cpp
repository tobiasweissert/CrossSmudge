#include "DownloadStore.h"
#include <cstring>
#include <algorithm>
namespace ink::http {
namespace {
constexpr const char* Names[] = {"0", "1"};
constexpr size_t Footer = 20;
void put(char* p,uint32_t v) { for(unsigned i=0;i<4;++i)p[i]=char(v>>(8*i)); }
uint32_t get(const char* p) { uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(static_cast<unsigned char>(p[i]))<<(8*i);return v; }
}
DownloadStore::Record DownloadStore::inspect(const char* name) {
  Record r;
  int present=files_.exists(name);
  if(present<0){healthy_=false;return r;}
  if(!present)return r;
  if(!files_.open(name,false)){healthy_=false;return r;}
  size_t n=files_.size();char footer[Footer];
  bool valid=n>=Footer && n<=MaxDownload+Footer && files_.seek(n-Footer) && files_.read(footer,Footer)==int(Footer);
  bool closed=files_.close();
  if(valid && closed && !memcmp(footer,"CPH4",4)) {
    auto gen=get(footer+4),size=get(footer+8);
    if(gen && size==n-Footer && get(footer+12)==~gen && get(footer+16)==~size)r={gen,size};
  }
  if(!r.generation)healthy_=false;
  return r;
}
DownloadStore::Record DownloadStore::latest() {
  healthy_=true;
  auto a=inspect(Names[0]),b=inspect(Names[1]);
  selected_=b.generation>a.generation?1:0;
  return selected_?b:a;
}
bool DownloadStore::begin() {
  if(writing_ || !files_.prepare())return false;
  auto old=latest();
  if(!healthy_ || old.generation==UINT32_MAX)return false;
  generation_=old.generation+1;
  selected_=old.generation?1-selected_:0;
  // Only discard the older generation, never the currently committed one.
  if(!files_.remove("part") || !files_.remove(Names[selected_]))return false;
  if(!files_.open("part",true))return false;
  writing_=true;size_=0;return true;
}
bool DownloadStore::append(const char* p,size_t n) {
  if(!writing_ || n>MaxDownload-size_ || files_.write(p,n)!=n)return false;
  size_+=n;return true;
}
bool DownloadStore::commit(const std::atomic<bool>& cancel,bool (*canPromote)(void*),void* context) {
  if(!writing_)return false;
  char footer[Footer];memcpy(footer,"CPH4",4);put(footer+4,generation_);put(footer+8,size_);
  put(footer+12,~generation_);put(footer+16,~uint32_t(size_));
  if(files_.write(footer,Footer)!=Footer || !files_.sync())return false;
  if(!files_.close())return false;
  if(cancel.load() || (canPromote && !canPromote(context)))return false;
  if(!files_.promote(Names[selected_]))return false;
  writing_=false;return true;
}
void DownloadStore::abort() {
  if(writing_) { files_.close();files_.remove("part");writing_=false; }
}
const char* DownloadStore::readAt(size_t offset,char* out,size_t count,size_t& got,size_t& total) {
  got=0;total=0;
  if(writing_)return "busy";
  if(count<1 || count>DownloadChunk || offset>MaxDownload)return "invalid_options";
  auto r=latest();if(!healthy_)return "file_read";if(!r.generation)return "not_found";
  total=r.size;if(offset>total)return "invalid_offset";
  count=std::min(count,total-offset);
  if(!files_.open(Names[selected_],false))return "file_read";
  bool ok=files_.seek(offset);
  int n=ok?files_.read(out,count):-1;
  bool closed=files_.close();
  if(n<0 || size_t(n)!=count || !closed)return "file_read";
  got=count;return nullptr;
}
}
