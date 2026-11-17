#include <cstdlib>
#include <HttpResource.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <unistd.h>
using ink::http::Resource;
int main(){
 char directory[256];snprintf(directory,sizeof(directory),"%s/browser-cursor-XXXXXX",getenv("TMPDIR")?getenv("TMPDIR"):"/tmp");assert(mkdtemp(directory));
 Resource source;assert(source.begin(directory,1));char bytes[256];memset(bytes,'x',sizeof(bytes));
 for(int i=0;i<1024;++i)assert(source.append(bytes,sizeof(bytes)));assert(source.finish());
 auto buffer=std::make_unique<char[]>(8192);
 for(size_t offset=0;offset<source.size;offset+=8192)assert(source.read(offset,buffer.get(),8192)==8192);
 assert(source.stats.opens==1 && source.stats.seeks==0 && source.stats.reads==32);
 const std::string destination=std::string(directory)+"/candidate.page";
 for(int i=0;i<20;++i)assert(source.copyStep(destination.c_str())==0);
 assert(source.read(0,buffer.get(),1)==0);source.cancelCopy();assert(!std::filesystem::exists(destination));
 auto opens=source.stats.opens,reads=source.stats.reads,seeks=source.stats.seeks;size_t steps=0;
 while(true){int state=source.copyStep(destination.c_str());++steps;assert(state>=0);if(state==1)break;assert(steps<300);}
 assert(source.stats.opens-opens==3 && source.stats.seeks==seeks && source.stats.reads-reads==256);
 assert(!source.copy(source.path));assert(std::filesystem::exists(source.path));
 assert(!source.copy(destination.c_str()));assert(std::filesystem::exists(destination));
 std::filesystem::remove(destination);
 assert(source.copyStep(destination.c_str())==0);
 // Truncated read during an owned copy must drop only the candidate.
 std::filesystem::resize_file(source.path,3);assert(source.copyStep(destination.c_str())==-1);
 assert(!std::filesystem::exists(destination) && std::filesystem::exists(source.path));
 printf("PASS persistent resource: sequential opens=1 seeks=0; verified 256KiB copy opens=3 seeks=0 chunks=256 steps=%zu; cancel/error/alias ownership\n",steps);
 source.reset();std::filesystem::remove_all(directory);
}
