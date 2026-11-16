#include "HttpResource.h"
#include <cstdio>
#include <cstring>
#include <utility>
#include <new>
#include <Memory.h>
#if !defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
#include <dirent.h>
#endif
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#if !defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
#include <dirent.h>
#endif
namespace {
void* open(const char* path,bool write){
 auto file=makeUniqueNoThrow<HalFile>(); // One small HAL handle per live operation, closed by this adapter.
 if(!file){LOG_ERR("LHTTP","Resource handle OOM");return nullptr;}
 *file=Storage.open(path,write?(O_WRONLY|O_CREAT|O_EXCL):O_RDONLY);
 if(!*file){LOG_ERR("LHTTP","Resource open failed");return nullptr;}
 return file.release(); // This C-style adapter owns it until close().
}
bool exists(const char* p){return Storage.exists(p);}
size_t write(void* f,const char* p,size_t n){return static_cast<HalFile*>(f)->write(p,n);}
size_t fsRead(void* f,char* p,size_t n){auto r=static_cast<HalFile*>(f)->read(p,n);return r>0?size_t(r):0;}
bool seek(void* f,size_t n){return static_cast<HalFile*>(f)->seekSet(n);}
bool close(void* f){if(!f)return true;auto* file=static_cast<HalFile*>(f);bool ok=file->close();delete file;return ok;}
void fsRemove(const char* p){if(!Storage.remove(p))LOG_ERR("LHTTP","Resource remove failed");}
}
#else
namespace {
void* open(const char* p,bool write){return fopen(p,write?"wbx":"rb");}
bool exists(const char* p){auto* f=fopen(p,"rb");if(!f)return false;fclose(f);return true;}
size_t write(void* f,const char* p,size_t n){return fwrite(p,1,n,static_cast<FILE*>(f));}
size_t fsRead(void* f,char* p,size_t n){return fread(p,1,n,static_cast<FILE*>(f));}
bool seek(void* f,size_t n){return fseek(static_cast<FILE*>(f),long(n),SEEK_SET)==0;}
bool close(void* f){return !f || fclose(static_cast<FILE*>(f))==0;}
void fsRemove(const char* p){std::remove(p);}
}
#endif
namespace ink::http {
void cleanupResources(const char* root){
 auto discard=[&](const char* name){
  int id=0;unsigned attempt=0;char trailing=0;
  if(sscanf(name,"b5-spool%d-%u.object%c",&id,&attempt,&trailing)!=2 || id<=0)return;
  char path[384];int n=snprintf(path,sizeof(path),"%s/%s",root,name);
  if(n>0 && size_t(n)<sizeof(path))fsRemove(path);
 };
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
 HalFile directory=Storage.open(root);
 if(directory && directory.isDirectory()){
  unsigned count=0;
  for(HalFile entry=directory.openNextFile();entry && count<256;entry=directory.openNextFile()){
   char name[128]{};entry.getName(name,sizeof(name));entry.close();discard(name);++count;
  }
 }
 directory.close();
#else
 auto* directory=opendir(root);if(!directory)return;unsigned count=0;
 while(auto* entry=readdir(directory)){if(++count>256)break;discard(entry->d_name);}closedir(directory);
#endif
}
static uint32_t sum(const char* p,size_t n,uint32_t a){while(n--)a=(a*257+static_cast<uint8_t>(*p++))%65521;return a;}
bool Resource::begin(const char* root,int id){
 reset();
 if(!root || !root[0])return false;
 for(unsigned attempt=0;attempt<128;++attempt){
  int n=snprintf(path,sizeof(path),"%s/b5-spool%d-%u.object",root,id,attempt);
  if(n<0 || size_t(n)>=sizeof(path)){path[0]=0;return false;}
  if(exists(path))continue;
  writer=open(path,true);if(writer)return true;
  path[0]=0;return false;
 }
 path[0]=0;return false;
}
bool Resource::append(const char* bytes,size_t n){
 if(!writer || complete || write(writer,bytes,n)!=n)return false;
 hash=sum(bytes,n,hash);size+=n;return true;
}
bool Resource::finish(){bool ok=writer && close(writer);writer=nullptr;complete=ok;return ok;}
static void resourceFailure(const char* message){
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
 LOG_ERR("LHTTP","%s",message);
#else
 (void)message;
#endif
}
struct Resource::Copy {
 void* input=nullptr;
 void* output=nullptr;
 char destination[512]{};
 std::unique_ptr<char[]> buffer;
 size_t offset=0;
 uint32_t check=1;
 unsigned stage=0;
 bool published=false;
 ~Copy(){close(input);close(output);if(!published && destination[0])fsRemove(destination);}
};
void Resource::closeReader()const{if(reader)close(reader);reader=nullptr;readerOffset=0;}
void Resource::cancelCopy()const{delete copying;copying=nullptr;}
size_t Resource::read(size_t offset,char* bytes,size_t n)const{
 if(!complete || copying || offset>size)return 0;n=n>size-offset?size-offset:n;
 if(!n)return 0;
 if(!reader){reader=open(path,false);++stats.opens;readerOffset=0;}
 if(!reader)return 0;
 if(offset!=readerOffset){++stats.seeks;if(!seek(reader,offset)){closeReader();return 0;}readerOffset=offset;}
 size_t count=fsRead(reader,bytes,n);++stats.reads;stats.bytes+=count;readerOffset+=count;
 if(count!=n)closeReader();
 return count;
}
int Resource::copyStep(const char* destination)const{
 if(!complete || !destination || !strcmp(path,destination))return -1;
 if(!copying){
  if(strlen(destination)>=sizeof(Copy::destination) || exists(destination))return -1;
  closeReader(); // SdFat permits only one reader for this source path.
  auto state=makeUniqueNoThrow<Copy>(); // Bounded cursor state survives UI slices; stack cannot own it.
  if(!state){resourceFailure("Resource copy state allocation failed");return -1;}
  // One reusable 2 KiB buffer per copy, never on the small activity stack.
  state->buffer=makeUniqueNoThrow<char[]>(2048);
  if(!state->buffer){resourceFailure("Resource copy buffer allocation failed (2048 bytes)");return -1;}
  state->input=open(path,false);++stats.opens;
  if(!state->input)return -1;
  state->output=open(destination,true);++stats.opens;
  if(!state->output)return -1;
  strcpy(state->destination,destination);copying=state.release();return 0;
 }
 auto& c=*copying;
 if(strcmp(c.destination,destination))return -1;
 bool ok=true;
 if(c.stage==0){
  if(c.offset<size){size_t n=size-c.offset<2048?size-c.offset:2048;
   size_t got=fsRead(c.input,c.buffer.get(),n);++stats.reads;stats.bytes+=got;
   ok=got==n && write(c.output,c.buffer.get(),n)==n;
   if(ok){c.offset+=n;stats.copied+=n;}
  }else{
   ok=close(c.input);c.input=nullptr;
   ok=close(c.output)&&ok;c.output=nullptr;
   c.stage=1;c.offset=0;
  }
 }else if(c.stage==1){
  c.input=open(destination,false);++stats.opens;ok=c.input!=nullptr;c.stage=2;
 }else if(c.offset<size){
  size_t n=size-c.offset<2048?size-c.offset:2048;size_t got=fsRead(c.input,c.buffer.get(),n);
  ++stats.reads;stats.bytes+=got;ok=got==n;
  if(ok){c.check=sum(c.buffer.get(),n,c.check);c.offset+=n;}
 }else{
  char extra;ok=!fsRead(c.input,&extra,1) && c.check==hash;
  ok=close(c.input)&&ok;c.input=nullptr;
  if(ok){c.published=true;cancelCopy();return 1;}
 }
 if(!ok){resourceFailure("Resource copy or readback failed");cancelCopy();return -1;}return 0;
}
bool Resource::copy(const char* destination)const{
 int status;do{status=copyStep(destination);}while(status==0);return status==1;
}
void Resource::take(Resource& other){reset();std::swap(reader,other.reader);std::swap(readerOffset,other.readerOffset);std::swap(copying,other.copying);std::swap(stats,other.stats);std::swap(writer,other.writer);std::swap(size,other.size);std::swap(hash,other.hash);std::swap(complete,other.complete);for(size_t i=0;i<sizeof(path);++i)std::swap(path[i],other.path[i]);}
void Resource::reset(){cancelCopy();closeReader();if(writer)close(writer);writer=nullptr;if(path[0])fsRemove(path);path[0]=0;size=0;hash=1;complete=false;}
}
