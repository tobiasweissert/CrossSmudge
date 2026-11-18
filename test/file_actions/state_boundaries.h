#pragma once
#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>
constexpr int O_RDONLY=0, O_WRONLY=1, O_CREAT=2, O_EXCL=4;
struct StateIO {
 std::map<std::string,std::string> files;
 std::string failOpen,failWriteOpen,shortWrite,failSync,failClose,failRead,failRemove,failRename;
 std::map<std::string,int> writeOpens, bodyReads;
 std::map<std::string,uint64_t> reportedSizes;
 bool renameAliases=false, failAnyWriteOpen=false;
 int syncCalls=0, failSyncNumber=0;
 std::vector<std::string> mutations;
};
inline StateIO io;
class HalFile {
 std::string path; size_t pos=0; bool valid=false, writable=false;
 public:
 HalFile()=default;
 explicit HalFile(std::string p,bool w=false):path(std::move(p)),valid(true),writable(w){}
 explicit operator bool()const{return valid;}
 bool isDirectory()const{return false;}
 uint64_t size()const{auto it=io.reportedSizes.find(path);return it==io.reportedSizes.end()?io.files.at(path).size():it->second;}
 uint64_t fileSize()const{return size();}
 int read(void* buf,size_t n){++io.bodyReads[path];if(io.failRead==path&&io.writeOpens.count(path))return -1; auto&s=io.files[path];n=std::min(n,s.size()-pos);std::memcpy(buf,s.data()+pos,n);pos+=n;return int(n);}
 size_t write(const void* buf,size_t n){if(io.shortWrite==path)n/=2;io.files[path].append(static_cast<const char*>(buf),n);return n;}
 bool sync(){++io.syncCalls;return io.failSync!=path&&io.syncCalls!=io.failSyncNumber;}
 bool close(){valid=false;return !writable||io.failClose!=path;}
};
using FsFile=HalFile;
class HalStorage {
 public:
 bool mkdir(const char*,bool=true){return true;}
 bool exists(const char*p){return io.files.count(p);}
 bool remove(const char*p){io.mutations.push_back(std::string("remove:")+p);return io.failRemove!=p&&io.files.erase(p);}
 bool rename(const char*a,const char*b){io.mutations.push_back(std::string("rename:")+a);if(io.failRename==a&&!io.renameAliases)return false;if(!exists(a)||exists(b))return false;io.files[b]=io.files[a];if(!io.renameAliases)io.files.erase(a);return io.failRename!=a;}
 HalFile open(const char*p,int flags=O_RDONLY){if(io.failOpen==p)return {};if(flags&O_CREAT){if(io.failAnyWriteOpen||io.failWriteOpen==p)return {};if((flags&O_EXCL)&&exists(p))return {};io.mutations.push_back(std::string("open-write:")+p);++io.writeOpens[p];io.files[p].clear();return HalFile(p,true);}return exists(p)?HalFile(p):HalFile();}
 bool openFileForRead(const char*,const char*p,HalFile&f){f=open(p);return bool(f);}
 bool openFileForWrite(const char*,const char*p,HalFile&f){remove(p);f=open(p,O_WRONLY|O_CREAT);return bool(f);}
 bool writeFile(const char*p,const String&s){remove(p);auto f=open(p,O_WRONLY|O_CREAT);if(!f)return false;f.write(s.data(),s.size());f.close();return true;}
 String readFile(const char*p){if(io.failRead==p||!exists(p))return {};return String(io.files[p]);}
};
inline HalStorage Storage;
