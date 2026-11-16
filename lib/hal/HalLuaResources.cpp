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
size_t Resource::read(size_t offset,char* bytes,size_t n)const{
 if(!complete || offset>size)return 0;n=n>size-offset?size-offset:n;
 auto* file=open(path,false);if(!file)return 0;
 size_t count=seek(file,offset)?fsRead(file,bytes,n):0;bool ok=close(file);return ok?count:0;
}
bool Resource::copy(const char* destination)const{
 if(!complete || exists(destination))return false;
 auto* output=open(destination,true);if(!output)return false;
 // Small fixed chunk replaces response-sized copies; use owned heap rather than
 // the renderer/FreeRTOS stack. The copy is outside all Lua allocation calls.
 auto buffer=makeUniqueNoThrow<char[]>(2048);
 if(!buffer){
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
 LOG_ERR("LHTTP","Resource copy buffer allocation failed");
#endif
 close(output);fsRemove(destination);return false;
 }
 bool ok=true;uint32_t check=1;size_t offset=0;
 while(offset<size){size_t n=size-offset<2048?size-offset:2048;
  if(read(offset,buffer.get(),n)!=n || write(output,buffer.get(),n)!=n){ok=false;break;}
  offset+=n;
 }
 ok=close(output)&&ok;
 // Independently open/validate the completed destination. A short/failed close
 // never makes it publishable. No aliased source is ever deleted on a rename.
 auto* input=ok?open(destination,false):nullptr;
 if(!input)ok=false;
 if(input){offset=0;while(offset<size){size_t n=size-offset<2048?size-offset:2048;size_t got=fsRead(input,buffer.get(),n);if(got!=n){ok=false;break;}check=sum(buffer.get(),n,check);offset+=n;}char extra;if(fsRead(input,&extra,1))ok=false;ok=close(input)&&ok;}

 if(!ok || check!=hash){fsRemove(destination);return false;}return true;
}
void Resource::take(Resource& other){reset();std::swap(writer,other.writer);std::swap(size,other.size);std::swap(hash,other.hash);std::swap(complete,other.complete);for(size_t i=0;i<sizeof(path);++i)std::swap(path[i],other.path[i]);}
void Resource::reset(){if(writer)close(writer);writer=nullptr;if(path[0])fsRemove(path);path[0]=0;size=0;hash=1;complete=false;}
}
