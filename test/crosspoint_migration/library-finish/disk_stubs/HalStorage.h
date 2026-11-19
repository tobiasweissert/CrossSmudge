#pragma once
#include "Print.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
namespace disk { inline std::string root; inline std::string path(const char* p){return root+p;} }
class HalFile:public Print{
 std::shared_ptr<int> fd;std::shared_ptr<std::vector<std::string>> children;std::string path_;size_t pos=0;bool directory=false;
public:
 HalFile()=default;
 explicit HalFile(std::string p,int flags=O_RDONLY):path_(std::move(p)){
  std::error_code ec;directory=std::filesystem::is_directory(path_,ec);
  if(directory){children=std::make_shared<std::vector<std::string>>();for(const auto&e:std::filesystem::directory_iterator(path_))children->push_back(e.path().string());std::sort(children->begin(),children->end());}
  else {int n=::open(path_.c_str(),flags,0600);if(n>=0)fd=std::shared_ptr<int>(new int(n),[](int*p){::close(*p);delete p;});}
 }
 explicit operator bool()const{return bool(fd)||bool(children);}bool isOpen()const{return bool(*this);}bool isDirectory()const{return directory&&bool(children);}bool close(){fd.reset();children.reset();return true;}
 bool allocationFailed()const{return false;}bool iterationFailed()const{return false;}
 HalFile openNextFile(){return children&&pos<children->size()?HalFile((*children)[pos++]):HalFile();}
 void rewindDirectory(){pos=0;}size_t position()const{return pos;}
 bool seekSet(size_t n){if(!*this)return false;pos=n;return true;}bool seek(size_t n){return seekSet(n);}bool seekCur(int64_t n){if(n<0&&size_t(-n)>pos)return false;return seekSet(pos+n);}
 size_t getName(char*out,size_t n){auto s=std::filesystem::path(path_).filename().string();if(n){std::strncpy(out,s.c_str(),n);out[n-1]=0;}return s.size();}
 uint64_t fileSize64()const{struct stat s{};return fd&&fstat(*fd,&s)==0?s.st_size:0;}size_t fileSize()const{return fileSize64();}size_t size()const{return fileSize();}int available()const{return fileSize()>pos?fileSize()-pos:0;}
 uint32_t modificationTime()const{struct stat s{};return fd&&fstat(*fd,&s)==0?s.st_mtime:0;}uint32_t creationTime()const{return modificationTime();}
 int read(void*b,size_t n){if(!fd)return -1;auto got=pread(*fd,b,n,pos);if(got>0)pos+=got;return got;}
 size_t write(uint8_t b)override{return write(&b,1);}size_t write(const uint8_t*b,size_t n)override{if(!fd)return 0;auto got=pwrite(*fd,b,n,pos);if(got>0)pos+=got;return got<0?0:got;}size_t write(const void*b,size_t n){return write(static_cast<const uint8_t*>(b),n);}bool sync(){return fd&&fsync(*fd)==0;}
};
class HalStorage{
public:
 static HalStorage&getInstance(){static HalStorage s;return s;}
 bool exists(const char*p){return std::filesystem::exists(disk::path(p));}
 bool mkdir(const char*p){std::error_code e;std::filesystem::create_directories(disk::path(p),e);return !e;}
 HalFile open(const char*p,int flags=O_RDONLY){return HalFile(disk::path(p),flags);}
 bool openFileForRead(const char*,const char*p,HalFile&f){f=open(p);return bool(f);}bool openFileForRead(const char*m,const std::string&p,HalFile&f){return openFileForRead(m,p.c_str(),f);}
 bool openFileForWrite(const char*,const char*p,HalFile&f){f=open(p,O_RDWR|O_CREAT|O_TRUNC);return bool(f);}bool openFileForWrite(const char*m,const std::string&p,HalFile&f){return openFileForWrite(m,p.c_str(),f);}
 bool remove(const char*p){std::error_code e;return std::filesystem::remove(disk::path(p),e)&&!e;}bool removeDir(const char*p){std::error_code e;std::filesystem::remove_all(disk::path(p),e);return !e;}
 bool rename(const char*a,const char*b){if(exists(b))return false;std::error_code e;std::filesystem::rename(disk::path(a),disk::path(b),e);return !e;}
};
#define Storage HalStorage::getInstance()
